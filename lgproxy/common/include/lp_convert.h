/*
    SPDX-License-Identifier: GPL-2.0-or-later

    Telescope Project  
    Looking Glass Proxy   
    Conversion Functions  
    
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

#ifndef _LP_CONVERT_H
#define _LP_CONVERT_H

#include "common/types.h"
#include "trf_def.h"

/**
 * @brief Convert LookingGlass texture types to Libtrf texture types
 * 
 * @param lg_type           Looking Glass texture type
 * @return Libtrf Texture type
 */
static inline uint64_t lpLGToTrfFormat(int lg_type)
{
    switch (lg_type)
    {
    case FRAME_TYPE_BGRA:
        return TRF_TEX_BGRA_8888;
    case FRAME_TYPE_RGBA:
        return TRF_TEX_RGBA_8888;
    case FRAME_TYPE_RGBA10:
        return TRF_TEX_RGBA_1010102;
    case FRAME_TYPE_RGBA16F:
        return TRF_TEX_RGBA_16161616F;
    case FRAME_TYPE_RGB_24:
        return TRF_TEX_RGB_888;
    case FRAME_TYPE_BGR_32:
        return TRF_TEX_BGR_32;
    default:
        return TRF_TEX_INVALID;
    }
}

/**
 * @brief Convert Libtrf texture types to Looking Glass texture types
 * 
 * @param trf_type      Libtrf texture types
 * @return Looking Glass texture type
 */
static inline uint64_t lpTrftoLGFormat(int trf_type)
{
    switch(trf_type)
    {
        case TRF_TEX_BGRA_8888:
            return FRAME_TYPE_BGRA;
        case TRF_TEX_RGBA_8888:
            return FRAME_TYPE_RGBA;
        case TRF_TEX_RGBA_1010102:
            return FRAME_TYPE_RGBA10;
        case TRF_TEX_RGBA_16161616F:
            return FRAME_TYPE_RGBA16F;
        case TRF_TEX_RGB_888:
            return FRAME_TYPE_RGB_24;
        case TRF_TEX_BGR_32:
            return FRAME_TYPE_BGR_32;
        default:
            return FRAME_TYPE_INVALID;
    }
}

#endif