#ifndef _LP_TRF_SERVER_H
#define _LP_TRF_SERVER_H


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#include "lp_types.h"
#include "lp_log.h"
#include <signal.h>
#include <sys/mman.h>
#include "trf_ncp_server.h"
#include "trf.h"
#include "trf_ncp.h"

/**
 * @brief Initialize the Libtrf Server
 * @param ctx       Context to use
 * @param host      Allow host set "0.0.0.0" to allow all incoming connections
 * @param port      Port to listen on during the negotiation process
 * @return 0 on success, negative error code on error.
 */ 
int lpTrfServerInit(PLPContext ctx, char * host, char * port);

#endif