/*

    SPDX-License-Identifier: GPL-2.0-only

    Telescope Project 
    Looking Glass Proxy
    Custom Messaging Functions

    Copyright (c) 2022 Matthew John McMullin

    This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA. 

*/

#ifndef _LP_MSG_H
#define _LP_MSG_H

#include <stdio.h>

#include "trf.h"
#include "trf_ncp.h"
#include "lp_msg.pb-c.h"
#include "lp_types.h"


int lpKeepAlive(PTRFContext ctx);

#endif