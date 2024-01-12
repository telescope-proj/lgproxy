// SPDX-License-Identifier: GPL-2.0-or-later

#include "lp_mem.h"

extern "C" {
#include "module/kvmfr.h"
}

// This is a random number, used to detect context corruption or invalid access.
// Only used in debugging.

#define CTX_DBG_NUM 6444909448273569323

// Remote message buffer handling

NFROffset lp_rbuf::operator[](int idx) { return this->offsets.at(idx); }

void lp_rbuf::clear() {
  base   = 0;
  rkey   = 0;
  maxlen = 0;
  offsets.clear();
  used.clear();
  peer = FI_ADDR_UNSPEC;
}

bool lp_rbuf::import(NFRClientFrameBuf * b, uint32_t size, fi_addr_t peer) {
  int n_off = (size - sizeof(*b)) / sizeof(NFROffset);
  if (n_off <= 0 || n_off >= 127) {
    lp_log_error("Malformed frame buffer message");
    return false;
  }
  uint64_t max_off = 0;
  this->offsets.reserve(n_off);
  this->used.reserve(n_off);
  this->base   = b->base;
  this->rkey   = b->key;
  this->maxlen = b->maxlen;
  this->peer   = peer;
  lp_log_debug("Importing remote frame buffers, base %p, key %lu, maxlen: %lu",
               this->base, this->rkey, this->maxlen);
  for (int i = 0; i < n_off; i++) {
    lp_log_debug("%s F Region %3d : %9lu -- %-9lu", i == n_off - 1 ? "└" : "├",
                 i, b->offsets[i], b->offsets[i] + this->maxlen);
    if (b->offsets[i] > max_off)
      max_off = b->offsets[i];
    this->offsets.push_back(b->offsets[i]);
    this->used.push_back(0);
  }
  this->size = max_off + b->maxlen;
  return true;
}

bool lp_rbuf::import(NFRClientCursorBuf * b, uint32_t size, fi_addr_t peer) {
  int n_off = (size - sizeof(*b)) / sizeof(NFROffset);
  if (n_off <= 0 || n_off >= 127) {
    lp_log_error("Malformed cursor buffer message");
    return false;
  }
  uint64_t max_off = 0;
  this->offsets.reserve(n_off);
  this->used.reserve(n_off);
  this->base   = b->base;
  this->rkey   = b->key;
  this->maxlen = b->maxlen;
  this->peer   = peer;
  lp_log_debug("Importing remote cursor buffers, base %p, key %lu, maxlen: %lu",
               this->base, this->rkey, this->maxlen);
  for (int i = 0; i < n_off; ++i) {
    lp_log_debug("%s C Region %3d : %9lu -- %-9lu", i == n_off - 1 ? "└" : "├",
                 i, b->offsets[i], b->offsets[i] + this->maxlen);
    if (b->offsets[i] > max_off)
      max_off = b->offsets[i];
    this->offsets.push_back(b->offsets[i]);
    this->used.push_back(0);
  }
  this->size = max_off + b->maxlen;
  return true;
}

tcm_remote_mem lp_rbuf::export_rmem() {
  tcm_remote_mem rmem;
  rmem.peer   = this->peer;
  rmem.addr   = this->base;
  rmem.len    = this->size;
  rmem.raw    = false;
  rmem.u.rkey = this->rkey;
  return rmem;
}

int lp_rbuf::lock(int index) {
  if (index < 0) {
    for (uint32_t i = 0; i < offsets.size(); ++i) {
      if (!used.at(i)) {
        used.at(i) = 1;
        return i;
      }
    }
    return -1;
  }

  if (!used.at(index)) {
    used.at(index) = 1;
    return index;
  }
  return -1;
}

void lp_rbuf::unlock(int index) {
  if (used.at(index) == 0)
    lp_log_debug("Already-unlocked buffer was unlocked again");
  used.at(index) = 0;
}

void lp_rbuf::unlock_all() {
  for (uint32_t i = 0; i < used.size(); ++i) {
    used.at(i) = 0;
  }
}

bool lp_rbuf::reclaim(int8_t * indexes, int n) {
  if (n < 0)
    return false;
  for (uint32_t i = 0; i < (uint32_t) n; ++i) {
    if (indexes[i] < 0)
      continue;
    if (indexes[i] > (int8_t) this->offsets.size()) {
      lp_log_error("Index %d exceeds %d provided buffers", indexes[i],
                   this->offsets.size());
      return false;
    }
    if (this->used.at(indexes[i]) == 0)
      lp_log_debug("Double-free of remote buffer %d", indexes[i]);
    this->used[i] = 0;
  }
  return true;
}

uint8_t lp_rbuf::avail() {
  uint8_t ret = 0;
  for (uint8_t i = 0; i < this->used.size(); ++i) {
    if (!this->used[i])
      ret++;
  }
  return ret;
}

lp_rbuf::~lp_rbuf() {
  this->offsets.clear();
  this->used.clear();
}

// Message buffer handling

lp_mbuf::lp_mbuf(std::shared_ptr<tcm_fabric> & fabric, size_t size) {
  void * buf = tcm_mem_align_rdma(size);
  if (!buf)
    throw tcm_exception(ENOMEM, _LP_FNAME_, __LINE__,
                        "Memory allocation failed");

  this->fabric = fabric;
  this->mem.reset(new tcm_mem(fabric, buf, size, TCM_MEM_PLAIN_ALIGNED));
  this->map.clear();
}

lp_mbuf::~lp_mbuf() {
  this->fabric.reset();
  this->mem.reset();
  this->map.clear();
}

uint64_t lp_mbuf::get_slot_size(uint8_t tag) {
  lp_mbuf_alloc & tmp = this->map.at(tag);
  return tmp.slot_size;
}

uint8_t lp_mbuf::get_slot_count(uint8_t tag) {
  lp_mbuf_alloc & tmp = this->map.at(tag);
  return tmp.slots;
}

uint8_t lp_mbuf::get_tag(lp_comp_info * comp) {
  assert(comp->_magic == CTX_DBG_NUM);
  return comp->_buftag;
}

bool lp_mbuf::alloc(uint8_t tag, size_t slot_size, uint8_t slots) {
  size_t next_free = 0;
  size_t len       = slot_size * slots;
  if (slot_size) {
    lp_log_debug("Allocating buffers; tag %d, size %lu, slots %d", tag,
                 slot_size, slots);
    for (auto it = this->map.begin(); it != this->map.end(); ++it) {
      lp_mbuf_alloc & mbuf = it->second;
      size_t          end  = mbuf.offset + mbuf.slot_size * mbuf.slots;
      if (end > next_free)
        next_free = end;
    }
    if (next_free + len > this->mem->get_len())
      return false;
  } else {
    lp_log_debug("Allocating buffers (completions only); tag %d, slots %d", tag,
                 slots);
  }

  uint64_t        mode = FI_CONTEXT2; // Worst case
  const fi_info * info =
      (const fi_info *) this->fabric->_get_fi_resource(TCM_RESRC_PARAM);
  if (info)
    mode = info->mode;

  lp_mbuf_alloc mbuf;
  this->map[tag]      = mbuf;
  lp_mbuf_alloc & tmp = this->map[tag];
  tmp.offset          = next_free;
  tmp.slot_size       = slot_size;
  tmp.slots           = slots;
  tmp.comps           = 0;
  size_t comp_size    = sizeof(*tmp.comps);
  if (mode & FI_CONTEXT2)
    comp_size += sizeof(fi_context2);
  else if (mode & FI_CONTEXT)
    comp_size += sizeof(fi_context);
  comp_size += (LP_CL_SIZE - (comp_size % LP_CL_SIZE));
  lp_log_debug("Completion entry size %lu, cache line size: %lu", comp_size,
               LP_CL_SIZE);
  posix_memalign((void **) &tmp.comps, LP_CL_SIZE, comp_size * slots);
  if (!tmp.comps) {
    this->map.erase(tag);
    return false;
  }
  for (uint8_t slot = 0; slot < slots; ++slot) {
    tmp.comps[slot]._magic                         = CTX_DBG_NUM;
    const_cast<uint8_t &>(tmp.comps[slot]._buftag) = tag;
    const_cast<uint8_t &>(tmp.comps[slot]._index)  = slot;
    tmp.comps[slot]._used.store(false, std::memory_order_relaxed);
    const_cast<void *&>(tmp.comps[slot].extra) = 0;
  }
  return true;
}

bool lp_mbuf::alloc_extra(uint8_t tag, size_t slot_size, size_t alignment) {
  lp_mbuf_alloc & tmp = this->map.at(tag);

  if (alignment)
    tmp.extra = tcm_mem_align(tmp.slots * slot_size, alignment);
  else
    tmp.extra = calloc(tmp.slots, slot_size);

  if (!tmp.extra)
    return false;

  for (size_t i = 0; i < tmp.slots; ++i) {
    void *& extra = const_cast<void *&>(tmp.comps[i].extra);
    extra         = (uint8_t *) tmp.extra + slot_size * i;
  }
  return true;
}

void lp_mbuf::free_extra(uint8_t tag) {
  lp_mbuf_alloc & tmp = this->map.at(tag);
  free(tmp.extra);
  tmp.extra = 0;
  for (size_t i = 0; i < tmp.slots; ++i) {
    const_cast<void *&>(tmp.comps[i].extra) = 0;
  }
}

lp_comp_info * lp_mbuf::get_context(uint8_t tag, uint8_t slot) {
  lp_mbuf_alloc & buf = this->map.at(tag);
  if (slot >= buf.slots)
    throw tcm_exception(EINVAL, _LP_FNAME_, __LINE__, "Out of bounds access");
  return &buf.comps[slot];
}

lp_comp_info * lp_mbuf::get_context(void * op_ctx) {
  lp_comp_info * info =
      (lp_comp_info *) ((uint8_t *) op_ctx - offsetof(lp_comp_info, fctx));
  assert(info->_magic == CTX_DBG_NUM);
  return info;
}

void * lp_mbuf::get_buffer(uint8_t tag, uint8_t slot) {
  lp_mbuf_alloc & buf = this->map.at(tag);
  if (slot >= buf.slots) {
    std::stringstream err;
    err << "Out of bounds access for tag " << std::to_string(tag) << "["
        << std::to_string(slot)
        << "], slots: " << std::to_string(this->get_slot_count(tag));
    throw tcm_exception(EINVAL, err, _LP_FNAME_, __LINE__);
  }
  return mem->offset(buf.offset + buf.slot_size * slot);
}

uint64_t lp_mbuf::get_offset(uint8_t tag, uint8_t slot) {
  return (uint64_t) ((uintptr_t) this->get_buffer(tag, slot) -
                     (uintptr_t) this->mem->get_ptr());
}

uint64_t lp_mbuf::get_offset(lp_comp_info * comp) {
  assert(comp->_magic == CTX_DBG_NUM);
  return (uint64_t) ((uintptr_t) this->get_buffer(comp) -
                     (uintptr_t) this->mem->get_ptr());
}

void * lp_mbuf::get_buffer(lp_comp_info * comp) {
  assert(comp->_magic == CTX_DBG_NUM);
  lp_mbuf_alloc & buf = this->map.at(comp->_buftag);
  assert(comp->_index <= buf.slots);
  return mem->offset(buf.offset + buf.slot_size * comp->_index);
}

int16_t lp_mbuf::lock(uint8_t tag, int16_t slot) {
  lp_mbuf_alloc & buf = this->map.at(tag);
  if (slot < 0) {
    for (uint8_t i = 0; i < buf.slots; ++i) {
      bool a = 0, b = 1;
      if (buf.comps[i]._used.compare_exchange_strong(
              a, b, std::memory_order_acquire, std::memory_order_relaxed)) {
        return (uint8_t) i;
      }
    }
    return -EAGAIN;
  } else {
    if (slot >= buf.slots)
      throw tcm_exception(EINVAL, _LP_FNAME_, __LINE__, "Out of bounds access");
    bool a = 0, b = 1;
    if (buf.comps[slot]._used.compare_exchange_strong(
            a, b, std::memory_order_acquire, std::memory_order_relaxed)) {
      lp_log_trace("Locking buffer %d", slot);
      return (uint8_t) slot;
    }
    return -EAGAIN;
  }
}

void lp_mbuf::unlock(uint8_t tag, uint8_t slot) {
  lp_mbuf_alloc & buf = this->map.at(tag);
  if (slot >= buf.slots)
    throw tcm_exception(EINVAL, _LP_FNAME_, __LINE__, "Out of bounds access");
  bool a = false;
  buf.comps[slot]._used.exchange(a, std::memory_order_release);
}

void lp_mbuf::unlock(lp_comp_info * comp) {
  assert(comp->_magic == CTX_DBG_NUM);
  lp_mbuf_alloc & buf = this->map.at(comp->_buftag);
  assert(comp->_index <= buf.slots);
  bool a = false;
  buf.comps[comp->_index]._used.exchange(a, std::memory_order_release);
}

tcm_mem * lp_mbuf::get_mem() { return this->mem.get(); }

// RDMA-enabled buffer container

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
      throw tcm_exception(errno, _LP_FNAME_, __LINE__,
                          "KVMFR file open failed");
    }
    tmp_s = ioctl(this->fd, KVMFR_DMABUF_GETSIZE, 0);
    if (tmp_s <= 0) {
      if (!size) {
        this->~lp_shmem();
        throw tcm_exception(EINVAL, _LP_FNAME_, __LINE__,
                            "DMABUF creation failed");
      }
      const struct kvmfr_dmabuf_create create = {
          .flags = KVMFR_DMABUF_FLAG_CLOEXEC, .offset = 0, .size = size};
      this->dma_fd = ioctl(this->fd, KVMFR_DMABUF_CREATE, &create);
      if (this->dma_fd < 0) {
        this->~lp_shmem();
        throw tcm_exception(errno, _LP_FNAME_, __LINE__,
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
          throw tcm_exception(errno, _LP_FNAME_, __LINE__,
                              "Requested file nonexistent");
        }
      } else {
        this->~lp_shmem();
        throw tcm_exception(errno, _LP_FNAME_, __LINE__,
                            "Could not get requested file details");
      }
    }
    this->fd = open(path_, O_RDWR | O_CREAT, (mode_t) 0600);
    if (this->fd < 0) {
      this->~lp_shmem();
      throw tcm_exception(errno, _LP_FNAME_, __LINE__,
                          "Unable to open SHM file");
    }
    if (!exist) {
      if (ftruncate(this->fd, size) != 0) {
        this->~lp_shmem();
        throw tcm_exception(-errno, _LP_FNAME_, __LINE__,
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
    throw tcm_exception(errno, _LP_FNAME_, __LINE__,
                        "SHM file memory mapping failed");
  }

  this->path = strdup(path_);
  if (!this->path) {
    this->~lp_shmem();
    throw tcm_exception(ENOMEM, _LP_FNAME_, __LINE__,
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
