#ifndef _LP_TYPES_H
#define _LP_TYPES_H

#include <stdio.h>
#include "lgmp/client.h"
#include "lgmp/host.h"
#include "lgmp/lgmp.h"
#include "common/KVMFR.h"
#include <stdlib.h>
#include <unistd.h>
#include "trf.h"
#include <sys/mman.h>

enum LP_STATE {
    LP_STATE_RUNNING,
    LP_STATE_INVALID,
    LP_STATE_STOP,
    LP_STATE_RESTART
};

typedef struct {
    PLGMPClient             lgmp_client;
    PLGMPClientQueue        client_q;
    PTRFContext             client_ctx;
} LPClient;

typedef struct {
    PLGMPHost               lgmp_host;
    PLGMPHostQueue          host_q;
    PLGMPMemory             frame_memory[LGMP_Q_FRAME_LEN];
    PTRFContext             server_ctx;
    unsigned int            frame_index;
} LPHost;

struct LPContext {
    enum LP_STATE           state;
    const char *            shm;
    void *                  ram;
    uint32_t                ram_size;
    bool                    format_valid;
    int                     shmFile;
    union{
        LPClient            lp_client;
        LPHost              lp_host;
    };
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