#include "tev.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include "cArray/array.h"

/* structs */

typedef struct
{
    int epollfd;
    // used to assist epoll
    array_handle_t fd_handlers;
    // timers is a sorted array
    array_handle_t timers;
    // used to fool proof promise
    array_handle_t promises;
} tev_t;

typedef int64_t timestamp_t;
typedef struct
{
    timestamp_t target;
    void(*handler)(void* ctx);
    void *ctx;
} tev_timeout_t;

typedef struct
{
    int fd;
    void(*handler)(void* ctx);
    void* ctx;
} tev_fd_handler_t;

typedef struct
{
    void (*then)(void *ctx, void *arg);
    void (*on_reject)(void *ctx, void *reason);
    void *ctx;
} tev_promise_t;

/* pre defined functions */
timestamp_t get_now_ms(void);

/* Flow control */

tev_handle_t tev_create_ctx(void)
{
    tev_t *tev = malloc(sizeof(tev_t));
    if(!tev)
        goto error;
    memset(tev,0,sizeof(*tev));
    tev->epollfd = -1;
    tev->timers = NULL;
    tev->fd_handlers = NULL;
    // create epoll fd
    tev->epollfd = epoll_create1(0);
    if(tev->epollfd == -1)
        goto error;
    // create fd handler list
    if(array_create(&tev->fd_handlers,sizeof(tev_fd_handler_t))<0)
        goto error;
    // create timer list
    if(array_create(&tev->timers,sizeof(tev_timeout_t))<0)
        goto error;
    // create promise list
    if(array_create(&tev->promises,sizeof(tev_promise_t))<0)
        goto error;

    return (tev_handle_t)tev;
error:
    if(tev->epollfd != -1)
        close(tev->epollfd);
    if(tev->promises != NULL)
        array_delete(tev->promises);
    if(tev->timers != NULL)
        array_delete(tev->timers);
    if(tev->fd_handlers != NULL)
        array_delete(tev->fd_handlers);
    if(tev != NULL)
        free(tev);
    return NULL;
}

#define MAX_EPOLL_EVENTS 10
void tev_main_loop(tev_handle_t handle)
{
    if(handle == NULL)
    {
        return;
    }
    tev_t *tev = (tev_t *)handle;
    struct epoll_event events[MAX_EPOLL_EVENTS];
    int next_timeout;
    for(;;)
    {
        // are there any files to listen to
        next_timeout = array_length(tev->fd_handlers)!=0?-1:0;
        // process due timers
        if(array_length(tev->timers)!=0)
        {
            timestamp_t now = get_now_ms();
            for(;;)
            {
                tev_timeout_t *p_timeout = array_get(tev->timers,0);
                if(p_timeout == NULL)
                    break;
                if(p_timeout->target <= now)
                {
                    tev_timeout_t timeout;
                    array_shift(tev->timers, &timeout);
                    if(timeout.handler != NULL)
                        timeout.handler(timeout.ctx);
                }
                else
                {
                    next_timeout = p_timeout->target - now;
                    break;
                }
            }
        }
        // wait for timers & fds
        if(next_timeout == 0)
        {
            // neither timer nor fds exist
            break;
        }
        int nfds = epoll_wait(tev->epollfd,events,MAX_EPOLL_EVENTS,next_timeout);
        // handle files
        for(int i=0;i<nfds;i++)
        {
            tev_fd_handler_t *fd_handler = (tev_fd_handler_t*)events[i].data.ptr;
            if(fd_handler != NULL)
                fd_handler->handler(fd_handler->ctx);
        }
    }
}

void tev_free_ctx(tev_handle_t handle)
{
    if(handle == NULL)
    {
        return;
    }
    tev_t *tev = (tev_t *)handle;
    close(tev->epollfd);
    array_delete(tev->promises);
    array_delete(tev->timers);
    array_delete(tev->fd_handlers);
    free(tev);
}

/* Timeout */

timestamp_t get_now_ms(void)
{
    struct timeval now;
    gettimeofday(&now,NULL);
    timestamp_t now_ts = (timestamp_t)now.tv_sec * 1000 + (timestamp_t)now.tv_usec/1000;
    return now_ts;
}

tev_timeout_handle_t tev_set_timeout(tev_handle_t handle, void (*handler)(void *ctx), void *ctx, int64_t timeout_ms)
{
    if(handle == NULL)
        return NULL;
    tev_t *tev = (tev_t *)handle;
    timestamp_t target = get_now_ms() + timeout_ms;
    int i = 0;
    tev_timeout_t* p_timeout = NULL;
    array_forEach(tev->timers,p_timeout)
    {
        if(p_timeout->target < target)
        {
            i++;
        }
        else
        {
            break;
        }
    }
    tev_timeout_t* new_timeout = array_insert(tev->timers,NULL,i);
    if(!new_timeout)
        return NULL;
    new_timeout->target = target;
    new_timeout->ctx = ctx;
    new_timeout->handler = handler;
    return (tev_timeout_handle_t)new_timeout;
}

bool match_by_data_ptr(void* data, void* arg)
{
    return data == arg;
}

bool tev_clear_timeout(tev_handle_t tev_handle, tev_timeout_handle_t handle)
{
    if(tev_handle == NULL)
        return false;
    tev_t * tev = (tev_t *)tev_handle;
    array_delete_match(tev->timers,match_by_data_ptr,handle);
    return true;
}

/* Fd read handler */

bool match_handler_by_fd(void* data, void* arg)
{
    int fd = *(int*)arg;
    tev_fd_handler_t *fd_handler = (tev_fd_handler_t *)data;
    return fd_handler->fd == fd;
}

bool tev_set_read_handler(tev_handle_t handle, int fd, void (*handler)(void *ctx), void *ctx)
{
    if(!handle)
        return false;
    tev_t *tev = (tev_t *)handle;
    tev_fd_handler_t *fd_handler = array_find(tev->fd_handlers,match_handler_by_fd,&fd);
    if(fd_handler == NULL)
    {
        // new fd handler
        if(handler != NULL)
        {
            // add
            fd_handler = array_push(tev->fd_handlers,NULL);
            fd_handler->fd = fd;
            fd_handler->ctx = ctx;
            fd_handler->handler = handler;
            struct epoll_event ev;
            ev.events = EPOLLIN;
            ev.data.ptr = fd_handler;
            return epoll_ctl(tev->epollfd,EPOLL_CTL_ADD,fd,&ev) == 0;
        }
        else
        {
            return false;
        }
    }
    else
    {
        // existing fd handler
        if(handler != NULL)
        {
            // update
            fd_handler->ctx = ctx;
            fd_handler->handler = handler;
            // no need to update epoll
            return true;
        }
        else
        {
            // remove
            return epoll_ctl(tev->epollfd,EPOLL_CTL_DEL,fd,NULL) == 0;
        }
    }
    return false;
}

/* Promise */

tev_promise_handle_t tev_new_promise(tev_handle_t handle, void (*then)(void *ctx, void *arg), void (*on_reject)(void *ctx, void *reason), void *ctx)
{
    if(!handle)
        return NULL;
    tev_t *tev = (tev_t *)handle;
    tev_promise_t *promise = array_push(tev->promises,NULL);
    if(!promise)
        return NULL;
    promise->ctx = ctx;
    promise->then = then;
    promise->on_reject = on_reject;
    return (tev_promise_handle_t)promise;
}

bool tev_resolve_promise(tev_handle_t handle, tev_promise_handle_t promise_handle, void *arg)
{
    if(!handle)
        return false;
    tev_t *tev = (tev_t *)handle;
    tev_promise_t *promise = (tev_promise_t *)promise_handle;
    if(array_find(tev->promises,match_by_data_ptr,promise)!=NULL)
    {
        if(promise->then != NULL)
            promise->then(promise->ctx,arg);
        array_delete_match(tev->promises,match_by_data_ptr,promise);
        return true;
    }
    return false;
}

bool tev_reject_promise(tev_handle_t handle, tev_promise_handle_t promise_handle, void *reason)
{
    if(!handle)
        return false;
    tev_t *tev = (tev_t *)handle;
    tev_promise_t *promise = (tev_promise_t *)promise_handle;
    if(array_find(tev->promises,match_by_data_ptr,promise)!=NULL)
    {
        if(promise->on_reject != NULL)
            promise->on_reject(promise->ctx,reason);
        array_delete_match(tev->promises,match_by_data_ptr,promise);
        return true;
    }
    return false;
}
