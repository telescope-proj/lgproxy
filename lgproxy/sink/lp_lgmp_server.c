/*
    SPDX-License-Identifier: GPL-2.0-only

    Telescope Project  
    Looking Glass Proxy   
    Client side functions 
    
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
#include "lp_lgmp_server.h"
#include "lp_utils.h"
#include "lg_build_version.h"

/* KVMFR User Data Internals - Source: LookingGlass/host/src/app.c */

typedef struct KVMFRUserData
{
  size_t    size;
  size_t    used;
  uint8_t * data;
}
KVMFRUserData;

static bool appendData(KVMFRUserData * dst, const void * src, const size_t size)
{
  if (size > dst->size - dst->used)
  {
    size_t newSize = dst->size + (1024 > size ? size : 1024);
    dst->data = realloc(dst->data, newSize);
    if (!dst->data)
    {
      lp__log_error("Out of memory!");
      return false;
    }

    memset(dst->data + dst->size, 0, newSize - dst->size);
    dst->size = newSize;
  }

  memcpy(dst->data + dst->used, src, size);
  dst->used += size;
  return true;
}

static bool newKVMFRData(KVMFRRecord_VMInfo * vm_info, KVMFRRecord_OSInfo * os_info,
                         uint16_t feature_flags, KVMFRUserData * dst)
{
    KVMFR kvmfr =
    {
        .magic    = KVMFR_MAGIC,
        .version  = KVMFR_VERSION,
        .features = feature_flags
    };
    strncpy(kvmfr.hostver, LG_BUILD_VERSION, sizeof(kvmfr.hostver) - 1);
    if (!appendData(dst, &kvmfr, sizeof(kvmfr)))
        return false;
    size_t model_len = strnlen(vm_info->model, 63) + 1;
    KVMFRRecord record =
    {
        .type = KVMFR_RECORD_VMINFO,
        .size = sizeof(*vm_info) + model_len
    };
    if (!appendData(dst, &record, sizeof(record))   ||
        !appendData(dst, vm_info, sizeof(*vm_info)) ||
        !appendData(dst, vm_info->model, model_len))
      return false;
    record.type = KVMFR_RECORD_OSINFO;
    size_t name_len = strnlen(os_info->name, 31) + 1;
    record.size = sizeof(*os_info) + name_len;
    if (!appendData(dst, &record, sizeof(record))   ||
        !appendData(dst, os_info, sizeof(*os_info)) ||
        !appendData(dst, os_info->name, name_len))
      return false;

    return true;
}

/* --------------------------------------------------------------- */

int lp_init_lgmp_host(lp_lgmp_server_ctx * ctx, KVMFRRecord_VMInfo * vm_info,
                      KVMFRRecord_OSInfo * os_info, uint16_t feature_flags,
                      size_t display_size)
{
    if (((ssize_t) display_size) > ctx->mem_size / LGMP_Q_FRAME_LEN 
        - POINTER_SHAPE_BUFFERS * MAX_POINTER_SIZE)
        return -ENOBUFS;
    
    int ret;
    KVMFRUserData udata = {0};
    if (!newKVMFRData(vm_info, os_info, feature_flags, &udata))
    {
        lp__log_error("KVMFR user data init failed");
        return -EINVAL;
    }
    lp__log_debug("KVMFR user data: %lu of %lu bytes used",
                  udata.used, udata.size);
    LGMP_STATUS status;
    status = lgmpHostInit(ctx->mem, ctx->mem_size, &ctx->host, 
                          udata.used, udata.data);
    if (status != LGMP_OK)
    {
        lp__log_error("Unable to initialize LGMP host: %s", 
                      lgmpStatusString(status));
        ret = -1;
        goto free_udata;
    }

    /* Create LGMP host queues - same config as the official LG host app */

    static const struct LGMPQueueConfig FRAME_QUEUE_CONFIG =
    {
        .queueID     = LGMP_Q_FRAME,
        .numMessages = LGMP_Q_FRAME_LEN,
        .subTimeout  = 1000
    };

    static const struct LGMPQueueConfig POINTER_QUEUE_CONFIG =
    {
        .queueID     = LGMP_Q_POINTER,
        .numMessages = LGMP_Q_POINTER_LEN,
        .subTimeout  = 1000
    };

    status = lgmpHostQueueNew(ctx->host, FRAME_QUEUE_CONFIG, &ctx->frame_q);
    if (status != LGMP_OK)
    {
        lp__log_error("LGMP frame queue creation failed: %s",
                      lgmpStatusString(status));
        return -ENOSPC;
    }
    status = lgmpHostQueueNew(ctx->host, POINTER_QUEUE_CONFIG, &ctx->ptr_q);
    if (status != LGMP_OK)
    {
        lp__log_error("LGMP cursor queue creation failed: %s",
                      lgmpStatusString(status));
        return -ENOSPC;
    }
    
    /* Allocate memory for all LGMP host queues */

    for(int i = 0; i < LGMP_Q_POINTER_LEN; ++i)
    {
        if ((status = lgmpHostMemAlloc(ctx->host, 
            sizeof(lp_msg_kvmfr), &ctx->ptr_q_mem[i])) != LGMP_OK)
        {
        lp__log_error("lgmpHostMemAlloc Failed (Pointer): %s", 
            lgmpStatusString(status));
        goto free_udata;
        }
        memset(lgmpHostMemPtr(ctx->ptr_q_mem[i]), 0, sizeof(lp_msg_kvmfr));
    }
    for(int i = 0; i < POINTER_SHAPE_BUFFERS; ++i)
    {
        if ((status = lgmpHostMemAlloc(ctx->host, 
            MAX_POINTER_SIZE_ALIGN, &ctx->ptr_shape_mem[i])) != LGMP_OK)
        {
        lp__log_error("lgmpHostMemAlloc Failed (Pointer Shapes): %s", 
                lgmpStatusString(status));
        goto free_udata;
        }
        memset(lgmpHostMemPtr(ctx->ptr_shape_mem[i]), 0, MAX_POINTER_SIZE);
    }
    for (int i = 0; i < LGMP_Q_FRAME_LEN; ++i )
    {
        if ((status = lgmpHostMemAllocAligned(ctx->host, display_size,
                lp_get_page_size(), &ctx->frame_q_mem[i])) != LGMP_OK)
        {
            lp__log_error("lgmpHostMemAllocAligned Failed: %s", 
                lgmpStatusString(status));
            ret = -1;
            goto free_udata;
        }
    }
    
    ctx->frame_q_pos    = 0;
    ctx->ptr_q_pos      = 0;
    ctx->ptr_shape_pos  = 0;
    return 0;

free_udata:
    free(udata.data);
    return ret;
}

int lp_init_mem_server(const char * path, int * fd_out, int * dma_fd_out, 
                       void ** mem, size_t * size)
{
    int fd, dma_fd, tmp_size, ret, is_kvmfr, exist = 1;
    tmp_size = ret = is_kvmfr = 0;
    fd = dma_fd = -1;
    if (strncmp(path, "/dev/kvmfr", 10) == 0)
    {
        fd = open(path, O_RDWR, (mode_t) 0600);
        if (fd < 0)
        {
            lp__log_error("Unable to open KVMFR file: %s", strerror(errno));
            return -errno;
        }
        tmp_size = ioctl(fd, KVMFR_DMABUF_GETSIZE, 0);
        if (tmp_size <= 0)
        {
            const struct kvmfr_dmabuf_create create =
            {
                .flags  = KVMFR_DMABUF_FLAG_CLOEXEC,
                .offset = 0,
                .size   = *size
            };
            dma_fd = ioctl(fd, KVMFR_DMABUF_CREATE, &create);
            if (dma_fd < 0)
            {
                lp__log_error("KVMFR DMABUF creation failed: %s", strerror(errno));
                return -errno;
            }
        }
        lp__log_info("Using DMABUF file: %s, size: %lu", path, tmp_size);
    }
    else
    {
        struct stat st;
        ret = stat(path, &st);
        if (ret < 0)
        {
            if (errno == ENOENT)
            {
                lp__log_info("File %s does not exist, creating it", path);
                exist = 0;
                if (!*size)
                {
                    lp__log_error("Size not specified, aborting creation");
                    return -EINVAL;
                }
            }
        }
        fd = open(path, O_RDWR | O_CREAT, (mode_t) 0600);
        if (fd < 0)
        {
            lp__log_error("Unable to open shared memory file: %s", 
                            strerror(errno));
            return -errno;
        }
        if (!exist)
        {
            if(ftruncate(fd, *size) != 0)
            {
                lp__log_error("Unable to resize shared memory file: %s",
                              strerror(errno));
                ret = -errno;
                goto close_fd;
            }
        }
        else
            tmp_size = st.st_size;
        lp__log_info("Using regular shm file: %s, size: %lu", path, tmp_size);
    }
    
    void * ram = mmap(0, tmp_size, PROT_READ | PROT_WRITE, MAP_SHARED, 
                      dma_fd > 0 ? dma_fd : fd, 0);
    if (ram == MAP_FAILED)
    {
        lp__log_error("Memory mapping failed: %s", strerror(errno));
    }
    
    /*  Unlike the sending side, we don't init the LGMP objects here, since
        we don't have the user data from that side yet. */
    *fd_out     = fd;
    *dma_fd_out = dma_fd;
    *size       = tmp_size;
    *mem        = ram;
    return is_kvmfr;

close_fd:
    close(fd);
    return ret;
}