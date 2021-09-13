#include <stdio.h>
#include "tev.h"

void init_script(void);

static tev_handle_t tev = NULL;

int main(int argc, char const *argv[])
{
    tev = tev_create_ctx();

    /* user code */
    init_script();

    tev_main_loop(tev);
    tev_free_ctx(tev);
    return 0;
}

static tev_timeout_handle_t test_timer = NULL;

void periodic_print_hello(void* ctx)
{
    printf("hello\n");
    test_timer = tev_set_timeout(tev,periodic_print_hello,NULL,1000);
}

void cancel_print_hello(void* ctx)
{
    tev_clear_timeout(tev,test_timer);
}

void init_script(void)
{
    periodic_print_hello(NULL);
    tev_set_timeout(tev,cancel_print_hello,NULL,5000);
}