/*
    SPDX-License-Identifier: GPL-2.0-only

    Telescope Project  
    Looking Glass Proxy   
    Type Definitions 
    
    Copyright (c) 2022 Telescope Project Developers

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
#ifndef _LP_TYPES_H
#define _LP_TYPES_H

#include "lp_log.h"

#include <stdio.h>
#include "lgmp/client.h"
#include "lgmp/host.h"
#include "lgmp/lgmp.h"
#include "common/KVMFR.h"
#include "common/time.h"
#include <stdlib.h>
#include <unistd.h>
#include "trf.h"
#include <sys/mman.h>

#define POINTER_SHAPE_BUFFERS 3
#define MAX_POINTER_SIZE (sizeof(KVMFRCursor) + (512 * 512 * 4))


enum T_STATE {
    T_RUNNING,
    T_ERR,
    T_STOP
};

enum LP_STATE {
    LP_STATE_RUNNING,
    LP_STATE_INVALID,
    LP_STATE_STOP,
    LP_STATE_RESTART
};

typedef enum LG_RendererCursor
{
    LG_CURSOR_COLOR,
    LG_CURSOR_MONOCHROME,
    LG_CURSOR_MASKED_COLOR
} LG_RendererCursor;


typedef struct {
    /**
     * @brief LGMP host
     * 
     */
    PLGMPHost               lgmp_host;
    /**
     * @brief Host queue for frame data
     * 
     */
    PLGMPHostQueue          host_q;
    /**
     * @brief Pointer LGMP queue
     * 
     */
    PLGMPHostQueue          pointer_q;
    /**
     * @brief PTRFContext containing client connection
     * 
     */
    PTRFContext             client_ctx;
    /**
     * @brief Subchannel for handling cursor data
     * 
     */
    PTRFContext             sub_channel;
    /**
     * @brief Thread flags for monitoring subchannel
     * 
     */
    enum T_STATE            thread_flags;
    /**
     * @brief LGMP memory for frame data
     * 
     */
    PLGMPMemory             frame_memory[LGMP_Q_FRAME_LEN];
    /**
     * @brief LGMP memory for pointer data
     * 
     */
    PLGMPMemory             pointer_memory[LGMP_Q_POINTER_LEN];
    /**
     * @brief LGMP memory for cursor shape data
     * 
     */
    PLGMPMemory             cursor_shape[POINTER_SHAPE_BUFFERS];
    /**
     * @brief Frame index
     * 
     */
    unsigned int            frame_index;
    /**
     * @brief Pointer index
     * 
     */
    unsigned int            pointer_index;
    /**
     * @brief Cursor shape index
     * 
     */
    unsigned                cursor_shape_index;
    /**
     * @brief Pointer shape state
     * 
     */
    bool                    pointer_shape_valid;
    /**
     * @brief Subchannel state whether it has started or not
     * 
     */
    bool                    sub_started;
    /**
     * @brief Cursor thread
     * 
     */
    pthread_t               cursor_thread;
} LPClient;

typedef struct {
    /**
     * @brief LGMP client
     * 
     */
    PLGMPClient             lgmp_client;
    /**
     * @brief LGMP client queue for frames
     * 
     */
    PLGMPClientQueue        client_q;
    /**
     * @brief LGMP pointer queue
     * 
     */
    PLGMPClientQueue        pointer_q;
    /**
     * @brief Server listening context
     * 
     */
    PTRFContext             server_ctx;
    /**
     * @brief Client context, will be created once a new client is received
     * 
     */
    PTRFContext             client_ctx;
    /**
     * @brief Subchannel for handling cursor data
     * 
     */
    PTRFContext             sub_channel;
    /**
     * @brief Subchannel flags for thread monitoring
     * 
     */
    enum T_STATE            thread_flags;
} LPHost;

typedef struct {
    /**
     * @brief       Libfabric Poll Interval default will be set to 1 millisecond
     * 
     */
    int                     poll_int;
    /**
     * @brief       Delete SHM file on exit default will it will not delete the shm file unless specified
     * 
     */
    bool                    delete_exit;
}LPUserOpts;

typedef enum {
    /**
     * @brief Memory has not been registered for RDMA transfers.
     */
    LP_MEM_UNREGISTERED     = 0,
    /**
     * @brief Memory has been registered temporarily. This is used when the
     * physical pages backing the buffer may change once a client reconnects,
     * e.g. in the case that DMABUF (/dev/kvmfr*) is used.
     */
    LP_MEM_REGISTERED_TEMP  = 1,
    /**
     * @brief Memory has been registered for the entire session. The page
     * mappings will not change while LGProxy is running.
     */
    LP_MEM_REGISTERED_PERM  = 2,
    /**
     * @brief Sentinel value
     */
    LP_MEM_MAX              = 3
} LPMemState;

/**
 * @brief Context containing all connection details for the server and client and also LGMP Queues, Client, Host information
 * 
 */
struct LPContext {
    /**
     * @brief Memory state
     * 
     */
    LPMemState              mem_state;
    /**
     * @brief App State
     * 
     */
    enum LP_STATE           state;
    /**
     * @brief Shared memory file path
     * 
     */
    const char *            shm;
    /**
     * @brief Pointer to shared memory
     * 
     */
    void *                  ram;
    /**
     * @brief Size of the shared memory
     * 
     */
    uint32_t                ram_size;
    /**
     * @brief Frame format valid state
     * 
     */
    bool                    format_valid;
    /**
     * @brief File Descriptor for the shared memory file
     * 
     */
    int                     shmFile;
    /**
     * @brief LP Client Context
     * 
     */
    LPClient                lp_client;
    /**
     * @brief LP Host Context
     * 
     */
    LPHost                  lp_host;
    /**
     * @brief User defined options (e.g. Delete shm file on closing)
     * 
     */
    LPUserOpts              opts;
    /**
     * @brief DMA Buffer support
     * 
     */
    bool                    dma_buf;
};


#define PLPContext struct LPContext *

/**
 * @brief Allocates Memory for lpAllocContext
 * 
 * @return PLPContext
*/
PLPContext lpAllocContext();

/**
 * @brief Destroy PTRFContext
 * 
 * @param ctx Context to use
 */
void lpDestroyContext(PLPContext ctx);

#endif