/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: lp_msg.proto */

/* Do not generate deprecated warnings for self */
#ifndef PROTOBUF_C__NO_DEPRECATED
#define PROTOBUF_C__NO_DEPRECATED
#endif

#include "lp_msg.pb-c.h"
void   lp_msg__build_version__init
                     (LpMsg__BuildVersion         *message)
{
  static const LpMsg__BuildVersion init_value = LP_MSG__BUILD_VERSION__INIT;
  *message = init_value;
}
size_t lp_msg__build_version__get_packed_size
                     (const LpMsg__BuildVersion *message)
{
  assert(message->base.descriptor == &lp_msg__build_version__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t lp_msg__build_version__pack
                     (const LpMsg__BuildVersion *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &lp_msg__build_version__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t lp_msg__build_version__pack_to_buffer
                     (const LpMsg__BuildVersion *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &lp_msg__build_version__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
LpMsg__BuildVersion *
       lp_msg__build_version__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (LpMsg__BuildVersion *)
     protobuf_c_message_unpack (&lp_msg__build_version__descriptor,
                                allocator, len, data);
}
void   lp_msg__build_version__free_unpacked
                     (LpMsg__BuildVersion *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &lp_msg__build_version__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   lp_msg__cursor_data__init
                     (LpMsg__CursorData         *message)
{
  static const LpMsg__CursorData init_value = LP_MSG__CURSOR_DATA__INIT;
  *message = init_value;
}
size_t lp_msg__cursor_data__get_packed_size
                     (const LpMsg__CursorData *message)
{
  assert(message->base.descriptor == &lp_msg__cursor_data__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t lp_msg__cursor_data__pack
                     (const LpMsg__CursorData *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &lp_msg__cursor_data__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t lp_msg__cursor_data__pack_to_buffer
                     (const LpMsg__CursorData *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &lp_msg__cursor_data__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
LpMsg__CursorData *
       lp_msg__cursor_data__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (LpMsg__CursorData *)
     protobuf_c_message_unpack (&lp_msg__cursor_data__descriptor,
                                allocator, len, data);
}
void   lp_msg__cursor_data__free_unpacked
                     (LpMsg__CursorData *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &lp_msg__cursor_data__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   lp_msg__keep_alive__init
                     (LpMsg__KeepAlive         *message)
{
  static const LpMsg__KeepAlive init_value = LP_MSG__KEEP_ALIVE__INIT;
  *message = init_value;
}
size_t lp_msg__keep_alive__get_packed_size
                     (const LpMsg__KeepAlive *message)
{
  assert(message->base.descriptor == &lp_msg__keep_alive__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t lp_msg__keep_alive__pack
                     (const LpMsg__KeepAlive *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &lp_msg__keep_alive__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t lp_msg__keep_alive__pack_to_buffer
                     (const LpMsg__KeepAlive *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &lp_msg__keep_alive__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
LpMsg__KeepAlive *
       lp_msg__keep_alive__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (LpMsg__KeepAlive *)
     protobuf_c_message_unpack (&lp_msg__keep_alive__descriptor,
                                allocator, len, data);
}
void   lp_msg__keep_alive__free_unpacked
                     (LpMsg__KeepAlive *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &lp_msg__keep_alive__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   lp_msg__disconnect__init
                     (LpMsg__Disconnect         *message)
{
  static const LpMsg__Disconnect init_value = LP_MSG__DISCONNECT__INIT;
  *message = init_value;
}
size_t lp_msg__disconnect__get_packed_size
                     (const LpMsg__Disconnect *message)
{
  assert(message->base.descriptor == &lp_msg__disconnect__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t lp_msg__disconnect__pack
                     (const LpMsg__Disconnect *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &lp_msg__disconnect__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t lp_msg__disconnect__pack_to_buffer
                     (const LpMsg__Disconnect *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &lp_msg__disconnect__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
LpMsg__Disconnect *
       lp_msg__disconnect__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (LpMsg__Disconnect *)
     protobuf_c_message_unpack (&lp_msg__disconnect__descriptor,
                                allocator, len, data);
}
void   lp_msg__disconnect__free_unpacked
                     (LpMsg__Disconnect *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &lp_msg__disconnect__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   lp_msg__message_wrapper__init
                     (LpMsg__MessageWrapper         *message)
{
  static const LpMsg__MessageWrapper init_value = LP_MSG__MESSAGE_WRAPPER__INIT;
  *message = init_value;
}
size_t lp_msg__message_wrapper__get_packed_size
                     (const LpMsg__MessageWrapper *message)
{
  assert(message->base.descriptor == &lp_msg__message_wrapper__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t lp_msg__message_wrapper__pack
                     (const LpMsg__MessageWrapper *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &lp_msg__message_wrapper__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t lp_msg__message_wrapper__pack_to_buffer
                     (const LpMsg__MessageWrapper *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &lp_msg__message_wrapper__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
LpMsg__MessageWrapper *
       lp_msg__message_wrapper__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (LpMsg__MessageWrapper *)
     protobuf_c_message_unpack (&lp_msg__message_wrapper__descriptor,
                                allocator, len, data);
}
void   lp_msg__message_wrapper__free_unpacked
                     (LpMsg__MessageWrapper *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &lp_msg__message_wrapper__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
static const ProtobufCFieldDescriptor lp_msg__build_version__field_descriptors[2] =
{
  {
    "lp_version",
    1,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_STRING,
    0,   /* quantifier_offset */
    offsetof(LpMsg__BuildVersion, lp_version),
    NULL,
    &protobuf_c_empty_string,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "lg_version",
    2,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_STRING,
    0,   /* quantifier_offset */
    offsetof(LpMsg__BuildVersion, lg_version),
    NULL,
    &protobuf_c_empty_string,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned lp_msg__build_version__field_indices_by_name[] = {
  1,   /* field[1] = lg_version */
  0,   /* field[0] = lp_version */
};
static const ProtobufCIntRange lp_msg__build_version__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 2 }
};
const ProtobufCMessageDescriptor lp_msg__build_version__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "lpMsg.BuildVersion",
  "BuildVersion",
  "LpMsg__BuildVersion",
  "lpMsg",
  sizeof(LpMsg__BuildVersion),
  2,
  lp_msg__build_version__field_descriptors,
  lp_msg__build_version__field_indices_by_name,
  1,  lp_msg__build_version__number_ranges,
  (ProtobufCMessageInit) lp_msg__build_version__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor lp_msg__cursor_data__field_descriptors[11] =
{
  {
    "dgid",
    1,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(LpMsg__CursorData, dgid),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "x",
    2,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(LpMsg__CursorData, x),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "y",
    3,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(LpMsg__CursorData, y),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "hpx",
    4,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(LpMsg__CursorData, hpx),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "hpy",
    5,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(LpMsg__CursorData, hpy),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "width",
    6,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(LpMsg__CursorData, width),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "height",
    7,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(LpMsg__CursorData, height),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "tex_fmt",
    8,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(LpMsg__CursorData, tex_fmt),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "data",
    9,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_BYTES,
    0,   /* quantifier_offset */
    offsetof(LpMsg__CursorData, data),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "pitch",
    10,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(LpMsg__CursorData, pitch),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "flags",
    11,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(LpMsg__CursorData, flags),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned lp_msg__cursor_data__field_indices_by_name[] = {
  8,   /* field[8] = data */
  0,   /* field[0] = dgid */
  10,   /* field[10] = flags */
  6,   /* field[6] = height */
  3,   /* field[3] = hpx */
  4,   /* field[4] = hpy */
  9,   /* field[9] = pitch */
  7,   /* field[7] = tex_fmt */
  5,   /* field[5] = width */
  1,   /* field[1] = x */
  2,   /* field[2] = y */
};
static const ProtobufCIntRange lp_msg__cursor_data__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 11 }
};
const ProtobufCMessageDescriptor lp_msg__cursor_data__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "lpMsg.CursorData",
  "CursorData",
  "LpMsg__CursorData",
  "lpMsg",
  sizeof(LpMsg__CursorData),
  11,
  lp_msg__cursor_data__field_descriptors,
  lp_msg__cursor_data__field_indices_by_name,
  1,  lp_msg__cursor_data__number_ranges,
  (ProtobufCMessageInit) lp_msg__cursor_data__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor lp_msg__keep_alive__field_descriptors[1] =
{
  {
    "info",
    1,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(LpMsg__KeepAlive, info),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned lp_msg__keep_alive__field_indices_by_name[] = {
  0,   /* field[0] = info */
};
static const ProtobufCIntRange lp_msg__keep_alive__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 1 }
};
const ProtobufCMessageDescriptor lp_msg__keep_alive__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "lpMsg.KeepAlive",
  "KeepAlive",
  "LpMsg__KeepAlive",
  "lpMsg",
  sizeof(LpMsg__KeepAlive),
  1,
  lp_msg__keep_alive__field_descriptors,
  lp_msg__keep_alive__field_indices_by_name,
  1,  lp_msg__keep_alive__number_ranges,
  (ProtobufCMessageInit) lp_msg__keep_alive__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor lp_msg__disconnect__field_descriptors[1] =
{
  {
    "info",
    1,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(LpMsg__Disconnect, info),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned lp_msg__disconnect__field_indices_by_name[] = {
  0,   /* field[0] = info */
};
static const ProtobufCIntRange lp_msg__disconnect__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 1 }
};
const ProtobufCMessageDescriptor lp_msg__disconnect__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "lpMsg.Disconnect",
  "Disconnect",
  "LpMsg__Disconnect",
  "lpMsg",
  sizeof(LpMsg__Disconnect),
  1,
  lp_msg__disconnect__field_descriptors,
  lp_msg__disconnect__field_indices_by_name,
  1,  lp_msg__disconnect__number_ranges,
  (ProtobufCMessageInit) lp_msg__disconnect__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor lp_msg__message_wrapper__field_descriptors[4] =
{
  {
    "cursor_data",
    1,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_MESSAGE,
    offsetof(LpMsg__MessageWrapper, wdata_case),
    offsetof(LpMsg__MessageWrapper, cursor_data),
    &lp_msg__cursor_data__descriptor,
    NULL,
    0 | PROTOBUF_C_FIELD_FLAG_ONEOF,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "ka",
    2,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_MESSAGE,
    offsetof(LpMsg__MessageWrapper, wdata_case),
    offsetof(LpMsg__MessageWrapper, ka),
    &lp_msg__keep_alive__descriptor,
    NULL,
    0 | PROTOBUF_C_FIELD_FLAG_ONEOF,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "disconnect",
    3,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_MESSAGE,
    offsetof(LpMsg__MessageWrapper, wdata_case),
    offsetof(LpMsg__MessageWrapper, disconnect),
    &lp_msg__disconnect__descriptor,
    NULL,
    0 | PROTOBUF_C_FIELD_FLAG_ONEOF,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "build_version",
    4,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_MESSAGE,
    offsetof(LpMsg__MessageWrapper, wdata_case),
    offsetof(LpMsg__MessageWrapper, build_version),
    &lp_msg__build_version__descriptor,
    NULL,
    0 | PROTOBUF_C_FIELD_FLAG_ONEOF,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned lp_msg__message_wrapper__field_indices_by_name[] = {
  3,   /* field[3] = build_version */
  0,   /* field[0] = cursor_data */
  2,   /* field[2] = disconnect */
  1,   /* field[1] = ka */
};
static const ProtobufCIntRange lp_msg__message_wrapper__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 4 }
};
const ProtobufCMessageDescriptor lp_msg__message_wrapper__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "lpMsg.MessageWrapper",
  "MessageWrapper",
  "LpMsg__MessageWrapper",
  "lpMsg",
  sizeof(LpMsg__MessageWrapper),
  4,
  lp_msg__message_wrapper__field_descriptors,
  lp_msg__message_wrapper__field_indices_by_name,
  1,  lp_msg__message_wrapper__number_ranges,
  (ProtobufCMessageInit) lp_msg__message_wrapper__init,
  NULL,NULL,NULL    /* reserved[123] */
};
