// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef LP_CONFIG_H_
#define LP_CONFIG_H_

#include <errno.h>
#include <getopt.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

class lp_sink_opts {

  void clear_fields();

public:
  const char * src_addr;
  const char * server_addr;
  const char * server_port;
  const char * file;
  const char * transport;
  uint32_t     fabric_version;
  int64_t      timeout;
  int64_t      interval;
  size_t       shm_len;

  lp_sink_opts(int argc, char ** argv);
  ~lp_sink_opts();
};

class lp_source_opts {

  void clear_fields();

public:
  const char * src_addr;
  const char * src_port;
  const char * file;
  const char * transport;
  uint32_t     fabric_version;
  uint16_t     data_port;
  uint16_t     frame_port;
  int64_t      timeout;
  int64_t      interval;

  lp_source_opts(int argc, char ** argv);
  ~lp_source_opts();
};

void lp_print_usage(int is_server);

#endif