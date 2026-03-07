#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @warning
 * As an event loop, this lib is not, will not be and should not be thread safe.
 * Use read handlers to inject events into the event loop.
*/

/* Flow control */

typedef void *tev_handle_t;

/**
 * @brief Create a new event loop.
 * 
 * @return tev_handle_t, NULL if failed. 
 */
tev_handle_t tev_create_ctx(void);

/**
 * @brief Run the event loop. This function will exit if there is no timer or fd to handle.
 * @note How to stop the event loop? Just clear all handlers and timers.
 * 
 * @param tev event loop handle.
 */
void tev_main_loop(tev_handle_t tev);

/**
 * @brief Free the event loop.
 * @warning This MUST be called after the event loop exited. NEVER call this inside the event loop.
 * 
 * @param tev 
 */
void tev_free_ctx(tev_handle_t tev);

/* Timeout */

typedef void *tev_timeout_handle_t;

/**
 * @brief Set a timeout.
 * 
 * @param tev event loop handle.
 * @param handler timeout handler.
 * @param ctx timeout handler context.
 * @param timeout_ms timeout in milliseconds.
 * @return tev_timeout_handle_t, NULL if failed.
 */
tev_timeout_handle_t tev_set_timeout(tev_handle_t tev, void (*handler)(void *ctx), void *ctx, int64_t timeout_ms);

/**
 * @brief Clear a timeout.
 * @note This is safe to call on a cleared or expired timeout handle. 0 will be returned in this case.
 * 
 * @param tev event loop handle.
 * @param handle timeout handle to clear.
 * @return int 0 if success, -1 if failed.
 */
int tev_clear_timeout(tev_handle_t tev, tev_timeout_handle_t handle);

/* Fd read handler */

/**
 * @brief Set / clear a read handler for a fd. This will overwrite the existing read handler if any.
 * 
 * @param tev event loop handle.
 * @param fd file descriptor to set the handler for.
 * @param handler read handler, NULL to clear the handler.
 * @param ctx read handler context.
 * @return int 0 if success, -1 if failed.
 */
int tev_set_read_handler(tev_handle_t tev, int fd, void (*handler)(void* ctx), void *ctx);

/* Fd write handler */

/**
 * @brief Set / clear a write handler for a fd. This will overwrite the existing write handler if any.
 * 
 * @param tev event loop handle.
 * @param fd file descriptor to set the handler for.
 * @param handler write handler, NULL to clear the handler.
 * @param ctx write handler context.
 * @return int 0 if success, -1 if failed.
 */
int tev_set_write_handler(tev_handle_t tev, int fd, void (*handler)(void* ctx), void* ctx);

#ifdef __cplusplus
}
#endif
