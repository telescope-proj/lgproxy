#ifndef _LP_TRF_CLIENT_H
#define _LP_TRF_CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#include "lp_types.h"
#include "lp_log.h"
#include <signal.h>
#include <sys/mman.h>
#include "trf.h"
#include "trf_ncp.h"
#include "trf_ncp_client.h"


/**
 * @brief Initialize the Client connection to server
 * @param ctx           Context to use
 * @param host          Host IP address to connect to
 * @param port          Port to connect to on server
 * @return 0 on success, negative error code on error.
 */
int lpTrfClientInit(PLPContext ctx, char * host, char * port);



#endif