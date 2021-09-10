#include "tev.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/epoll.h>

/* Flow control */

typedef struct
{

} tev_t;

tev_handle_t tev_create_ctx(void)
{

}

void tev_main_loop(tev_handle_t tev)
{

}

void tev_free_ctx(tev_handle_t tev)
{

}

/* Timeout */
typedef struct
{

} tev_timeout_t;

tev_timeout_handle_t tev_set_timeout(tev_handle_t tev, void (*handler)(void *ctx), void *ctx, int64_t timeout_ms);
bool tev_clear_timeout(tev_handle_t tev, tev_timeout_handle_t handle);

/* Fd read handler */

bool tev_set_read_handler(tev_handle_t tev, int fd, void (*handler)(void *ctx), void *ctx);

/* Promise */
typedef struct
{
    /* data */
} tev_promise_t;

tev_promise_handle_t tev_new_promise(tev_handle_t tev, void (*then)(void *ctx, void *arg), void (*on_reject)(void *ctx, void *reason), void *ctx);
bool tev_resolve_promise(tev_handle_t tev, tev_promise_handle_t promise, void *arg);
bool tev_reject_promise(tev_handle_t tev, tev_promise_handle_t promise, void *reason);
