// SPDX-License-Identifier: GPL-2.0-or-later
// Telescope Project
// Looking Glass Proxy (LGProxy)
// Copyright (c) 2022 - 2024, Telescope Project Developers

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

#include "nfr_protocol.h"

#include <atomic>
#include <memory>
#include <unordered_map>
#include <vector>

#include "lp_log.h"
#include "lp_types.h"
#include "tcm_fabric.h"

// Cache line size
#if __cplusplus >= 201703L
#include <new>
#define LP_CL_SIZE std::hardware_destructive_interference_size
#else
#define LP_CL_SIZE 64
#endif

struct lp_comp_info {
  uint64_t _magic;

  // User-defined values
  uint8_t type;

  // Extra data buffer
  void * const extra;

  // Internal tracking data. May only be read outside of internal functions

  std::atomic_bool _used;
  const uint8_t    _index;
  const uint8_t    _buftag;

  // Start of the fabric context, if any was allocated. When using the
  // lp_comp_info struct as the context for TCM data transfer functions, fctx
  // must be passed, not the start of the structure. This applies even if the
  // provider in use does not require FI_CONTEXT, in which case there would be
  // no allocated memory past this point and the fabric does not write anything.
  alignas(LP_CL_SIZE) fi_context fctx[0];
};

struct lp_mbuf_alloc {
  size_t         offset;
  size_t         slot_size;
  size_t         extra_size;
  uint8_t        slots;
  void *         extra;
  lp_comp_info * comps;
  lp_mbuf_alloc() {
    offset    = 0;
    slot_size = 0;
    slots     = 0;
    comps     = 0;
    extra     = 0;
  }
  ~lp_mbuf_alloc() {
    free(comps);
    free(extra);
    comps = 0;
  }
};

struct lp_rbuf {

  uint64_t              base;
  uint64_t              rkey;
  uint64_t              maxlen;
  uint64_t              size;
  fi_addr_t             peer;
  std::vector<uint64_t> offsets;
  std::vector<uint8_t>  used;

  NFROffset operator[](int idx);

  void clear();

  bool import(NFRClientFrameBuf * b, uint32_t size, fi_addr_t peer);
  bool import(NFRClientCursorBuf * b, uint32_t size, fi_addr_t peer);

  int  lock(int index = -1);
  void unlock(int index);
  bool reclaim(int8_t * indexes, int n);
  void unlock_all();

  uint8_t avail();

  tcm_remote_mem export_rmem();

  ~lp_rbuf();
};

class lp_mbuf {

  std::shared_ptr<tcm_fabric>                fabric;
  std::unique_ptr<tcm_mem>                   mem;
  std::unordered_map<uint8_t, lp_mbuf_alloc> map;

public:
  /**
   * @brief Allocate a message buffer accessible by the RDMA device.
   *
   * @param fabric Fabric object to bind this buffer to
   * @param size   Size of the buffer
   */
  lp_mbuf(std::shared_ptr<tcm_fabric> & fabric, size_t size);

  /**
   * @brief Allocate a slice of memory within the buffer.
   *
   * This allocation function allocates a subregion of memory for sending and
   * receiving messages, in addition to allocating memory to store completion
   * information.
   *
   * @param tag         Tag used to identify the buffer
   * @param slot_size   Size of individual message slots
   * @param slots       Total number of message slots
   *
   * @return            Whether the allocation succeeded
   */
  bool alloc(uint8_t tag, size_t slot_size, uint8_t slots);

  /**
   * @brief Allocate pointers to store extra data.
   *
   * lp_comp_info structures contain a field ``extra``, which can be used to
   * store arbitrary user-defined data. By default, this is not allocated, and
   * this function can be used to do so.
   *
   * @param tag         Buffer tag, must have previously been alloc()'d
   * @param slot_size   Per-buffer-slot extra data length.
   * @param alignment   Optional memory buffer alignment. This only applies to
   * the start of the buffer; to guarantee alignment for all slots, slot_size
   * must be a multiple of alignment.
   * @return            Whether the allocation succeeded.
   */
  bool alloc_extra(uint8_t tag, size_t slot_size, size_t alignment = 0);

  /**
   * @brief Free extra data.
   *
   * If extra data was previously allocated for a subregion using alloc_extra(),
   * it can be freed here.
   *
   * @param tag Buffer tag
   */
  void free_extra(uint8_t tag);

  /**
   * @brief Get a completion context associated with a specific tag/slot.
   *
   * Note that when used with libfabric functions the member ->fctx must be used
   * as the context parameter, NOT the returned structure!
   *
   * @param tag   Buffer tag
   * @param slot  Slot index
   *
   * @return lp_comp_info*
   */
  lp_comp_info * get_context(uint8_t tag, uint8_t slot);

  /**
   * @brief Return the context structure associated with a completion.
   *
   * The member ->fctx is offset relative to the start of the structure, and the
   * exact offset varies depending on the system. This function subtracts the
   * offset from the provided pointer and returns the start of the completion
   * structure.
   *
   * @param op_context  fi_cq_*_entry->op_context struct member
   * @return            lp_comp_info*
   */
  lp_comp_info * get_context(void * op_context);

  /**
   * @brief Get a pointer to the buffer associated with a tag and slot index.
   *
   * @param tag   Buffer tag
   * @param slot  Slot index
   * @return      void*
   */
  void * get_buffer(uint8_t tag, uint8_t slot);

  /**
   * @brief Get the offset between the start of the full memory region (not the
   * subregion identified by the tag) and the buffer identified by a tag and
   * slot index.
   *
   * @param tag   Buffer tag
   * @param slot  Slot index
   * @return      uint64_t
   */
  uint64_t get_offset(uint8_t tag, uint8_t slot);

  /**
   * @brief Get the offset between the start of the full memory region
   * identified by a completion context.
   *
   * @param comp  Completion structure
   * @return      uint64_t
   */
  uint64_t get_offset(lp_comp_info * comp);

  /**
   * @brief Get the number of bytes available per message slot for a given
   * buffer tag.
   *
   * @param tag   Buffer tag
   * @return uint64_t
   */
  uint64_t get_slot_size(uint8_t tag);

  /**
   * @brief Get the total number of available slots for a given buffer tag.
   *
   * @param tag   Buffer tag
   * @return uint8_t
   */
  uint8_t get_slot_count(uint8_t tag);

  /**
   * @brief Return the buffer tag associated with the completion.
   *
   * @param comp  Completion structure
   * @return uint8_t
   */
  uint8_t get_tag(lp_comp_info * comp);

  /**
   * @brief Return the raw message buffer associated with the completion.
   *
   * @param comp  Completion structure
   * @return      void*
   */
  void * get_buffer(lp_comp_info * comp);

  /**
   * @brief Reserve a message buffer slot.
   *
   * @param tag   Buffer tag
   * @param slot  Slot index
   * @return      The index of the locked buffer slot, or -1 if none are free
   */
  int16_t lock(uint8_t tag, int16_t slot = -1);

  /**
   * @brief Unlock a previously reserved message buffer slot.
   *
   * @param tag   Buffer tag
   * @param slot  Slot index
   */
  void unlock(uint8_t tag, uint8_t slot);

  /**
   * @brief Unlock a previously reserved message buffer slot.
   *
   * @param comp  Completion structure
   */
  void unlock(lp_comp_info * comp);

  /**
   * @brief Get the underlying memory object.
   *
   * This is designed for ephemeral use (e.g. in short function calls) only!
   *
   * @return tcm_mem*
   */
  tcm_mem * get_mem();

  ~lp_mbuf();
};

class lp_shmem {

  char *   path;
  int      fd;
  int      dma_fd;
  void *   mem;
  uint64_t mem_size;
  bool     hp;

  void clear_fields();

public:
  lp_shmem(const char * path, size_t size, bool hugepage = false);
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