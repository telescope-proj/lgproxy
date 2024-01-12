// SPDX-License-Identifier: GPL-2.0-or-later
// Telescope Project
// Looking Glass Proxy (LGProxy)
// Copyright (c) 2022 - 2024, Telescope Project Developers

#include "lp_config.h"
#include "lg_build_version.h"
#include "lp_build_version.h"
#include "lp_exception.h"
#include "lp_log.h"
#include "lp_utils.h"
#include "lp_version.h"

#include "tcm_fabric.h"

#include <arpa/inet.h>
#include <errno.h>
#include <string.h>

#include <rdma/fabric.h>

extern "C" {
#include "common/KVMFR.h"
}

#define HIGHLIGHT(x) "\033[0;33m" x "\033[0;0m"

/* ---- Helper macros ---- */

#define CHECK_NZ(x, y)                                                         \
  do {                                                                         \
    if (!x) {                                                                  \
      lp_log_error("Invalid value for " y);                                    \
      goto cleanup;                                                            \
    }                                                                          \
  } while (0);

/* ---- LGProxy Options ---- */

// clang-format off

enum {
    OPTIONAL,            /* Optional (flag) */
    OPTIONAL_WITH_VALUE, /* Optional with required value */
    REQUIRED_WITH_VALUE  /* Required with value */
};

const char * req_opt_str[] = {
    "Optional flag",
    "Optional argument with value",
    "Required argument with value"
};

struct lp_optdesc {
    int          id;
    int          required;
    const char * help_str;
    const char * example;
    const char * default_val;
};

static struct option sink_opts[] = {
    {"server",      required_argument, NULL, 's'},
    {"port",        required_argument, NULL, 'p'},
    {"nic",         required_argument, NULL, 'n'},
    {"version",     required_argument, NULL, 'v'},
    {"backend",     required_argument, NULL, 'b'},
    {"mem",         required_argument, NULL, 'm'},
    {"mem_size",    required_argument, NULL, 'l'},
    {"interval",    required_argument, NULL, 'i'},
    {"timeout",     required_argument, NULL, 't'},
    {"help",        no_argument,       NULL, 'h'},
    {0, 0, 0, 0}
};

static struct lp_optdesc sink_opts_desc[] = {
  {'s',
    REQUIRED_WITH_VALUE,
    "Server hostname",
    "192.168.1.200",
    NULL
  },
  {'p',
    REQUIRED_WITH_VALUE,
    "Server port",
    "20000",
    NULL
  },
  {'n',
    REQUIRED_WITH_VALUE,
    "Local network interface address",
    "192.168.1.200", 
    NULL
  },
  {'v',
    OPTIONAL_WITH_VALUE,
    "Libfabric API version (" HIGHLIGHT("must match on source/sink") ")",
    "major.minor: 1.10",
    "1.10"
  },
  {'b',
    OPTIONAL_WITH_VALUE,
    "Libfabric transport backend (" HIGHLIGHT("must match on source/sink") ")",
    "A transport type supported by TCM",
    "rdma"
  },
  {'m',
    OPTIONAL_WITH_VALUE,
    "Shared memory or KVMFR file path.", 
    "File path: /dev/shm/... or /dev/kvmfr...",
    "/dev/shm/looking-glass"
  },
  {'l',
    OPTIONAL_WITH_VALUE,
    "Shared memory or KVMFR file size in bytes (ignored if file exists)",
    "Integer followed by optional unit: 128M (k=KiB, m=MiB, g=GiB)", 
    "32M"
  },
  {'i',
    OPTIONAL_WITH_VALUE,
    "Data polling interval",
    "Integer followed by optional unit: 1u (u=µs, m=ms (default), s=sec)",
    "0"
  },
  {'t',
    OPTIONAL_WITH_VALUE,
    "Timeout",
    "Integer followed by optional unit: 1m (m=ms (default), s=sec)",
    "3s"
  },
  {'h',
    OPTIONAL,
    "Display help message",
    NULL,
    NULL
  },
  {0, 0, 0, 0, 0}
};

static struct option source_opts[] = {
    {"nic",         required_argument, NULL, 'n'},
    {"port",        required_argument, NULL, 'p'},
    {"version",     required_argument, NULL, 'v'},
    {"backend",     required_argument, NULL, 'b'},
    {"mem",         required_argument, NULL, 'm'},
    {"interval",    required_argument, NULL, 'i'},
    {"timeout",     required_argument, NULL, 't'},
    {"once",        no_argument,       NULL, 'o'},
    {"profiler",    no_argument,       NULL, 'd'},
    {"help",        no_argument,       NULL, 'h'},
    {0, 0, 0, 0}
};

static struct lp_optdesc source_opts_desc[] = {
  {'n',
    REQUIRED_WITH_VALUE,
    "Local network interface address",
    "192.168.1.200", 
    NULL
  },
  {'p',
    REQUIRED_WITH_VALUE,
    "Local port",
    "10000", 
    NULL
  },
  {'v',
    OPTIONAL_WITH_VALUE,
    "Libfabric API version (" HIGHLIGHT("must match on source/sink") ")",
    "major.minor: 1.10",
    "1.10"
  },
  {'b',
    OPTIONAL_WITH_VALUE,
    "Libfabric transport backend (" HIGHLIGHT("must match on source/sink") ")",
    "A transport type supported by TCM",
    "rdma"
  },
  {'m',
    OPTIONAL_WITH_VALUE,
    "Shared memory or KVMFR file path.",
    "/dev/shm/* or /dev/kvmfr*",
    "/dev/shm/looking-glass"
  },
  {'i',
    OPTIONAL_WITH_VALUE,
    "Data polling interval",
    "Integer followed by optional unit: 1u (u=µs, m=ms (default), s=sec)",
    "0"
  },
  {'t',
    OPTIONAL_WITH_VALUE,
    "Timeout before dropping client",
    "Integer followed by optional unit: 1m (m=ms (default), s=sec)",
    "3s"
  },
  {'o',
    OPTIONAL,
    "Accept a single client and exit after it disconnects",
    NULL,
    NULL
  },
  {'d',
    OPTIONAL,
    "Enable the Telescope Profiler for debugging",
    NULL,
    NULL
  },
  {'h',
    OPTIONAL,
    "Display help message",
    NULL,
    NULL
  },
  {0, 0, 0, 0, 0}
};

// clang-format on

/* ---- Helper functions ---- */

char * long_to_short_opt_string(struct option * long_opts) {
  char * out = (char *) calloc(1, 64);
  if (!out) {
    errno = ENOMEM;
    return 0;
  }
  char * tmp = out;
  int    i   = 0;
  while (1) {
    if (long_opts[i].has_arg == no_argument) {
      snprintf(tmp, 2, "%c", (char) long_opts[i].val);
      tmp++;
    } else {
      snprintf(tmp, 3, "%c:", (char) long_opts[i].val);
      tmp += 2;
    }
    i++;

    /* The short option string should not be this long, probably a bug */
    if (tmp - out > 63) {
      errno = ENOBUFS;
      free(out);
      return 0;
    }
    if (long_opts[i].name == 0)
      break;
  }
  return out;
}

void lp_print_usage(int source) {
  struct option *     opts = source ? source_opts : sink_opts;
  struct lp_optdesc * desc = source ? source_opts_desc : sink_opts_desc;
  uint64_t            ver  = tcm_get_version();
  printf("LGProxy v%d.%d.%d-%s %s\n"
         "TCM %lu.%lu.%lu; Looking Glass %s; KVMFR %d\n"
         "Copyright (c) 2024 Tim Dettmar\n"
         "\n"
         "Documentation & software licenses:\n"
         "    https://telescope-proj.github.io/lgproxy\n"
         "\n"
         "Available options:\n\n",
         LP_VERSION_MAJOR, LP_VERSION_MINOR, LP_VERSION_PATCH, LP_BUILD_VERSION,
         source ? "(server)" : "(client)", TCM_MAJOR(ver), TCM_MINOR(ver),
         TCM_PATCH(ver), LG_BUILD_VERSION, KVMFR_VERSION);

  for (int i = 0; opts[i].val; i++) {
    for (int j = 0; desc[j].id; j++) {
      if (desc[j].id == opts[i].val) {
        char req_arg[64];
        switch (desc[i].required) {
          case OPTIONAL_WITH_VALUE:
            strcpy(req_arg, "(optional, value required)");
            break;
          case REQUIRED_WITH_VALUE:
            strcpy(req_arg, "(" HIGHLIGHT("required") ", value required)");
            break;
          case OPTIONAL: strcpy(req_arg, "(optional flag)"); break;
          default: LP_THROW(EIO, "Internal lookup table error");
        }
        printf("-%c, --%s %s\n", opts[i].val, opts[i].name, req_arg);
        printf("\t%s\n", desc[j].help_str);
        if (desc[j].example)
          printf("\tExample: %s\n", desc[j].example);
        if (desc[j].default_val)
          printf("\tDefault: %s\n", desc[j].default_val);
        printf("\n");
      }
    }
  }
}

/* Convert a memory size string (e.g. 128M) to the actual size in bytes.
   Unitless input is considered
 */
uint64_t parse_mem_string(char * data) {
  char     multiplier = data[strlen(data) - 1];
  uint64_t b          = atoi(data);
  int      val;
  char     unit;
  int      ret = sscanf(data, "%d%c", &val, &unit);
  if (ret < 0 || val <= 0) {
    lp_log_error("Malformed memory size input");
    return -EINVAL;
  }
  if (multiplier >= '0' && multiplier <= '9') {
    return b;
  } else {
    switch (multiplier) {
      case 'K':
      case 'k': /* Kibibytes */ return b * 1024;
      case 'M':
      case 'm': /* Mebibytes */ return b * 1024 * 1024;
      case 'G':
      case 'g': /* Gibibytes */ return b * 1024 * 1024 * 1024;
      default: return 0;
    }
  }
}

/* Convert a polling interval string (e.g. 10u) to the actual interval in
 * microseconds. By default, if the user does not provide a unit, the parser
 * will assume the user specified milliseconds (LGProxy < 0.2.1 behaviour)
 */
int64_t parse_interval_string(char * data) {
  int  val;
  char unit;
  int  ret = sscanf(data, "%d%c", &val, &unit);
  if (ret < 0 || val < 0) {
    lp_log_error("Malformed interval input");
    LP_THROW(EINVAL, "Malformed interval input");
  }
  switch (unit) {
    case 'U':
    case 'u': /* Microseconds */ return val;
    case 0:
    case 'M':
    case 'm': /* Milliseconds */ return val * 1000;
    case 'S':
    case 's': /* Seconds */ return val * 1000000;
    default:
      lp_log_error("Unknown delay unit %c", unit);
      LP_THROW(EINVAL, "Malformed interval input");
  }
}

/* Convert a major.minor version number string into the Libfabric version */
uint32_t parse_version_string(char * data) {
  uint32_t cur_ver = fi_version();

  int maj, min;
  int ret = sscanf(data, "%d.%d", &maj, &min);
  if (ret < 0 || maj <= 0 || min <= 0) {
    lp_log_error("Malformed version input");
    lp_log_debug("%s -> %d %d", data, maj, min);
    errno = EINVAL;
    return 0;
  }

  if ((uint32_t) maj != FI_MAJOR(cur_ver) ||
      (uint32_t) min > FI_MINOR(cur_ver) ||
      (uint32_t) maj != FI_MAJOR(TCM_DEFAULT_FABRIC_VERSION) ||
      (uint32_t) min < FI_MINOR(TCM_DEFAULT_FABRIC_VERSION)) {
    lp_log_error("Requested Libfabric API version invalid");
    lp_log_error("System: %d.%d, Requested: %d.%d, Minimum: %d.%d",
                 FI_MAJOR(cur_ver), FI_MINOR(cur_ver), maj, min,
                 FI_MAJOR(TCM_DEFAULT_FABRIC_VERSION),
                 FI_MINOR(TCM_DEFAULT_FABRIC_VERSION));
    errno = EINVAL;
    return 0;
  }

  errno = 0;
  return FI_VERSION(maj, min);
}

void set_opts_flags(char * reqd, char * pass, struct option * opts,
                    lp_optdesc * desc, int n_opts) {
  for (int i = 0; i < n_opts; i++) {
    if (desc[i].id != opts[i].val) {
      lp_log_fatal("System error: invalid argument configuration");
      lp_log_fatal("This is a programming bug");
      LP_THROW(EINVAL, "Internal argument configuration error");
    }
    if (desc[i].required == REQUIRED_WITH_VALUE) {
      reqd[i] = 1;
    } else {
      reqd[i] = 0;
    }
    pass[i] = 0;
  }
}

int check_valid_opts(char * reqd, char * pass, struct option * opts,
                     lp_optdesc * desc, int n_opts) {
  for (int i = 0; i < n_opts; i++) {
    if (desc[i].required == REQUIRED_WITH_VALUE) {
      if (reqd[i] && !pass[i]) {
        lp_log_warn("Required argument -%c / --%s is missing", opts[i].val,
                    opts[i].name);
        return 0;
      }
    }
  }
  return 1;
}

/* ---- Sink options ---- */

void lp_sink_opts::clear_fields() {
  this->src_addr       = 0;
  this->server_addr    = 0;
  this->server_port    = 0;
  this->file           = 0;
  this->fabric_version = 0;
  this->timeout        = 0;
  this->interval       = 0;
  this->shm_len        = 0;
  this->transport      = 0;
}

lp_sink_opts::~lp_sink_opts() { this->clear_fields(); }

lp_sink_opts::lp_sink_opts(int argc, char ** argv) {
  this->clear_fields();
  int o, idx = -1;

  const int       n_opts = sizeof(sink_opts) / sizeof(struct option);
  lp_optdesc *    desc   = sink_opts_desc;
  struct option * opts   = sink_opts;
  char            arg_reqd[n_opts];
  char            arg_pass[n_opts];

  char * shortopts = long_to_short_opt_string(opts);
  if (!shortopts) {
    LP_THROW(ENOMEM, "Failed to generate option string");
  }

  /* Defaults */
  this->fabric_version = TCM_DEFAULT_FABRIC_VERSION;
  this->timeout        = 3000;
  this->interval       = 500;

  set_opts_flags(arg_reqd, arg_pass, opts, desc, n_opts);
  while ((o = getopt_long(argc, argv, shortopts, opts, &idx)) != -1) {
    if (idx >= 0) {
      o = opts[idx].val;
    }
    for (int i = 0; i < n_opts; i++) {
      if (desc[i].id == o) {
        arg_pass[i] = 1;
      }
    }
    idx = -1;
    switch (o) {
      case 's':
        lp_log_trace("Server address: %s", optarg);
        this->server_addr = optarg;
        break;
      case 'p':
        lp_log_trace("Server port: %s", optarg);
        this->server_port = optarg;
        break;
      case 'n':
        lp_log_trace("Source NIC address: %s", optarg);
        this->src_addr = optarg;
        break;
      case 'v':
        this->fabric_version = parse_version_string(optarg);
        if (!this->fabric_version) {
          this->~lp_sink_opts();
          free(shortopts);
          LP_THROW(errno, "Libfabric version string parse failed");
        }
        break;
      case 'b':
        if (optarg && strcmp(optarg, "rdma") == 0) {
          this->transport = "verbs;ofi_rxm";
        } else {
          this->transport = optarg;
        }
        break;
      case 'm':
        lp_log_trace("Shared mem path: %s", optarg);
        this->file = optarg;
        break;
      case 'l':
        this->shm_len = parse_mem_string(optarg);
        if (!this->shm_len) {
          free(shortopts);
          LP_THROW(EINVAL, "Shared memory file length invalid");
        }
        break;
      case 'i': this->interval = parse_interval_string(optarg); break;
      case 't': this->timeout = parse_interval_string(optarg) / 1000; break;
      case 'h':
        lp_log_debug("Help message requested, stopping");
        errno = EAGAIN;
        free(shortopts);
        LP_THROW(EAGAIN, "Help was requested");
      default: lp_log_warn("Invalid argument detected"); break;
    }
  }

  free(shortopts);
  if (!this->transport)
    this->transport = "verbs;ofi_rxm";
  if (!this->file)
    this->file = "/dev/shm/looking-glass";
  if (!check_valid_opts(arg_reqd, arg_pass, opts, desc, n_opts))
    LP_THROW(EINVAL, "Missing options");
  return;
}

/* ---- Source options ---- */

void lp_source_opts::clear_fields() {
  this->src_addr       = 0;
  this->src_port       = 0;
  this->file           = 0;
  this->fabric_version = 0;
  this->data_port      = 0;
  this->frame_port     = 0;
  this->timeout        = 0;
  this->interval       = 0;
  this->transport      = 0;
}

lp_source_opts::~lp_source_opts() { this->clear_fields(); }

lp_source_opts::lp_source_opts(int argc, char ** argv) {
  this->clear_fields();
  int o, idx = -1;

  const int       n_opts = sizeof(source_opts) / sizeof(struct option);
  lp_optdesc *    desc   = source_opts_desc;
  struct option * opts   = source_opts;
  char            arg_reqd[n_opts];
  char            arg_pass[n_opts];

  char * shortopts = long_to_short_opt_string(opts);
  if (!shortopts) {
    LP_THROW(ENOMEM, "Option string generation failed");
  }

  /* Defaults */
  this->fabric_version = TCM_DEFAULT_FABRIC_VERSION;
  this->timeout        = 3000;
  this->interval       = 500;
  this->once           = false;
  this->profiler       = false;

  set_opts_flags(arg_reqd, arg_pass, opts, desc, n_opts);
  while ((o = getopt_long(argc, argv, shortopts, opts, &idx)) != -1) {
    if (idx >= 0) {
      o = opts[idx].val;
    }
    for (int i = 0; i < n_opts; i++) {
      if (desc[i].id == o) {
        arg_pass[i] = 1;
      }
    }
    idx = -1;
    switch (o) {
      case 'n':
        lp_log_trace("Source NIC address: %s", optarg);
        this->src_addr = optarg;
        break;
      case 'p':
        lp_log_trace("Source port: %s", optarg);
        this->src_port = optarg;
        break;
      case 'v':
        this->fabric_version = parse_version_string(optarg);
        if (!this->fabric_version) {
          this->~lp_source_opts();
          LP_THROW(errno, "Libfabric version string parse failed");
        }
        break;
      case 'b':
        if (optarg && strcmp(optarg, "rdma") == 0) {
          this->transport = "verbs;ofi_rxm";
        } else {
          this->transport = optarg;
        }
        break;
      case 'm':
        lp_log_trace("Shared mem path: %s", optarg);
        this->file = optarg;
        break;
      case 'i': this->interval = parse_interval_string(optarg); break;
      case 't': this->timeout = parse_interval_string(optarg) / 1000; break;
      case 'o': this->once = true; break;
      case 'd': this->profiler = true; break;
      case 'h':
        lp_log_debug("Help message requested, stopping");
        errno = EAGAIN;
        free(shortopts);
        LP_THROW(EAGAIN, "Help was requested");
      default: lp_log_warn("Invalid argument detected"); break;
    }
  }

  free(shortopts);
  if (!this->file)
    this->file = "/dev/shm/looking-glass";
  if (!check_valid_opts(arg_reqd, arg_pass, opts, desc, n_opts))
    LP_THROW(EINVAL, "Missing options");
  return;
}