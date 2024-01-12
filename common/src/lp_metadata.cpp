#include "lp_metadata.h"
#include "lg_build_version.h"
#include "lp_utils.h"

void lp_metadata::clear() {
  width       = 0;
  height      = 0;
  stride      = 0;
  pitch       = 0;
  sockets     = 0;
  threads     = 0;
  cores       = 0;
  fr_flags    = 0;
  fr_os       = 0;
  fr_format   = 0;
  fr_rotation = 0;
  nfr_flags   = 0;
  proxied     = 0;
  memset(fr_uuid, 0, sizeof(fr_uuid));
  fr_capture.clear();
  cpu_model.clear();
  os_name.clear();
  x_link_rate = 0;
  x_sys_name.clear();
}

int lp_metadata::export_udata(void * udata, size_t * size,
                              uint32_t feature_flags) {
  assert(size);
  size_t ttl = this->cpu_model.length() + 1 + this->os_name.length() + 1 +
               sizeof(KVMFRRecord_OSInfo) + sizeof(KVMFRRecord_VMInfo) +
               2 * sizeof(KVMFRRecord) + sizeof(KVMFR);
  if (*size < ttl) {
    *size = ttl;
    return -ENOBUFS;
  }
  assert(udata);

  KVMFR * kvmfr   = static_cast<KVMFR *>(udata);
  kvmfr->version  = KVMFR_VERSION;
  kvmfr->features = feature_flags;
  memcpy(kvmfr->magic, KVMFR_MAGIC, sizeof(kvmfr->magic));
  strncpy(kvmfr->hostver, LG_BUILD_VERSION, sizeof(kvmfr->hostver) - 1);
  kvmfr->hostver[sizeof(kvmfr->hostver) - 1] = 0;

  KVMFRRecord * rec = (KVMFRRecord *) (kvmfr + 1);
  rec->type         = KVMFR_RECORD_OSINFO;
  rec->size         = sizeof(KVMFRRecord_OSInfo) + this->os_name.length() + 1;

  KVMFRRecord_OSInfo * osi = (KVMFRRecord_OSInfo *) rec->data;
  osi->os                  = this->fr_os;
  strcpy(osi->name, this->os_name.c_str());

  rec = (KVMFRRecord *) ((uint8_t *) osi->name + this->os_name.length() + 1);
  rec->type = KVMFR_RECORD_VMINFO;
  rec->size = sizeof(KVMFRRecord_VMInfo) + this->cpu_model.length() + 1;

  KVMFRRecord_VMInfo * vmi = (KVMFRRecord_VMInfo *) rec->data;
  vmi->cores               = this->cores;
  vmi->sockets             = this->sockets;
  vmi->cpus                = this->threads;
  memcpy(vmi->uuid, this->fr_uuid, sizeof(vmi->uuid));
  lp_strfcpy(vmi->capture, this->fr_capture.c_str(), sizeof(vmi->capture));
  strcpy(vmi->model, this->cpu_model.c_str());
  *size = ttl;
  return 0;
}

void lp_metadata::import_nfr(NFRHostMetadata * msg, size_t size) {
  NFRField * f        = (NFRField *) msg->data;
  ssize_t    rem_size = size - sizeof(*msg);
  while (rem_size) {
    if (f->len > rem_size - sizeof(*f))
      throw tcm_exception(EINVAL, _LP_FNAME_, __LINE__, "Malformed message");
    rem_size -= (sizeof(*f) + f->len);
    assert(rem_size >= 0);
    switch (f->type) {
      case NFR_F_FEATURE_FLAGS: {
        if (f->len != 1)
          throw tcm_exception(EINVAL, _LP_FNAME_, __LINE__,
                              "Feature flag size invalid");
        this->nfr_flags = *(uint8_t *) f->data;
        break;
      }
      case NFR_F_UUID: {
        if (f->len != 16)
          throw tcm_exception(EINVAL, _LP_FNAME_, __LINE__, "Invalid UUID");
        memcpy(this->fr_uuid, f->data, 16);
        break;
      }
      case NFR_F_NAME: {
        if (f->len > LP_NAME_MAX)
          throw tcm_exception(EINVAL, _LP_FNAME_, __LINE__, "Invalid name");
        this->x_sys_name.assign((const char *) f->data, f->len);
        break;
      }
      case NFR_F_EXT_PROXIED: {
        if (f->len != 1)
          throw tcm_exception(EINVAL, _LP_FNAME_, __LINE__, "Invalid proxy flag");
        this->proxied = *(uint8_t *) f->data;
        break;
      }
      case NFR_F_EXT_CPU_SOCKETS: {
        if (f->len != 2)
          throw tcm_exception(EINVAL, _LP_FNAME_, __LINE__,
                              "Invalid CPU socket field");
        this->sockets = *(uint16_t *) f->data;
        break;
      }
      case NFR_F_EXT_CPU_CORES: {
        if (f->len != 2)
          throw tcm_exception(EINVAL, _LP_FNAME_, __LINE__,
                              "Invalid CPU core field");
        this->cores = *(uint16_t *) f->data;
        break;
      }
      case NFR_F_EXT_CPU_THREADS: {
        if (f->len != 2)
          throw tcm_exception(EINVAL, _LP_FNAME_, __LINE__,
                              "Invalid CPU thread field");
        this->threads = *(uint16_t *) f->data;
        break;
      }
      case NFR_F_EXT_CAPTURE_METHOD: {
        if (f->len > LP_CAPTURE_METHOD_MAX)
          throw tcm_exception(EINVAL, _LP_FNAME_, __LINE__,
                              "Invalid capture method");
        this->fr_capture.assign((const char *) f->data, f->len);
        break;
      }
      case NFR_F_EXT_OS_ID: {
        if (f->len != 1)
          throw tcm_exception(EINVAL, _LP_FNAME_, __LINE__, "Invalid OS ID");
        this->fr_os = *(uint8_t *) f->data;
        break;
      }
      case NFR_F_EXT_OS_NAME: {
        if (f->len > LP_OS_NAME_MAX)
          throw tcm_exception(EINVAL, _LP_FNAME_, __LINE__, "Invalid OS name");
        this->os_name.assign((const char *) f->data, f->len);
        break;
      }
      case NFR_F_EXT_CPU_MODEL: {
        if (f->len > LP_CPU_NAME_MAX)
          throw tcm_exception(EINVAL, _LP_FNAME_, __LINE__, "Invalid CPU model");
        this->cpu_model.assign((const char *) f->data, f->len);
        break;
      }
      case NFR_F_EXT_LINK_RATE: {
        if (f->len != 8)
          throw tcm_exception(EINVAL, _LP_FNAME_, __LINE__, "Invalid link rate");
        this->x_link_rate = *(uint64_t *) f->data;
        break;
      }
      case NFR_F_INVALID:
      case NFR_F_MAX:
        throw tcm_exception(EINVAL, _LP_FNAME_, __LINE__, "Invalid field type");
      default: {
        lp_log_debug("Ignored field type %d", f->type);
      }
    }
    f = (NFRField *) (((uint8_t *) f) + sizeof(*f) + f->len);
    if (rem_size <= 0)
      break;
  }
}