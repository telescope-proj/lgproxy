/*
    SPDX-License-Identifier: GPL-2.0-or-later

    Telescope Project  
    Looking Glass Proxy
    
    Copyright (c) 2022 - 2023 Telescope Project Developers

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
    more details.

    You should have received a copy of the GNU General Public License along with
    this program; if not, write to the Free Software Foundation, Inc., 51
    Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA. 
*/

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