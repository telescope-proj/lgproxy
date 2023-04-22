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

#ifndef _LP_TYPES_H_
#define _LP_TYPES_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#include "lgmp/client.h"
#include "lgmp/host.h"
#include "lgmp/lgmp.h"
#include "common/KVMFR.h"
#include "common/time.h"

#include "lp_log.h"
#include "lp_msg.h"

#define POINTER_SHAPE_BUFFERS   3
#define MAX_POINTER_SIZE        (sizeof(lp_msg_cursor) + (512 * 512 * 4))
#define MAX_POINTER_SIZE_ALIGN  (MAX_POINTER_SIZE + (4096 - MAX_POINTER_SIZE % 4096))

#endif