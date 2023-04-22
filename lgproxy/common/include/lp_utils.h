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

#ifndef _LP_UTILS_H_
#define _LP_UTILS_H_

#include <stdio.h>
#include <sys/stat.h>
#include <math.h>

#include "lp_log.h"
#include "tcm_log.h"
#include "lp_types.h"

static inline size_t lp_get_page_size() {
    #if defined(_WIN32)
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return si.dwPageSize;
    #else
        return sysconf(_SC_PAGESIZE);
    #endif
}

/**
 * @brief Parse byte amount with units from string
 * 
 * @param data          String containing memory amount (e.g. 1048576 or 128m)
 * @return              Actual size in bytes, -1 if invalid data 
 */
uint64_t lp_parse_mem_string(char * data);

/**
 * @brief Set Looking Glass Proxy logging level
 */
static inline void lp_set_log_level()
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
            tcm__log_set_level(TCM__LOG_TRACE);
            break;
        case 2:
            lp__log_set_level(LP__LOG_DEBUG);
            tcm__log_set_level(LP__LOG_DEBUG);
            break;
        case 3:
            lp__log_set_level(LP__LOG_INFO);
            tcm__log_set_level(LP__LOG_INFO);
            break;
        case 4:
            lp__log_set_level(LP__LOG_WARN);
            tcm__log_set_level(LP__LOG_WARN);
            break;
        case 5:
            lp__log_set_level(LP__LOG_ERROR);
            tcm__log_set_level(LP__LOG_ERROR);
            break;
        case 6:
            lp__log_set_level(LP__LOG_FATAL);
            tcm__log_set_level(LP__LOG_FATAL);
            break;
        default:
            lp__log_set_level(LP__LOG_INFO);
            tcm__log_set_level(LP__LOG_INFO);
            break;
        }
    }
}

#endif