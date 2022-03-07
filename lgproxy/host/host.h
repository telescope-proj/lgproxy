#include "trf.h"
#include "trf_ncp.h"
#include <signal.h>
#include <sys/mman.h>


/**
 * @brief Initialize LibTRF Server
 * 
 * @param ctx               Server Context to use
 * @param client_ctx        Client Context to use
 * @param host              Host set '0.0.0.0' to accept all connections
 * @param port              Server port to listen on
 * @return return 0 on success, negative errno on failure
 */
int lpInitServer(PTRFContext ctx, PTRFContext client_ctx, 
            char* host, char* port);