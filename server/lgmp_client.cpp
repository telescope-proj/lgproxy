// SPDX-License-Identifier: GPL-2.0-or-later

#include "lgmp_client.hpp"

#include <sys/ioctl.h>

using std::shared_ptr;

static const char * get_kvmfr_record_type(uint8_t type) {
  switch (type) {
    case KVMFR_RECORD_VMINFO: return "VMINFO";
    case KVMFR_RECORD_OSINFO: return "OSINFO";
    default: return "?";
  }
}

void udata_to_metadata(void * udata, size_t udata_size, lp_metadata * out) {
  /* Extract KVMFR udata */
  assert(udata);
  assert(udata_size);
  KVMFR *   base_info = (KVMFR *) udata;
  size_t    size_tmp  = udata_size - sizeof(KVMFR);
  uint8_t * p         = (uint8_t *) (base_info + 1);
  while (size_tmp >= sizeof(KVMFRRecord)) {
    KVMFRRecord * record = (KVMFRRecord *) p;
    p += sizeof(*record);
    size_tmp -= sizeof(*record);
    if (record->size > size_tmp) {
      tcm__log_warn("KVMFRRecord size %lu invalid", record->size);
      break;
    }
    lp_log_trace("KVMFRRecord Type: %s (%d), size: %d",
                 get_kvmfr_record_type(record->type), record->type,
                 record->size);
    KVMFRRecord_OSInfo * osi;
    KVMFRRecord_VMInfo * vmi;
    switch (record->type) {
      case KVMFR_RECORD_VMINFO:
        vmi          = (KVMFRRecord_VMInfo *) p;
        out->cores   = vmi->cores;
        out->threads = vmi->cpus;
        out->sockets = vmi->sockets;
        if (strnlen(vmi->model, LP_CPU_NAME_MAX) == LP_CPU_NAME_MAX)
          throw tcm_exception(ENOBUFS, __FILE__, __LINE__, "CPU string error");
        out->cpu_model.assign(vmi->model);
        out->fr_capture.assign(vmi->capture);
        memcpy(out->fr_uuid, vmi->uuid, 16);
        break;
      case KVMFR_RECORD_OSINFO:
        osi        = (KVMFRRecord_OSInfo *) p;
        out->fr_os = osi->os;
        if (strnlen(osi->name, LP_OS_NAME_MAX) == LP_OS_NAME_MAX)
          throw tcm_exception(ENOBUFS, __FILE__, __LINE__, "OS name error");
        out->os_name.assign(osi->name);
        break;
      default:
        tcm__log_warn("Unknown KVMFR record type: %d", record->type);
        break;
    }
    p += record->size;
    size_tmp -= record->size;
  }
}

void lp_kvmfr_udata_to_nfr(void * udata, size_t udata_size,
                           NFRHostMetadataConstructor & c) {
  assert(udata);
  assert(udata_size);
  uint16_t cores = 0, threads = 0, sockets = 0;
  char     uuid[16];
  memset(uuid, 0, 16);
  KVMFR *   base_info = (KVMFR *) udata;
  size_t    size_tmp  = udata_size - sizeof(KVMFR);
  uint8_t * p         = (uint8_t *) (base_info + 1);
  uint8_t   proxied   = 1;
  c.addField(NFR_F_EXT_PROXIED, &proxied);
  while (size_tmp >= sizeof(KVMFRRecord)) {
    KVMFRRecord * record = (KVMFRRecord *) p;
    p += sizeof(*record);
    size_tmp -= sizeof(*record);
    if (record->size > size_tmp) {
      tcm__log_warn("KVMFRRecord size %lu invalid", record->size);
      break;
    }
    lp_log_trace("KVMFRRecord Type: %d, size: %d", record->type, record->size);
    KVMFRRecord_OSInfo * osi;
    KVMFRRecord_VMInfo * vmi;
    switch (record->type) {
      case KVMFR_RECORD_VMINFO:
        if (record->size < sizeof(KVMFRRecord_VMInfo))
          throw tcm_exception(EINVAL, __FILE__, __LINE__, "VM info invalid");
        vmi     = (KVMFRRecord_VMInfo *) p;
        cores   = vmi->cores;
        threads = vmi->cpus;
        sockets = vmi->sockets;
        c.addField(NFR_F_UUID, vmi->uuid, 16);
        c.addField(NFR_F_EXT_CPU_SOCKETS, &sockets);
        c.addField(NFR_F_EXT_CPU_CORES, &cores);
        c.addField(NFR_F_EXT_CPU_THREADS, &threads);
        c.addField(NFR_F_EXT_CAPTURE_METHOD, vmi->capture,
                   strnlen(vmi->capture, sizeof(vmi->capture)));
        if (record->size - sizeof(*vmi)) {
          c.addField(NFR_F_EXT_CPU_MODEL, vmi->model,
                     record->size - sizeof(*vmi));
        }
        break;
      case KVMFR_RECORD_OSINFO:
        if (record->size < sizeof(KVMFRRecord_OSInfo))
          throw tcm_exception(EINVAL, __FILE__, __LINE__, "OS info invalid");
        osi = (KVMFRRecord_OSInfo *) p;
        c.addField(NFR_F_EXT_OS_ID, &osi->os);
        if (record->size - sizeof(*osi))
          c.addField(NFR_F_EXT_OS_NAME, &osi->name,
                     record->size - sizeof(*osi));
        break;
      default:
        lp_log_warn("Unknown KVMFR record type: %d", record->type);
        break;
    }
    p += record->size;
    size_tmp -= record->size;
  }
}

void lp_lgmp_client_q::bind_exit_flag(volatile int * flag) {
  this->exit_flag = flag;
}

void lp_lgmp_client_q::clear_fields() {
  this->lgmp        = 0;
  this->allow_skip  = 0;
  this->client_id   = 0;
  this->timeout_ms  = 0;
  this->interval_us = 0;
  this->mem         = 0;
  this->q           = 0;
  this->q_id        = (uint32_t) -1;
  this->udata       = 0;
  this->udata_size  = 0;
  this->mtd.clear();
}

lp_lgmp_msg lp_lgmp_client_q::get_msg() {
  lp_lgmp_msg out;
  LGMPMessage msg;
  LGMP_STATUS s;
  if (this->msg_pending) {
    s = lgmpClientMessageDone(this->q);
    if (s != LGMP_OK) {
      out.ptr   = 0;
      out.size  = -s;
      out.udata = 0;
    }
  }
  assert(this->q);
  s = lgmpClientProcess(this->q, &msg);
  switch (s) {
    case LGMP_OK:
      this->last_msg = lp_get_deadline(LP_NOW);
      lp_log_trace("Q %d Rcvd LGMP message, size: %d", this->q_id, msg.size);
      this->msg_pending = true;
      out.import(&msg);
      return out;
    default:
      out.ptr   = 0;
      out.size  = -s;
      out.udata = 0;
      return out;
  }
}

LGMP_STATUS lp_lgmp_client_q::send_msg(const void * data, size_t size,
                                       uint32_t * serial) {
  return lgmpClientSendData(this->q, data, size, serial);
}

LGMP_STATUS lp_lgmp_client_q::ack_msg() {
  if (this->msg_pending) {
    LGMP_STATUS s     = lgmpClientMessageDone(this->q);
    this->msg_pending = false;
    return s;
  }
  return LGMP_ERR_INVALID_ARGUMENT;
}

LGMP_STATUS lp_lgmp_client_q::subscribe(uint32_t q_id) {
  LGMP_STATUS ret = lgmpClientSubscribe(this->lgmp, q_id, &this->q);
  if (ret == LGMP_OK)
    this->q_id = q_id;
  return ret;
}

LGMP_STATUS lp_lgmp_client_q::unsubscribe() {
  return lgmpClientUnsubscribe(&this->q);
  this->q_id = (uint32_t) -1;
}

LGMP_STATUS lp_lgmp_client_q::connect(uint32_t q_id) {
  LGMP_STATUS s;
  timespec    dl = lp_get_deadline(this->timeout_ms);
  s              = this->init();
  if (s != LGMP_OK)
    return s;
  do {
    s = this->subscribe(q_id);
    switch (s) {
      case LGMP_OK:
      case LGMP_ERR_NO_SUCH_QUEUE: return s;
      default: break;
    }
    if (exit_flag && *exit_flag > 0)
      return LGMP_ERR_INVALID_SESSION;
  } while (!lp_check_deadline(dl));
  return LGMP_ERR_QUEUE_TIMEOUT;
}

LGMP_STATUS lp_lgmp_client_q::init() {
  LGMP_STATUS s;
  timespec    dl = lp_get_deadline(this->timeout_ms);
  do {
    s = lgmpClientInit(this->mem->get_ptr(), this->mem->get_size(),
                       &this->lgmp);
    if (exit_flag && *exit_flag > 0)
      return LGMP_ERR_INVALID_SESSION;
    if (s != LGMP_OK) {
      tcm_usleep(this->interval_us);
      continue;
    }
    break;
  } while (!lp_check_deadline(dl));
  if (s != LGMP_OK) {
    lp_log_debug("Failed to set up LGMP client: %s", lgmpStatusString(s));
    return s;
  }
  do {
    s = lgmpClientSessionInit(this->lgmp, &this->udata_size,
                              (uint8_t **) &this->udata, &this->client_id);
    if (exit_flag && *exit_flag > 0)
      return LGMP_ERR_INVALID_SESSION;
    if (s != LGMP_OK) {
      tcm_usleep(this->interval_us);
      continue;
    }
    break;
  } while (!lp_check_deadline(dl));
  if (s != LGMP_OK) {
    lp_log_debug("Failed to set up LGMP session: %s", lgmpStatusString(s));
    return s;
  }
  if (this->udata && this->udata_size)
    udata_to_metadata(this->udata, this->udata_size, &this->mtd);
  lp_log_debug("LGMP session initialized. id: %d, udata %p, len %d",
               this->client_id, this->udata, this->udata_size);
  return LGMP_OK;
}

lp_lgmp_client_q::lp_lgmp_client_q(shared_ptr<lp_shmem> shm,
                                   int64_t              timeout_msec,
                                   int64_t              interval_usec) {
  this->clear_fields();
  if (!shm.get() || !shm->get_ptr())
    throw tcm_exception(ENOBUFS, __FILE__, __LINE__, "Bad memory region");
  this->mem         = shm;
  this->timeout_ms  = timeout_msec;
  this->interval_us = interval_usec;
  this->q           = 0;
  this->client_id   = 0;
  this->msg_pending = false;
  this->exit_flag   = 0;
  return;
}

bool lp_lgmp_client_q::connected() {
  return this->lgmp && lgmpClientSessionValid(this->lgmp) && this->q;
}

const timespec & lp_lgmp_client_q::last_msg_time() {
  return this->last_msg;
}

lp_lgmp_client_q::~lp_lgmp_client_q() {
  this->unsubscribe();
  lgmpClientFree(&this->lgmp);
  this->clear_fields();
}