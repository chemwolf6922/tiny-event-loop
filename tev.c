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
    void(*read_handler)(void* ctx);
    void* read_ctx;
    void(*write_handler)(void* ctx);
    void* write_ctx;
} tev_fd_handler_t;

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

    return (tev_handle_t)tev;
error:
    if(tev)
    {
        if(tev->epollfd != -1)
            close(tev->epollfd);
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
            {
                if((events[i].events & EPOLLIN) && fd_handler->read_handler)
                    fd_handler->read_handler(fd_handler->read_ctx);
                if((events[i].events & EPOLLOUT) && fd_handler->write_handler)
                    fd_handler->write_handler(fd_handler->write_ctx);
            }
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

static int tev_set_read_write_handler(tev_handle_t handle, int fd, void (*handler)(void* ctx), void* ctx, bool is_read)
{
    if(!handle)
        return -1;
    tev_t *tev = (tev_t *)handle;
    tev_fd_handler_t *fd_handler = map_get(tev->fd_handlers,&fd,sizeof(fd));
    /* create fd_handler if none */
    if(fd_handler == NULL)
    {
        fd_handler = malloc(sizeof(tev_fd_handler_t));
        if(!fd_handler)
            return -1;
        memset(fd_handler,0,sizeof(tev_fd_handler_t));
        fd_handler->fd = fd;
        fd_handler->read_handler = NULL;
        fd_handler->read_ctx = NULL;
        fd_handler->write_handler = NULL;
        fd_handler->write_ctx = NULL;
        if(map_add(tev->fd_handlers,&fd,sizeof(fd),fd_handler)==NULL)
        {
            free(fd_handler);
            return -1;
        }
    }
    /* adjust the content of fd_handler */
    bool read_handler_existed = fd_handler->read_handler != NULL;
    bool write_handler_existed = fd_handler->write_handler != NULL;
    if(is_read)
    {
        fd_handler->read_handler = handler;
        fd_handler->read_ctx = ctx;
    }
    else
    {
        fd_handler->write_handler = handler;
        fd_handler->write_ctx = ctx;
    }
    /* change epoll settings */
    if((fd_handler->read_handler == NULL) && (fd_handler->write_handler == NULL))
    {
        /* remove from epoll and map */
        epoll_ctl(tev->epollfd,EPOLL_CTL_DEL,fd,NULL);
        map_remove(tev->fd_handlers,&fd,sizeof(fd));
        free(fd_handler);
    }
    else if((!read_handler_existed) && (!write_handler_existed))
    {
        /* add to epoll */
        struct epoll_event ev;
        memset(&ev,0,sizeof(ev));
        if(fd_handler->read_handler != NULL)
            ev.events |= EPOLLIN;
        if(fd_handler->write_handler != NULL)
            ev.events |= EPOLLOUT;
        ev.data.ptr = fd_handler;
        if(epoll_ctl(tev->epollfd,EPOLL_CTL_ADD,fd,&ev) < 0)
        {
            map_remove(tev->fd_handlers,&fd,sizeof(fd));
            free(fd_handler);
            return -1;
        }
    }
    else if( (read_handler_existed != (fd_handler->read_handler!=NULL)) || (write_handler_existed != (fd_handler->write_handler!=NULL)) )
    {
        /* adjust epoll if needed */
        struct epoll_event ev;
        memset(&ev,0,sizeof(ev));
        if(fd_handler->read_handler != NULL)
            ev.events |= EPOLLIN;
        if(fd_handler->write_handler != NULL)
            ev.events |= EPOLLOUT;
        ev.data.ptr = fd_handler;
        if(epoll_ctl(tev->epollfd,EPOLL_CTL_MOD,fd,&ev) < 0)
        {
            map_remove(tev->fd_handlers,&fd,sizeof(fd));
            free(fd_handler);
            return -1;
        }
    }
    return 0;
}

int tev_set_read_handler(tev_handle_t handle, int fd, void (*handler)(void *ctx), void *ctx)
{
    return tev_set_read_write_handler(handle,fd,handler,ctx,true);
}

int tev_set_write_handler(tev_handle_t handle, int fd, void (*handler)(void* ctx), void* ctx)
{
    return tev_set_read_write_handler(handle,fd,handler,ctx,false);
}
