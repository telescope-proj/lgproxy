#include "lp_config.h"
#include "lp_log.h"
#include "lp_build_version.h"
#include "lp_version.h"
#include "lp_utils.h"
#include "lg_build_version.h"

#include <arpa/inet.h>
#include <string.h>
#include <errno.h>

#include <rdma/fabric.h>

struct lp__optdesc {
    int     id;
    char    * help_str;
    char    * example;
    char    * default_val;
};

static struct option client_opts[] = {
    { "bind",       required_argument, NULL, 'b'},  
    { "server",     required_argument, NULL, 's'},  
    { "transport",  required_argument, NULL, 't'}, 
    { "data_port",  required_argument, NULL, 'd'}, 
    { "frame_port", required_argument, NULL, 'f'}, 
    { "version",    required_argument, NULL, 'v'}, 
    { "mem",        required_argument, NULL, 'm'},
    { "interval",   required_argument, NULL, 'i'}, 
    { "mem_size",   required_argument, NULL, 'l'},
    { 0, 0, 0, 0 }
};

static struct lp__optdesc opts_desc[] = {
    {'b',
        "Bind (local) address", 
        "192.168.1.100:12345", 
        NULL
    },
    {'s', 
        "Server address",
        "192.168.1.200:23456",
        NULL
    },
    {'t', 
        "Libfabric transport name",
        "verbs;ofi_rxm or tcp;ofi_rxm",
        "verbs;ofi_rxm"
    },
    {'d',
        "Data channel source port",
        "32000",
        "32000"
    },
    {'f',
        "Frame channel source port",
        "32001",
        "32001"
    },
    {'v',
        "Libfabric version",
        "1.15",
        "1.10"
    },
    {'m',
        "Shared memory or KVMFR file path.",
        "/dev/shm/looking-glass",
        "/dev/shm/looking-glass"
    },
    {'i',
        "Data polling interval in milliseconds",
        "1",
        "0"
    },
    {'l',
        "Shared memory or KVMFR file size (only created if the file does not "
        "exist)",
        "32000",
        "32000"
    },
    {'n',
        "Disable socket data check when the fabric channels are active",
        "1",
        "0"
    },
    { 0, 0, 0, 0 }
};

static struct option server_opts[] = {
    { "bind",       required_argument, NULL, 'b'},
    { "transport",  required_argument, NULL, 't'},
    { "data_port",  required_argument, NULL, 'd'},  
    { "frame_port", required_argument, NULL, 'f'}, 
    { "version",    required_argument, NULL, 'v'}, 
    { "mem",        required_argument, NULL, 'm'}, 
    { "interval",   required_argument, NULL, 'i'}, 
    { "no_sock",    optional_argument, NULL, 'n'},
    { 0, 0, 0, 0 }
};

void lp_print_usage(int is_server)
{
    struct option * opts = is_server ? server_opts : client_opts;
    
    printf(
        "Telescope Project LGProxy v%d.%d.%d-%s %s\n"
        "Looking Glass %s + Libfabric %d.%d.%d\n"
        "Copyright (c) 2022-2023 Telescope Project Developers\n"
        "\n"
        "Documentation & software licenses:\n"
        "    https://telescope-proj.github.io/lgproxy\n"
        "\n"
        "Available options:\n\n",
        LP_VERSION_MAJOR, LP_VERSION_MINOR, LP_VERSION_PATCH, LP_BUILD_VERSION,
        is_server ? "(source)" : "(sink)", 
        LG_BUILD_VERSION,
        FI_MAJOR_VERSION, FI_MINOR_VERSION, FI_REVISION_VERSION
    );
    
    for (int i = 0; opts[i].val; i++)
    {
        for (int j = 0; opts_desc[j].id; j++)
        {
            if (opts_desc[j].id == opts[i].val)
            {
                printf("-%c, --%s\n", opts[i].val, opts[i].name);
                // printf("\t%s\n", opts[i].has_arg == required_argument 
                                                //    ? "required" : "optional");
                printf("\t%s\n", opts_desc[j].help_str);
                if (opts_desc[j].example)
                    printf("\tExample: %s\n", opts_desc[j].example);
                if (opts_desc[j].default_val)
                    printf("\tDefault: %s\n", opts_desc[j].default_val);
                printf("\n");
            }
        }
    }
}

int lp_str_to_sockaddr(char * str, struct sockaddr * sa_out)
{
    char * tmp = strtok(str, ":");
    if (!tmp)
        return -EINVAL;
    int ret = inet_pton(AF_INET, str, &((struct sockaddr_in *) sa_out)->sin_addr);
    if (ret != 1)
        return -EINVAL;
    tmp = strtok(NULL, ":");
    if (!tmp)
        return -EINVAL;
    ret = atoi(tmp);
    if (ret == 0)
        return -EINVAL;
    ((struct sockaddr_in *) sa_out)->sin_port = ntohs(ret);
    sa_out->sa_family = AF_INET;
    return 0;
}

uint32_t lp_str_to_fabric_ver(char * str)
{
    int val = 0;
    char * tmp = strtok(optarg, ".");
    if (!tmp)
        return 0;
    val += FI_MAJOR(atoi(tmp));
    tmp = strtok(NULL, ".");
    if (!tmp)
        return 0;
    val += FI_MINOR(atoi(tmp));
    return val;
}

int lp_load_cmdline(int argc, char ** argv, int is_server, lp_config_opts * out)
{
    int o, idx, ret;
    lp_config_opts * tmp = calloc(1, sizeof(*tmp));
    if (!tmp)
        goto nomem;
    tmp->bind_addr      = calloc(1, sizeof(*tmp->bind_addr));
    tmp->connect_addr   = calloc(1, sizeof(*tmp->connect_addr));
    tmp->data_addr      = calloc(1, sizeof(*tmp->data_addr));
    tmp->frame_addr     = calloc(1, sizeof(*tmp->frame_addr));
    if (!tmp->bind_addr || !tmp->connect_addr || !tmp->data_addr || !tmp->frame_addr)
        goto nomem;
    
    opterr = 0;
    idx = 0;
    while ((o = getopt_long(argc, argv, 
            is_server ? "b:t:d:f:v:m:i:n" : "b:s:t:d:f:v:m:i:l:", 
            is_server ? server_opts : client_opts, &idx)) != -1)
    {
        lp__log_trace("Input argument: %s", optarg);

        switch (o)
        {
            /* Bind address (client/server) */
            case 'b':
                lp__log_trace("Bind address: %s", optarg);
                ret = lp_str_to_sockaddr(optarg, tmp->bind_addr);
                if (ret < 0)
                {
                    lp__log_error("Bind address parsing failed: %s", strerror(-ret));
                    lp_free_config(tmp);
                    return ret;
                }
                ((struct sockaddr_in *) tmp->data_addr)->sin_addr.s_addr = \
                    ((struct sockaddr_in *) tmp->bind_addr)->sin_addr.s_addr;
                ((struct sockaddr_in *) tmp->frame_addr)->sin_addr.s_addr = \
                    ((struct sockaddr_in *) tmp->bind_addr)->sin_addr.s_addr;
                tmp->data_addr->sa_family = AF_INET;
                tmp->frame_addr->sa_family = AF_INET;
                break;
            
            /* Server address (client) */
            case 's':
                lp__log_trace("Server address: %s", optarg);
                ret = lp_str_to_sockaddr(optarg, tmp->connect_addr);
                if (ret < 0)
                {
                    lp_free_config(tmp);
                    return ret;
                }
                break;

            /* Data channel port */
            case 'd':
                lp__log_trace("Data channel port: %s", optarg);
                ((struct sockaddr_in *) tmp->data_addr)->sin_port = \
                    htons(atoi(optarg));
                break;
            
            /* Frame channel port */
            case 'f':
                lp__log_trace("Frame channel port: %s", optarg);
                ((struct sockaddr_in *) tmp->frame_addr)->sin_port = \
                    htons(atoi(optarg));
                break;
            
            /* Transport selection (client/server) */
            case 't':
                lp__log_trace("LF transport: %s", optarg);
                out->transport = strdup(optarg);
                if (!out->transport)
                    goto nomem;
                break;

            /* Shared memory file path (client/server) */
            case 'm':
                lp__log_trace("SHM file: %s", optarg);
                out->mem_path = strdup(optarg);
                if (!out->mem_path)
                    goto nomem;
                break;

            /* Shared memory file size */
            case 'l':
                lp__log_trace("Mem size: %s", optarg);
                out->mem_size = lp_parse_mem_string(optarg);
                if (out->mem_size == 0 || out->mem_size == UINT64_MAX)
                {
                    lp__log_error("Invalid memory size %s", optarg);
                    lp_free_config(out);
                    return -EINVAL;
                }
                break;

            /* Fabric polling interval (client/server) */
            case 'i':
                out->poll_interval = (strtof(optarg, NULL) / 1000.0f);
                lp__log_trace("Polling interval: %f (%s)", out->poll_interval, optarg);
                break;

            /* Libfabric API version (client/server) */
            case 'v':
                lp__log_trace("Libfabric version: %s", optarg);
                uint32_t fver = lp_str_to_fabric_ver(optarg);
                if (fver < FI_VERSION(1, 0) || fver >= FI_VERSION(2, 0))
                {
                    lp_free_config(out);
                    return -EINVAL;
                }
                out->fabric_ver = fver;
                break;
            
            /* Disable socket check (server) */
            case 'n':
                lp__log_trace("Socket check: %s", optarg);
                if (optarg == NULL)
                    out->no_socket_check = 1;
                else
                    out->no_socket_check = atoi(optarg);
                break;

            /* Invalid argument */
            default:
                lp__log_warn("Unknown argument %s", optarg);
                break;
        }
    }

    /* Verify that the required arguments are present */
    if (is_server)
    {
        if (((struct sockaddr_in *) tmp->bind_addr)->sin_family == AF_UNSPEC)
        {
            lp__log_error("Required argument not found: bind address");
            lp_free_config(tmp);
            return -EINVAL;
        }
    }
    else
    {
        if (((struct sockaddr_in *) tmp->bind_addr)->sin_family == AF_UNSPEC)
        {
            lp__log_error("Required argument not found: bind address");
            lp_free_config(tmp);
            return -EINVAL;
        }
        if (((struct sockaddr_in *) tmp->connect_addr)->sin_family == AF_UNSPEC)
        {
            lp__log_error("Required argument not found: server address");
            lp_free_config(tmp);
            return -EINVAL;
        }
    }

    if (tmp->bind_addr->sa_family != AF_UNSPEC)
        out->bind_addr = tmp->bind_addr;
    else
        free(tmp->bind_addr);
    
    if (tmp->connect_addr->sa_family != AF_UNSPEC) 
        out->connect_addr = tmp->connect_addr;
    else
        free(tmp->connect_addr);

    if (tmp->frame_addr->sa_family != AF_UNSPEC)
        out->frame_addr = tmp->frame_addr;
    else
        free(tmp->frame_addr);

    if (tmp->data_addr->sa_family != AF_UNSPEC)
        out->data_addr = tmp->data_addr;
    else
        free(tmp->data_addr);
    
    /* Set the default options for optional arguments */
    if (!out->mem_path)
    {
        lp__log_trace("Setting default value for mem_path");
        out->mem_path = strdup("/dev/shm/looking-glass");
        if (!out->mem_path)
            goto nomem;
    }
    if (!out->transport)
    {
        lp__log_trace("Setting default value for transport");
        out->transport = strdup("verbs;ofi_rxm");
        if (!out->transport)
            goto nomem;
    }
    if (((struct sockaddr_in *) tmp->data_addr)->sin_port == 0)
        ((struct sockaddr_in *) tmp->data_addr)->sin_port = htons(32000);
    if (((struct sockaddr_in *) tmp->frame_addr)->sin_port == 0)
        ((struct sockaddr_in *) tmp->frame_addr)->sin_port = htons(32001);
    if (!out->fabric_ver)
        out->fabric_ver = FI_VERSION(1, 10);
    if (!out->poll_interval)
        out->poll_interval = 0;

    lp__log_debug("Config parsing successful");
    return 0;

nomem:
    lp__log_error("Config parsing aborted due to out of memory error");
    lp_free_config(out);
    return -ENOMEM;
}