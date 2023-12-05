// SPDX-License-Identifier: GPL-2.0-or-later

#include "lp_mem.h"

extern "C" {
#include "module/kvmfr.h"
}


lp_rdma_shmem::~lp_rdma_shmem() {
  this->fab = 0;
  this->shm = 0;
  this->mem = 0;
}

lp_rdma_shmem::lp_rdma_shmem(std::shared_ptr<tcm_fabric> fabric,
                             std::shared_ptr<lp_shmem>   shm) {
  this->fab = fabric;
  this->shm = shm;
  tcm__log_debug("Binding shared memory region to RDMA fabric");
  this->mem = std::make_shared<tcm_mem>(fabric, **shm, shm->get_size(),
                                        TCM_MEM_UNMANAGED);
  tcm__log_debug("Shared memory region %p bound to fabric %p", shm.get(),
                 fabric.get());
}

void lp_shmem::clear_fields() {
  this->path     = 0;
  this->fd       = -1;
  this->dma_fd   = -1;
  this->mem      = 0;
  this->mem_size = 0;
}

lp_shmem::lp_shmem(const char * path_, size_t size, bool hugepage) {
  this->clear_fields();
  int tmp_s;
  int map_flags = hugepage ? MAP_SHARED | MAP_HUGETLB : MAP_SHARED;

  lp_log_trace("Opening SHM file %s", path_);
  if (strncmp(path_, "/dev/kvmfr", 10) == 0) {
    this->fd = open(path_, O_RDWR, (mode_t) 0600);
    if (this->fd < 0) {
      this->~lp_shmem();
      throw tcm_exception(errno, __FILE__, __LINE__, "KVMFR file open failed");
    }
    tmp_s = ioctl(this->fd, KVMFR_DMABUF_GETSIZE, 0);
    if (tmp_s <= 0) {
      if (!size) {
        this->~lp_shmem();
        throw tcm_exception(EINVAL, __FILE__, __LINE__,
                            "DMABUF creation failed");
      }
      const struct kvmfr_dmabuf_create create = {
          .flags = KVMFR_DMABUF_FLAG_CLOEXEC, .offset = 0, .size = size};
      this->dma_fd = ioctl(this->fd, KVMFR_DMABUF_CREATE, &create);
      if (this->dma_fd < 0) {
        this->~lp_shmem();
        throw tcm_exception(errno, __FILE__, __LINE__,
                            "KVMFR DMABUF creation failed");
      }
      this->mem_size = size;
    } else {
      this->mem_size = (uint64_t) tmp_s;
    }
    lp_log_debug("Using DMABUF file: %s, size: %lu", path_, this->mem_size);
  } else {
    struct stat st;
    int         ret   = stat(path_, &st);
    int         exist = 1;
    if (ret < 0) {
      if (errno == ENOENT) {
        lp_log_debug("File %s does not exist, creating it", path_);
        exist = 0;
        if (!size) {
          this->~lp_shmem();
          throw tcm_exception(errno, __FILE__, __LINE__,
                              "Requested file nonexistent");
        }
      } else {
        this->~lp_shmem();
        throw tcm_exception(errno, __FILE__, __LINE__,
                            "Could not get requested file details");
      }
    }
    this->fd = open(path_, O_RDWR | O_CREAT, (mode_t) 0600);
    if (this->fd < 0) {
      this->~lp_shmem();
      throw tcm_exception(errno, __FILE__, __LINE__, "Unable to open SHM file");
    }
    if (!exist) {
      if (ftruncate(this->fd, size) != 0) {
        this->~lp_shmem();
        throw tcm_exception(-errno, __FILE__, __LINE__,
                            "Unable to resize SHM file");
      }
      this->mem_size = size;
    } else {
      this->mem_size = (uint64_t) st.st_size;
    }
  }

  this->mem = mmap(0, this->mem_size, PROT_READ | PROT_WRITE, map_flags,
                   this->dma_fd > 0 ? this->dma_fd : fd, 0);
  if (this->mem == MAP_FAILED) {
    this->~lp_shmem();
    throw tcm_exception(errno, __FILE__, __LINE__,
                        "SHM file memory mapping failed");
  }

  this->path = strdup(path_);
  if (!this->path) {
    this->~lp_shmem();
    throw tcm_exception(ENOMEM, __FILE__, __LINE__,
                        "Unable to copy path string");
  }

  lp_log_debug("Shared memory file: %s, size: %lu opened", path_,
               this->mem_size);
  return;
}

lp_shmem::~lp_shmem() {
  if (this->mem && this->mem != MAP_FAILED && this->mem_size) {
    munmap(this->mem, this->mem_size);
    this->mem      = 0;
    this->mem_size = 0;
  }
  if (this->dma_fd > 0) {
    close(this->dma_fd);
    this->dma_fd = -1;
  }
  if (this->fd > 0) {
    close(this->fd);
    this->fd = -1;
  }
  free(this->path);
  this->path = 0;
}
