#ifndef _LP_UTILS_H
#define _LP_UTILS_H

#include "trf.h"
#include <stdio.h>
#include "lp_types.h"
#include <sys/stat.h>
#include <math.h>

/**
 * @brief Poll for a message, decoding it if the message has been received.
 * 
 * @param ctx   Context to use.
 * @param msg   Message pointer to be set when a message has been received.
 * @return      0 on success, negative error code on failure.
 */
int lpPollMsg(PLPContext ctx, TrfMsg__MessageWrapper ** msg);

/**
 * @brief Parse bytes neede from string passed in to arguments
 * 
 * @param data          String containing memory amount (e.g. 1048576 or 128m)
 * @return              size in bytes, -1 if invalid data 
 */
uint64_t lpParseMemString(char * data);

/**
 * @brief Check if the SHM File needs to be truncated 
 * 
 * @param  shmFile       Path to shm file
 * @return true if shm file needs to be truncated, false if not
 */
bool lpShouldTruncate(PLPContext ctx);

/**
 * @brief Set Looking Glass Proxy logging level
 * 
 */
static inline void lpSetLPLogLevel()
{
    char* loglevel = getenv("LP_LOG_LEVEL");
    if (!loglevel)
    {
        lp__log_set_level(LP__LOG_INFO);
    }
    else
    {
        int ll = atoi(loglevel);
        switch (ll)
        {
        case 1:
            lp__log_set_level(LP__LOG_TRACE);
            break;
        case 2:
            lp__log_set_level(LP__LOG_DEBUG);
            break;
        case 3:
            lp__log_set_level(LP__LOG_INFO);
            break;
        case 4:
            lp__log_set_level(LP__LOG_WARN);
            break;
        case 5:
            lp__log_set_level(LP__LOG_ERROR);
            break;
        case 6:
            lp__log_set_level(LP__LOG_FATAL);
            break;
        default:
            lp__log_set_level(LP__LOG_INFO);
            break;
        }
    }
}

/**
 * @brief Set Libtrf logging level
 * 
 */
static inline void lpSetTRFLogLevel()
{
    char* loglevel = getenv("TRF_LOG_LEVEL");
    if (!loglevel)
    {
        trf__log_set_level(LP__LOG_INFO);
    }
    else
    {
        int ll = atoi(loglevel);
        switch (ll)
        {
        case 1:
            trf__log_set_level(TRF__LOG_TRACE);
            break;
        case 2:
            trf__log_set_level(TRF__LOG_DEBUG);
            break;
        case 3:
            trf__log_set_level(TRF__LOG_INFO);
            break;
        case 4:
            trf__log_set_level(TRF__LOG_WARN);
            break;
        case 5:
            trf__log_set_level(TRF__LOG_ERROR);
            break;
        case 6:
            trf__log_set_level(TRF__LOG_FATAL);
            break;
        default:
            trf__log_set_level(TRF__LOG_INFO);
            break;
        }
    }
}

/**
 * @brief Set Default Options
 * 
 * @param ctx     Context to use
 * @return 0 on success, negative error code on failure
 */
int lpSetDefaultOpts(PLPContext ctx);

/**
 * @brief Calculate the size needed for shared memory file
 * 
 * @param display   Display metadata from Libtrf
 * @return size of the display needed
 */
int lpCalcFrameSizeNeeded(PTRFDisplay display);
#endif