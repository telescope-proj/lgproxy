#ifndef _LP_CONFIG_H_
#define _LP_CONFIG_H_

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

typedef struct {
    struct sockaddr * bind_addr;
    struct sockaddr * data_addr;
    struct sockaddr * frame_addr;
    struct sockaddr * connect_addr;
    char            * mem_path;
    size_t          mem_size;
    char            * transport;
    uint32_t        fabric_ver;
    float           poll_interval;
    int             no_socket_check;
} lp_config_opts;

static inline void lp_free_config(lp_config_opts * opts)
{
    if (opts)
    {
        free(opts->bind_addr);
        free(opts->data_addr);
        free(opts->frame_addr);
        free(opts->connect_addr);
        free(opts->mem_path);
        free(opts->transport);
    }
}

void lp_print_usage(int is_server);

int lp_load_cmdline(int argc, char ** argv, int is_server, lp_config_opts * out);

#endif