#include "test_common.h"

/* ===== Test: create and free context ===== */

static void test_create_free_ctx(void)
{
    printf("test_create_free_ctx\n");

    tev_handle_t ctx = tev_create_ctx();
    ASSERT_NOT_NULL(ctx, "tev_create_ctx returns non-NULL");

    /* free should not crash */
    tev_free_ctx(ctx);

    /* free NULL should not crash */
    tev_free_ctx(NULL);
}

/* ===== Test: main loop exits immediately with no handlers ===== */

static void test_main_loop_exits_empty(void)
{
    printf("test_main_loop_exits_empty\n");

    tev_handle_t ctx = tev_create_ctx();
    ASSERT_NOT_NULL(ctx, "create ctx");

    /* should return immediately since there are no timers or fds */
    tev_main_loop(ctx);

    /* also should not crash with NULL */
    tev_main_loop(NULL);

    tev_free_ctx(ctx);
}

/* ===== Test: single timeout fires ===== */

static int timeout_fire_count;

static void on_timeout_fire(void *ctx)
{
    (void)ctx;
    timeout_fire_count++;
}

static void test_single_timeout(void)
{
    printf("test_single_timeout\n");

    tev_handle_t ctx = tev_create_ctx();
    timeout_fire_count = 0;

    tev_timeout_handle_t h = tev_set_timeout(ctx, on_timeout_fire, NULL, 10);
    ASSERT_NOT_NULL(h, "set_timeout returns handle");

    tev_main_loop(ctx);

    ASSERT_EQ(timeout_fire_count, 1, "timeout fired exactly once");

    tev_free_ctx(ctx);
}

/* ===== Test: multiple timeouts fire in order ===== */

static int timeout_order[4];
static int timeout_order_idx;

static void on_timeout_order(void *ctx)
{
    int id = *(int *)ctx;
    timeout_order[timeout_order_idx++] = id;
}

static void test_timeout_ordering(void)
{
    printf("test_timeout_ordering\n");

    tev_handle_t ctx = tev_create_ctx();
    timeout_order_idx = 0;
    int ids[] = {1, 2, 3};

    /* schedule in reverse delay order — 3 should still fire last */
    tev_set_timeout(ctx, on_timeout_order, &ids[2], 30);
    tev_set_timeout(ctx, on_timeout_order, &ids[0], 10);
    tev_set_timeout(ctx, on_timeout_order, &ids[1], 20);

    tev_main_loop(ctx);

    ASSERT_EQ(timeout_order_idx, 3, "all three timeouts fired");
    ASSERT_EQ(timeout_order[0], 1, "first timeout is id 1");
    ASSERT_EQ(timeout_order[1], 2, "second timeout is id 2");
    ASSERT_EQ(timeout_order[2], 3, "third timeout is id 3");

    tev_free_ctx(ctx);
}

/* ===== Test: clear timeout before it fires ===== */

static int cleared_timeout_fired;

static void on_cleared_timeout(void *ctx)
{
    (void)ctx;
    cleared_timeout_fired = 1;
}

static void clear_the_timeout(void *ctx)
{
    tev_handle_t tev = ((void **)ctx)[0];
    tev_timeout_handle_t *ph = ((void **)ctx)[1];
    int ret = tev_clear_timeout(tev, *ph);
    ASSERT_EQ(ret, 0, "clear_timeout returns 0");
}

static void test_clear_timeout(void)
{
    printf("test_clear_timeout\n");

    tev_handle_t ctx = tev_create_ctx();
    cleared_timeout_fired = 0;

    /* schedule a timeout at 200ms */
    tev_timeout_handle_t h = tev_set_timeout(ctx, on_cleared_timeout, NULL, 200);

    /* schedule another timeout at 10ms that clears the first */
    void *clear_ctx[2] = { ctx, &h };
    tev_set_timeout(ctx, clear_the_timeout, clear_ctx, 10);

    tev_main_loop(ctx);

    ASSERT_EQ(cleared_timeout_fired, 0, "cleared timeout did not fire");

    tev_free_ctx(ctx);
}

/* ===== Test: clear already-expired / double-clear timeout ===== */

static void test_clear_expired_timeout(void)
{
    printf("test_clear_expired_timeout\n");

    tev_handle_t ctx = tev_create_ctx();

    tev_timeout_handle_t h = tev_set_timeout(ctx, on_timeout_fire, NULL, 0);
    ASSERT_NOT_NULL(h, "set_timeout with 0ms");

    tev_main_loop(ctx);

    /* clear after already expired — should return 0 */
    int ret = tev_clear_timeout(ctx, h);
    ASSERT_EQ(ret, 0, "clear expired timeout returns 0");

    /* double clear */
    ret = tev_clear_timeout(ctx, h);
    ASSERT_EQ(ret, 0, "double clear returns 0");

    /* clear with NULL handle */
    ret = tev_clear_timeout(ctx, NULL);
    ASSERT_EQ(ret, 0, "clear NULL handle returns 0");

    /* clear on NULL ctx */
    ret = tev_clear_timeout(NULL, h);
    ASSERT_EQ(ret, -1, "clear on NULL ctx returns -1");

    tev_free_ctx(ctx);
}

/* ===== Test: set_timeout with NULL ctx ===== */

static void test_set_timeout_null_ctx(void)
{
    printf("test_set_timeout_null_ctx\n");

    tev_timeout_handle_t h = tev_set_timeout(NULL, on_timeout_fire, NULL, 10);
    ASSERT_NULL(h, "set_timeout on NULL ctx returns NULL");
}

/* ===== Test: read handler ===== */

typedef struct {
    tev_handle_t tev;
    int read_fd;
    int write_fd;
    char received[64];
    int received_len;
} read_test_ctx_t;

static void on_read_ready(void *ctx)
{
    read_test_ctx_t *t = (read_test_ctx_t *)ctx;
    ssize_t n = read(t->read_fd, t->received + t->received_len,
                     sizeof(t->received) - (size_t)t->received_len - 1);
    if (n > 0)
        t->received_len += (int)n;
    t->received[t->received_len] = '\0';

    /* remove read handler after receiving data */
    tev_set_read_handler(t->tev, t->read_fd, NULL, NULL);
}

static void on_write_test_data(void *ctx)
{
    read_test_ctx_t *t = (read_test_ctx_t *)ctx;
    const char *msg = "hello";
    ssize_t n = write(t->write_fd, msg, strlen(msg) + 1);
    (void)n;
}

static void test_read_handler(void)
{
    printf("test_read_handler\n");

    tev_handle_t ctx = tev_create_ctx();
    int fds[2];
    create_pipe(fds);

    read_test_ctx_t tctx;
    memset(&tctx, 0, sizeof(tctx));
    tctx.tev = ctx;
    tctx.read_fd = fds[0];
    tctx.write_fd = fds[1];

    int ret = tev_set_read_handler(ctx, fds[0], on_read_ready, &tctx);
    ASSERT_EQ(ret, 0, "set_read_handler returns 0");

    /* write data after a small delay */
    tev_set_timeout(ctx, on_write_test_data, &tctx, 10);

    tev_main_loop(ctx);

    ASSERT_STR_EQ(tctx.received, "hello", "received correct data via read handler");

    close(fds[0]);
    close(fds[1]);
    tev_free_ctx(ctx);
}

/* ===== Test: write handler ===== */

typedef struct {
    tev_handle_t tev;
    int write_fd;
    int write_handler_called;
} write_test_ctx_t;

static void on_write_ready(void *ctx)
{
    write_test_ctx_t *t = (write_test_ctx_t *)ctx;
    t->write_handler_called++;
    /* clear write handler after first call */
    tev_set_write_handler(t->tev, t->write_fd, NULL, NULL);
}

static void test_write_handler(void)
{
    printf("test_write_handler\n");

    tev_handle_t ctx = tev_create_ctx();
    int fds[2];
    create_pipe(fds);

    write_test_ctx_t tctx = { .tev = ctx, .write_fd = fds[1], .write_handler_called = 0 };

    int ret = tev_set_write_handler(ctx, fds[1], on_write_ready, &tctx);
    ASSERT_EQ(ret, 0, "set_write_handler returns 0");

    tev_main_loop(ctx);

    ASSERT_EQ(tctx.write_handler_called, 1, "write handler called exactly once");

    close(fds[0]);
    close(fds[1]);
    tev_free_ctx(ctx);
}

/* ===== Test: read + write on same fd ===== */

typedef struct {
    tev_handle_t tev;
    int read_fd;
    int write_fd;
    int read_called;
    int write_called;
    char read_buf[64];
} rw_test_ctx_t;

static void on_rw_write_ready(void *ctx)
{
    rw_test_ctx_t *t = (rw_test_ctx_t *)ctx;
    t->write_called++;
    const char *msg = "data";
    ssize_t n = write(t->write_fd, msg, strlen(msg) + 1);
    (void)n;
    /* remove write handler after writing */
    tev_set_write_handler(t->tev, t->write_fd, NULL, NULL);
}

static void on_rw_read_ready(void *ctx)
{
    rw_test_ctx_t *t = (rw_test_ctx_t *)ctx;
    t->read_called++;
    ssize_t n = read(t->read_fd, t->read_buf, sizeof(t->read_buf) - 1);
    if (n > 0)
        t->read_buf[n] = '\0';
    /* remove read handler */
    tev_set_read_handler(t->tev, t->read_fd, NULL, NULL);
}

static void test_read_write_same_pipe(void)
{
    printf("test_read_write_same_pipe\n");

    tev_handle_t ctx = tev_create_ctx();
    int fds[2];
    create_pipe(fds);

    rw_test_ctx_t tctx;
    memset(&tctx, 0, sizeof(tctx));
    tctx.tev = ctx;
    tctx.read_fd = fds[0];
    tctx.write_fd = fds[1];

    tev_set_read_handler(ctx, fds[0], on_rw_read_ready, &tctx);
    tev_set_write_handler(ctx, fds[1], on_rw_write_ready, &tctx);

    tev_main_loop(ctx);

    ASSERT_EQ(tctx.write_called, 1, "write handler called");
    ASSERT_EQ(tctx.read_called, 1, "read handler called");
    ASSERT_STR_EQ(tctx.read_buf, "data", "read correct data");

    close(fds[0]);
    close(fds[1]);
    tev_free_ctx(ctx);
}

/* ===== Test: overwrite read handler ===== */

static int handler_a_count;
static int handler_b_count;

static void handler_a(void *ctx)
{
    (void)ctx;
    handler_a_count++;
}

static void handler_b(void *ctx)
{
    rw_test_ctx_t *t = (rw_test_ctx_t *)ctx;
    handler_b_count++;
    /* consume data */
    char buf[64];
    ssize_t n = read(t->read_fd, buf, sizeof(buf));
    (void)n;
    tev_set_read_handler(t->tev, t->read_fd, NULL, NULL);
}

static void write_for_overwrite_test(void *ctx)
{
    rw_test_ctx_t *t = (rw_test_ctx_t *)ctx;
    const char *msg = "x";
    ssize_t n = write(t->write_fd, msg, 1);
    (void)n;
}

static void test_overwrite_read_handler(void)
{
    printf("test_overwrite_read_handler\n");

    tev_handle_t ctx = tev_create_ctx();
    int fds[2];
    create_pipe(fds);

    rw_test_ctx_t tctx;
    memset(&tctx, 0, sizeof(tctx));
    tctx.tev = ctx;
    tctx.read_fd = fds[0];
    tctx.write_fd = fds[1];

    handler_a_count = 0;
    handler_b_count = 0;

    /* set handler A, then immediately overwrite with handler B */
    tev_set_read_handler(ctx, fds[0], handler_a, &tctx);
    tev_set_read_handler(ctx, fds[0], handler_b, &tctx);

    /* write data so the read handler fires */
    tev_set_timeout(ctx, write_for_overwrite_test, &tctx, 10);

    tev_main_loop(ctx);

    ASSERT_EQ(handler_a_count, 0, "handler A was not called");
    ASSERT_TRUE(handler_b_count >= 1, "handler B was called");

    close(fds[0]);
    close(fds[1]);
    tev_free_ctx(ctx);
}

/* ===== Test: set_read_handler / set_write_handler with NULL tev ===== */

static void test_fd_handler_null_ctx(void)
{
    printf("test_fd_handler_null_ctx\n");

    int ret = tev_set_read_handler(NULL, 0, handler_a, NULL);
    ASSERT_EQ(ret, -1, "set_read_handler on NULL ctx returns -1");

    ret = tev_set_write_handler(NULL, 0, handler_a, NULL);
    ASSERT_EQ(ret, -1, "set_write_handler on NULL ctx returns -1");
}

/* ===== Test: periodic timer with cancellation ===== */

static tev_handle_t periodic_tev;
static tev_timeout_handle_t periodic_handle;
static int periodic_count;

static void periodic_callback(void *ctx)
{
    (void)ctx;
    periodic_count++;
    periodic_handle = tev_set_timeout(periodic_tev, periodic_callback, NULL, 10);
}

static void cancel_periodic(void *ctx)
{
    (void)ctx;
    tev_clear_timeout(periodic_tev, periodic_handle);
}

static void test_periodic_timer(void)
{
    printf("test_periodic_timer\n");

    periodic_tev = tev_create_ctx();
    periodic_count = 0;
    periodic_handle = NULL;

    /* start periodic timer */
    periodic_callback(NULL);

    /* cancel after 55ms — expect ~5 fires (initial + 4 periodic at 10ms each) */
    tev_set_timeout(periodic_tev, cancel_periodic, NULL, 55);

    tev_main_loop(periodic_tev);

    ASSERT_TRUE(periodic_count >= 3, "periodic timer fired at least 3 times");
    ASSERT_TRUE(periodic_count <= 8, "periodic timer fired at most 8 times");

    tev_free_ctx(periodic_tev);
}

/* ===== Test: remove fd handler inside read handler callback ===== */

typedef struct {
    tev_handle_t tev;
    int read_fd;
    int write_fd;
    int callback_count;
} remove_in_handler_ctx_t;

static void remove_self_read_handler(void *ctx)
{
    remove_in_handler_ctx_t *t = (remove_in_handler_ctx_t *)ctx;
    t->callback_count++;
    char buf[64];
    ssize_t n = read(t->read_fd, buf, sizeof(buf));
    (void)n;
    /* remove ourselves inside the callback — this must not crash */
    tev_set_read_handler(t->tev, t->read_fd, NULL, NULL);
}

static void write_for_remove_test(void *ctx)
{
    remove_in_handler_ctx_t *t = (remove_in_handler_ctx_t *)ctx;
    const char *msg = "z";
    ssize_t n = write(t->write_fd, msg, 1);
    (void)n;
}

static void test_remove_handler_in_callback(void)
{
    printf("test_remove_handler_in_callback\n");

    tev_handle_t ctx = tev_create_ctx();
    int fds[2];
    create_pipe(fds);

    remove_in_handler_ctx_t tctx = {
        .tev = ctx, .read_fd = fds[0], .write_fd = fds[1], .callback_count = 0
    };

    tev_set_read_handler(ctx, fds[0], remove_self_read_handler, &tctx);
    tev_set_timeout(ctx, write_for_remove_test, &tctx, 10);

    tev_main_loop(ctx);

    ASSERT_EQ(tctx.callback_count, 1, "handler called once before self-removal");

    close(fds[0]);
    close(fds[1]);
    tev_free_ctx(ctx);
}

/* ===== Test: zero-delay timeout fires immediately ===== */

static int zero_delay_count;

static void on_zero_delay(void *ctx)
{
    (void)ctx;
    zero_delay_count++;
}

static void test_zero_delay_timeout(void)
{
    printf("test_zero_delay_timeout\n");

    tev_handle_t ctx = tev_create_ctx();
    zero_delay_count = 0;

    tev_set_timeout(ctx, on_zero_delay, NULL, 0);
    tev_set_timeout(ctx, on_zero_delay, NULL, 0);
    tev_set_timeout(ctx, on_zero_delay, NULL, 0);

    tev_main_loop(ctx);

    ASSERT_EQ(zero_delay_count, 3, "all three zero-delay timeouts fired");

    tev_free_ctx(ctx);
}

/* ===== Test: timeout passes correct context ===== */

static void on_timeout_with_ctx(void *ctx)
{
    int *val = (int *)ctx;
    *val = 42;
}

static void test_timeout_context(void)
{
    printf("test_timeout_context\n");

    tev_handle_t ctx = tev_create_ctx();
    int value = 0;

    tev_set_timeout(ctx, on_timeout_with_ctx, &value, 10);

    tev_main_loop(ctx);

    ASSERT_EQ(value, 42, "timeout handler received correct context");

    tev_free_ctx(ctx);
}

/* ===== Test: read handler v2 (receives fd) ===== */

typedef struct {
    tev_handle_t tev;
    int read_fd;
    int write_fd;
    char received[64];
    int received_len;
    int received_fd;
} read_test_v2_ctx_t;

static void on_read_ready_v2(int fd, void *ctx)
{
    read_test_v2_ctx_t *t = (read_test_v2_ctx_t *)ctx;
    t->received_fd = fd;
    ssize_t n = read(fd, t->received + t->received_len,
                     sizeof(t->received) - (size_t)t->received_len - 1);
    if (n > 0)
        t->received_len += (int)n;
    t->received[t->received_len] = '\0';
    tev_set_read_handler2(t->tev, fd, NULL, NULL);
}

static void on_write_test_data_v2(void *ctx)
{
    read_test_v2_ctx_t *t = (read_test_v2_ctx_t *)ctx;
    const char *msg = "world";
    ssize_t n = write(t->write_fd, msg, strlen(msg) + 1);
    (void)n;
}

static void test_read_handler_v2(void)
{
    printf("test_read_handler_v2\n");

    tev_handle_t ctx = tev_create_ctx();
    int fds[2];
    create_pipe(fds);

    read_test_v2_ctx_t tctx;
    memset(&tctx, 0, sizeof(tctx));
    tctx.tev = ctx;
    tctx.read_fd = fds[0];
    tctx.write_fd = fds[1];
    tctx.received_fd = -1;

    int ret = tev_set_read_handler2(ctx, fds[0], on_read_ready_v2, &tctx);
    ASSERT_EQ(ret, 0, "set_read_handler2 returns 0");

    tev_set_timeout(ctx, on_write_test_data_v2, &tctx, 10);

    tev_main_loop(ctx);

    ASSERT_STR_EQ(tctx.received, "world", "received correct data via read handler v2");
    ASSERT_EQ(tctx.received_fd, fds[0], "read handler v2 received correct fd");

    close(fds[0]);
    close(fds[1]);
    tev_free_ctx(ctx);
}

/* ===== Test: write handler v2 (receives fd) ===== */

typedef struct {
    tev_handle_t tev;
    int write_fd;
    int write_handler_called;
    int received_fd;
} write_test_v2_ctx_t;

static void on_write_ready_v2(int fd, void *ctx)
{
    write_test_v2_ctx_t *t = (write_test_v2_ctx_t *)ctx;
    t->write_handler_called++;
    t->received_fd = fd;
    tev_set_write_handler2(t->tev, fd, NULL, NULL);
}

static void test_write_handler_v2(void)
{
    printf("test_write_handler_v2\n");

    tev_handle_t ctx = tev_create_ctx();
    int fds[2];
    create_pipe(fds);

    write_test_v2_ctx_t tctx = {
        .tev = ctx, .write_fd = fds[1], .write_handler_called = 0, .received_fd = -1
    };

    int ret = tev_set_write_handler2(ctx, fds[1], on_write_ready_v2, &tctx);
    ASSERT_EQ(ret, 0, "set_write_handler2 returns 0");

    tev_main_loop(ctx);

    ASSERT_EQ(tctx.write_handler_called, 1, "write handler v2 called exactly once");
    ASSERT_EQ(tctx.received_fd, fds[1], "write handler v2 received correct fd");

    close(fds[0]);
    close(fds[1]);
    tev_free_ctx(ctx);
}

/* ===== Test: read + write v2 on same pipe ===== */

typedef struct {
    tev_handle_t tev;
    int read_fd;
    int write_fd;
    int read_called;
    int write_called;
    char read_buf[64];
} rw_test_v2_ctx_t;

static void on_rw_write_ready_v2(int fd, void *ctx)
{
    rw_test_v2_ctx_t *t = (rw_test_v2_ctx_t *)ctx;
    t->write_called++;
    const char *msg = "v2data";
    ssize_t n = write(fd, msg, strlen(msg) + 1);
    (void)n;
    tev_set_write_handler2(t->tev, fd, NULL, NULL);
}

static void on_rw_read_ready_v2(int fd, void *ctx)
{
    rw_test_v2_ctx_t *t = (rw_test_v2_ctx_t *)ctx;
    t->read_called++;
    ssize_t n = read(fd, t->read_buf, sizeof(t->read_buf) - 1);
    if (n > 0)
        t->read_buf[n] = '\0';
    tev_set_read_handler2(t->tev, fd, NULL, NULL);
}

static void test_read_write_v2_same_pipe(void)
{
    printf("test_read_write_v2_same_pipe\n");

    tev_handle_t ctx = tev_create_ctx();
    int fds[2];
    create_pipe(fds);

    rw_test_v2_ctx_t tctx;
    memset(&tctx, 0, sizeof(tctx));
    tctx.tev = ctx;
    tctx.read_fd = fds[0];
    tctx.write_fd = fds[1];

    tev_set_read_handler2(ctx, fds[0], on_rw_read_ready_v2, &tctx);
    tev_set_write_handler2(ctx, fds[1], on_rw_write_ready_v2, &tctx);

    tev_main_loop(ctx);

    ASSERT_EQ(tctx.write_called, 1, "write handler v2 called");
    ASSERT_EQ(tctx.read_called, 1, "read handler v2 called");
    ASSERT_STR_EQ(tctx.read_buf, "v2data", "read correct data via v2");

    close(fds[0]);
    close(fds[1]);
    tev_free_ctx(ctx);
}

/* ===== Test: set_read_handler2 / set_write_handler2 with NULL tev ===== */

static void dummy_handler_v2(int fd, void *ctx)
{
    (void)fd;
    (void)ctx;
}

static void test_fd_handler_v2_null_ctx(void)
{
    printf("test_fd_handler_v2_null_ctx\n");

    int ret = tev_set_read_handler2(NULL, 0, dummy_handler_v2, NULL);
    ASSERT_EQ(ret, -1, "set_read_handler2 on NULL ctx returns -1");

    ret = tev_set_write_handler2(NULL, 0, dummy_handler_v2, NULL);
    ASSERT_EQ(ret, -1, "set_write_handler2 on NULL ctx returns -1");
}

/* ===== Main ===== */

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    printf("=== tev functional tests ===\n\n");

    test_create_free_ctx();
    test_main_loop_exits_empty();
    test_single_timeout();
    test_timeout_ordering();
    test_clear_timeout();
    test_clear_expired_timeout();
    test_set_timeout_null_ctx();
    test_zero_delay_timeout();
    test_timeout_context();
    test_periodic_timer();
    test_read_handler();
    test_write_handler();
    test_read_write_same_pipe();
    test_overwrite_read_handler();
    test_fd_handler_null_ctx();
    test_remove_handler_in_callback();
    test_read_handler_v2();
    test_write_handler_v2();
    test_read_write_v2_same_pipe();
    test_fd_handler_v2_null_ctx();

    PRINT_RESULTS();

    return tests_failed == 0 ? 0 : 1;
}
