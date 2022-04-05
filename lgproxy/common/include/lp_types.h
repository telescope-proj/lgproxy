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
    T_STOPPED
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
    PLGMPClient             lgmp_client;
    PLGMPClientQueue        client_q;
    PLGMPClientQueue        pointer_q;
    PTRFContext             client_ctx;
    PTRFContext             sub_channel;
    enum T_STATE            thread_flags;
} LPClient;

typedef struct {
    PLGMPHost               lgmp_host;
    PLGMPHostQueue          host_q;
    PLGMPHostQueue          pointer_q;
    PLGMPMemory             frame_memory[LGMP_Q_FRAME_LEN];
    PLGMPMemory             pointer_memory[LGMP_Q_POINTER_LEN];
    PLGMPMemory             cursor_shape[POINTER_SHAPE_BUFFERS];
    // PLGMPMemory             pointer_shape;
    unsigned                cursor_shape_index;
    bool                    pointer_shape_valid;
    PTRFContext             server_ctx;
    PTRFContext             sub_channel;
    enum T_STATE            thread_flags;
    unsigned int            frame_index;
    unsigned int            pointer_index;
} LPHost;

struct LPContext {
    enum LP_STATE           state;
    const char *            shm;
    void *                  ram;
    uint32_t                ram_size;
    bool                    format_valid;
    int                     shmFile;
    LPClient                lp_client;
    LPHost                  lp_host;
};


#define PLPContext struct LPContext *

/**
 * @brief Poll for a message, decoding it if the message has been received.
 * 
 * @param ctx   Context to use.
 * @param msg   Message pointer to be set when a message has been received.
 * @return      0 on success, negative error code on failure.
 */
int lpPollMsg(PLPContext ctx, TrfMsg__MessageWrapper ** msg);

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