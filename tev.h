#ifndef __TINY_EVENT_LOOP_H
#define __TINY_EVENT_LOOP_H

#include <stdint.h>
#include <stdbool.h>


#define TINY_EVENT_LOOP_VERSION "v1.3.3"
/**
 * Notice: 
 * As an event loop, this lib is not, will not be and should not be thread safe.
 * Use fifos & read handlers to inject events into the event loop.
*/

/* Flow control */

typedef void *tev_handle_t;

tev_handle_t tev_create_ctx(void);
void tev_main_loop(tev_handle_t tev);
void tev_free_ctx(tev_handle_t tev);

/* Timeout */

typedef void *tev_timeout_handle_t;

tev_timeout_handle_t tev_set_timeout(tev_handle_t tev, void (*handler)(void *ctx), void *ctx, int64_t timeout_ms);
int tev_clear_timeout(tev_handle_t tev, tev_timeout_handle_t handle);

/* Fd read handler */

int tev_set_read_handler(tev_handle_t tev, int fd, void (*handler)(void* ctx), void *ctx);

/* Fd write handler */

int tev_set_write_handler(tev_handle_t tev, int fd, void (*handler)(void* ctx), void* ctx);


#endif