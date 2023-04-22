/*
    SPDX-License-Identifier: GPL-2.0-only

    Telescope Project  
    Looking Glass Proxy   
    Utilities Functions
    
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
#include "lp_utils.h"

uint64_t lp_parse_mem_string(char * data)
{
    char multiplier = data[strlen(data)-1];
    uint64_t b = atoi(data);
    if (multiplier >= '0' && multiplier <= '9')
    {
        return b;
    }
    else
    {
        switch (multiplier)
        {
            case 'K':
            case 'k':
                return b * 1024;
            case 'M':
            case 'm':
                return b * 1024 * 1024;
            case 'G':
            case 'g':
                return b * 1024 * 1024 * 1024;
            default:
                return 0;
        }
    }
}