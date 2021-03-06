#include "tev.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include "cHeap/heap.h"
#include "map/map.h"

/* structs */

typedef struct
{
    int epollfd;
    // used to assist epoll
    map_handle_t fd_handlers;
    // timers is a minimum heap
    heap_handle_t timers;
    // used to fool proof promise
    map_handle_t promises;
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
    void (*on_reject)(void *ctx, char *reason);
    void *ctx;
} tev_promise_t;

/* pre defined functions */
timestamp_t get_now_ms(void);
bool compare_timeout(void* A,void* B);

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
    tev->fd_handlers = map_create();
    if(!tev->fd_handlers)
        goto error;
    // create timer list
    tev->timers = heap_create(compare_timeout);
    if(!tev->timers)
        goto error;
    // create promise list
    tev->promises = map_create();
    if(!tev->promises)
        goto error;

    return (tev_handle_t)tev;
error:
    if(tev)
    {
        if(tev->epollfd != -1)
            close(tev->epollfd);
        if(tev->promises != NULL)
            map_delete(tev->fd_handlers,NULL,NULL);
        if(tev->timers != NULL)
            heap_free(tev->timers,NULL);
        if(tev->fd_handlers != NULL)
            map_delete(tev->fd_handlers,NULL,NULL);
        free(tev);
    }
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
        next_timeout = 0;
        // process due timers
        if(heap_get_length(tev->timers)!=0)
        {
            timestamp_t now = get_now_ms();
            for(;;)
            {
                tev_timeout_t *p_timeout = heap_get(tev->timers);
                if(p_timeout == NULL)
                    break;
                if(p_timeout->target <= now)
                {
                    // heap does not manage values like array do.
                    heap_pop(tev->timers);
                    if(p_timeout->handler != NULL)
                        p_timeout->handler(p_timeout->ctx);
                    free(p_timeout);
                }
                else
                {
                    next_timeout = p_timeout->target - now;
                    break;
                }
            }
        }
        // are there any files to listen to
        if(next_timeout == 0 && map_get_length(tev->fd_handlers)!=0)
        {
            next_timeout = -1;
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

void free_with_ctx(void* ptr,void* ctx)
{
    free(ptr);
}

void tev_free_ctx(tev_handle_t handle)
{
    if(handle == NULL)
    {
        return;
    }
    tev_t *tev = (tev_t *)handle;
    close(tev->epollfd);
    map_delete(tev->promises,free_with_ctx,NULL);
    heap_free(tev->timers,free);
    map_delete(tev->fd_handlers,free_with_ctx,NULL);
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

bool compare_timeout(void* A,void* B)
{
    tev_timeout_t* timeout_A = (tev_timeout_t*)A;
    tev_timeout_t* timeout_B = (tev_timeout_t*)B;
    return timeout_A->target > timeout_B->target;
}

tev_timeout_handle_t tev_set_timeout(tev_handle_t handle, void (*handler)(void *ctx), void *ctx, int64_t timeout_ms)
{
    if(handle == NULL)
        return NULL;
    tev_t *tev = (tev_t *)handle;
    timestamp_t target = get_now_ms() + timeout_ms;
    tev_timeout_t* new_timeout = malloc(sizeof(tev_timeout_t));
    if(!new_timeout)
        return NULL;
    new_timeout->target = target;
    new_timeout->ctx = ctx;
    new_timeout->handler = handler;
    bool add_result = heap_add(tev->timers,new_timeout);
    if(!add_result)
    {
        free(new_timeout);
        return NULL;
    }
    return (tev_timeout_handle_t)new_timeout;
}

bool match_by_data_ptr(void* data, void* arg)
{
    return data == arg;
}

int tev_clear_timeout(tev_handle_t tev_handle, tev_timeout_handle_t handle)
{
    if(tev_handle == NULL)
        return -1;
    tev_t * tev = (tev_t *)tev_handle;
    if(heap_delete(tev->timers,handle))
        free(handle);
    return 0;
}

/* Fd read handler */

bool match_handler_by_fd(void* data, void* arg)
{
    int fd = *(int*)arg;
    tev_fd_handler_t *fd_handler = (tev_fd_handler_t *)data;
    return fd_handler->fd == fd;
}

int tev_set_read_handler(tev_handle_t handle, int fd, void (*handler)(void *ctx), void *ctx)
{
    if(!handle)
        return -1;
    tev_t *tev = (tev_t *)handle;
    tev_fd_handler_t *fd_handler = map_get(tev->fd_handlers,&fd,sizeof(fd));
    if(fd_handler == NULL)
    {
        // new fd handler
        if(handler != NULL)
        {
            // add
            fd_handler = malloc(sizeof(*fd_handler));
            if(!fd_handler)
                return -1;
            if(map_add(tev->fd_handlers,&fd,sizeof(fd),fd_handler)==NULL)
            {
                free(fd_handler);
                return -1;
            }
            fd_handler->fd = fd;
            fd_handler->ctx = ctx;
            fd_handler->handler = handler;
            struct epoll_event ev;
            ev.events = EPOLLIN;
            ev.data.ptr = fd_handler;
            if(epoll_ctl(tev->epollfd,EPOLL_CTL_ADD,fd,&ev)<0)
            {
                map_remove(tev->fd_handlers,&fd,sizeof(fd));
                free(fd_handler);
                return -1;
            }
        }
        else
        {
            return -1;
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
        }
        else
        {
            // remove
            epoll_ctl(tev->epollfd,EPOLL_CTL_DEL,fd,NULL);
            map_remove(tev->fd_handlers,&fd,sizeof(fd));
            free(fd_handler);
        }
    }
    return 0;
}

/* Promise */

tev_promise_handle_t tev_new_promise(tev_handle_t handle, void (*then)(void *ctx, void *arg), void (*on_reject)(void *ctx, char *reason), void *ctx)
{
    if(!handle)
        return NULL;
    tev_t *tev = (tev_t *)handle;
    tev_promise_t *promise = malloc(sizeof(tev_promise_t));
    if(!promise)
        return NULL;
    if(!map_add(tev->promises,&promise,sizeof(promise),promise))
    {
        free(promise);
        return NULL;
    }
    promise->ctx = ctx;
    promise->then = then;
    promise->on_reject = on_reject;
    return (tev_promise_handle_t)promise;
}

int tev_resolve_promise(tev_handle_t handle, tev_promise_handle_t promise_handle, void *arg)
{
    if(!handle)
        return -1;
    tev_t *tev = (tev_t *)handle;
    tev_promise_t *promise = (tev_promise_t *)promise_handle;
    if(map_remove(tev->promises,&promise,sizeof(promise))!=NULL)
    {
        if(promise->then != NULL)
            promise->then(promise->ctx,arg);
        free(promise);
        return 0;
    }
    return -1;
}

int tev_reject_promise(tev_handle_t handle, tev_promise_handle_t promise_handle, void *reason)
{
    if(!handle)
        return -1;
    tev_t *tev = (tev_t *)handle;
    tev_promise_t *promise = (tev_promise_t *)promise_handle;
    if(map_remove(tev->promises,&promise,sizeof(promise))!=NULL)
    {
        if(promise->on_reject != NULL)
            promise->on_reject(promise->ctx,reason);
        free(promise);
        return 0;
    }
    return -1;
}
