#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include "tev.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

void periodic_print_hello(void* ctx);
void cancel_print_hello(void* ctx);
void read_handler(void* ctx);
void write_some_data(void* ctx);
void clear_read_handler(void* ctx);

static tev_handle_t tev = NULL;

/* errors are not handled for simplicity */
int main(int argc, char const *argv[])
{
    tev = tev_create_ctx();

    /* user init starts */
    int fds[2] = {-1,-1};
    pipe2(fds,O_CLOEXEC|O_NONBLOCK);

    tev_set_read_handler(tev,fds[0],read_handler,&(fds[0]));
    // periodic_print_hello(NULL);
    // tev_set_timeout(tev,cancel_print_hello,NULL,5000);
    tev_set_timeout(tev,write_some_data,&(fds[1]),500);
    tev_set_timeout(tev,clear_read_handler,&(fds[0]),3000);
    /* user init ends */

    tev_main_loop(tev);

    /* user deinit starts */
    close(fds[0]);
    close(fds[1]);
    /* user deinit ends */

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

void read_handler(void* ctx)
{
    char buffer[10] = {0};
    int fd = *(int*)ctx;
    ssize_t readlen = read(fd,buffer,sizeof(buffer));
    printf("Read handler: %s\n",buffer);
}

void write_some_data(void* ctx)
{
    const char* data="hello";
    int fd = *(int*)ctx;
    write(fd,data,strlen(data)+1);
    printf("Write data to pipe.\n");
}

void clear_read_handler(void* ctx)
{
    tev_set_read_handler(tev,*(int*)ctx,NULL,NULL);
}
