// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef LP_MEM_H_
#define LP_MEM_H_

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <memory>

#include "lp_log.h"
#include "tcm_fabric.h"

class lp_shmem {

  char *   path;
  int      fd;
  int      dma_fd;
  void *   mem;
  uint64_t mem_size;
  bool     hp;

  void clear_fields();

public:
  lp_shmem(const char * path, size_t size, bool hugepage);
  ~lp_shmem();

  void * operator*() { return this->mem; }

  uint8_t  is_dmabuf() { return this->dma_fd > 0; }
  void *   get_ptr() { return this->mem; }
  uint64_t get_size() { return this->mem_size; }
  bool     hp_enabled() { return this->hp; }
};

class lp_rdma_shmem {

public:
  std::shared_ptr<lp_shmem>   shm;
  std::shared_ptr<tcm_fabric> fab;
  std::shared_ptr<tcm_mem>    mem;

  lp_rdma_shmem(std::shared_ptr<tcm_fabric> fab, std::shared_ptr<lp_shmem> shm);
  ~lp_rdma_shmem();
};

#endif