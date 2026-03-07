#ifndef TEV_TEST_COMMON_H
#define TEV_TEST_COMMON_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "tev.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_TRUE(cond, msg) do { \
    if ((cond)) { \
        tests_passed++; \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s (at %s:%d)\n", (msg), __FILE__, __LINE__); \
    } \
} while(0)

#define ASSERT_EQ(actual, expected, msg) do { \
    if ((actual) == (expected)) { \
        tests_passed++; \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s (expected %d, got %d at %s:%d)\n", \
               (msg), (int)(expected), (int)(actual), __FILE__, __LINE__); \
    } \
} while(0)

#define ASSERT_NOT_NULL(ptr, msg) do { \
    if ((ptr) != NULL) { \
        tests_passed++; \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s (expected non-NULL at %s:%d)\n", (msg), __FILE__, __LINE__); \
    } \
} while(0)

#define ASSERT_NULL(ptr, msg) do { \
    if ((ptr) == NULL) { \
        tests_passed++; \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s (expected NULL at %s:%d)\n", (msg), __FILE__, __LINE__); \
    } \
} while(0)

#define ASSERT_STR_EQ(actual, expected, msg) do { \
    if (strcmp((actual), (expected)) == 0) { \
        tests_passed++; \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s (expected \"%s\", got \"%s\" at %s:%d)\n", \
               (msg), (expected), (actual), __FILE__, __LINE__); \
    } \
} while(0)

#define PRINT_RESULTS() do { \
    printf("\n  Results: %d passed, %d failed\n", tests_passed, tests_failed); \
} while(0)

/**
 * @brief Create a non-blocking pipe. Aborts on failure.
 */
static inline void create_pipe(int fds[2])
{
#ifdef __APPLE__
    if (pipe(fds) != 0)
        abort();
    if (fcntl(fds[0], F_SETFD, fcntl(fds[0], F_GETFD) | FD_CLOEXEC) == -1)
        abort();
    if (fcntl(fds[0], F_SETFL, fcntl(fds[0], F_GETFL) | O_NONBLOCK) == -1)
        abort();
    if (fcntl(fds[1], F_SETFD, fcntl(fds[1], F_GETFD) | FD_CLOEXEC) == -1)
        abort();
    if (fcntl(fds[1], F_SETFL, fcntl(fds[1], F_GETFL) | O_NONBLOCK) == -1)
        abort();
#else
    if (pipe2(fds, O_CLOEXEC | O_NONBLOCK) != 0)
        abort();
#endif
}

#endif /* TEV_TEST_COMMON_H */
