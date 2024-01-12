// SPDX-License-Identifier: GPL-2.0-or-later
// Telescope Project
// Looking Glass Proxy (LGProxy)
// Copyright (c) 2022 - 2024, Telescope Project Developers

#ifndef LP_METADATA_H_
#define LP_METADATA_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>

extern "C" {
#include "common/KVMFR.h"
#include "lgmp/client.h"
#include "lgmp/host.h"
#include "lgmp/lgmp.h"
}

#include "lp_log.h"
#include "lp_types.h"
#include "nfr_protocol.h"
#include "tcm_exception.h"

#define LP_NAME_MAX 1024
#define LP_OS_NAME_MAX 1024
#define LP_CPU_NAME_MAX 1024
#define LP_CAPTURE_METHOD_MAX 31

struct lp_metadata {

  // KVMFR/LGMP basic information

  uint32_t    width;
  uint32_t    height;
  uint32_t    stride;
  uint32_t    pitch;
  uint16_t    sockets;
  uint16_t    threads;
  uint16_t    cores;
  uint16_t    fr_flags;
  uint16_t    fr_os;
  uint16_t    fr_format;
  uint8_t     fr_rotation;
  uint8_t     nfr_flags;
  uint8_t     proxied;
  char        fr_uuid[16];
  std::string fr_capture;
  std::string cpu_model;
  std::string os_name;

  // Extended information for LGProxy

  uint64_t    x_link_rate;
  std::string x_sys_name;

  void clear();
  int export_udata(void * udata, size_t * size, uint32_t feature_flags);
  void import_nfr(NFRHostMetadata * msg, size_t size);
  
};

#endif