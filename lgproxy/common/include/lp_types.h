#ifndef _LP_TYPES_H
#define _LP_TYPES_H


#include <stdio.h>
#include "lgmp/client.h"
#include "common/KVMFR.h"
#include <stdlib.h>
#include <unistd.h>
#include "trf.h"


struct LPContext {
    PLGMPClient             lgmp_client;
    PLGMPHost               lgmp_host;
    PLGMPClientQueue        client_q;
    PLGMPHostQueue          host_q;
    PTRFContext             server_ctx;
    PTRFContext             client_ctx;
    enum LP_STATE           state;
    const char*             shm;
    void *                  ram;
    uint32_t                ram_size;
};

enum LP_STATE{
    LP_STATE_RUNNING,
    LP_STATE_INVALID,
    LP_STATE_STOP,
    LP_STATE_RESTART
};

#define PLPContext struct LPContext *


#endif