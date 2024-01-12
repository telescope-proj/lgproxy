// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef LP_TYPES_H_
#define LP_TYPES_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define POINTER_SHAPE_BUFFERS 3
#define MAX_POINTER_SIZE (512 * 512 * 4 + sizeof(KVMFRCursor))
#define MAX_POINTER_SIZE_ALIGN                                                 \
  (MAX_POINTER_SIZE + (4096 - MAX_POINTER_SIZE % 4096))

/*  This error code is used specifically for conditions that should never occur
    in properly functioning code: the error should crash the process. */
#define LP_INVALID_BEHAVIOUR 1025

#endif