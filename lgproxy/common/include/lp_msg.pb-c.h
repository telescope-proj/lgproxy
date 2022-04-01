/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: lp_msg.proto */

#ifndef PROTOBUF_C_lp_5fmsg_2eproto__INCLUDED
#define PROTOBUF_C_lp_5fmsg_2eproto__INCLUDED

#include <protobuf-c/protobuf-c.h>

PROTOBUF_C__BEGIN_DECLS

#if PROTOBUF_C_VERSION_NUMBER < 1003000
# error This file was generated by a newer version of protoc-c which is incompatible with your libprotobuf-c headers. Please update your headers.
#elif 1004000 < PROTOBUF_C_MIN_COMPILER_VERSION
# error This file was generated by an older version of protoc-c which is incompatible with your libprotobuf-c headers. Please regenerate this file with a newer version of protoc-c.
#endif


typedef struct LpMsg__CursorData LpMsg__CursorData;
typedef struct LpMsg__MessageWrapper LpMsg__MessageWrapper;


/* --- enums --- */


/* --- messages --- */

/**
 *  @brief Cursor data update
 *If the display source contains a cursor, the cursor position information
 *should be sent separately. To update the cursor shape, the values 
 *width and height should be set to non-zero values, with tex_fmt and
 *bytes being set to the cursor data type and data respectively.
 */
struct  LpMsg__CursorData
{
  ProtobufCMessage base;
  /**
   * Display group ID
   */
  uint32_t dgid;
  /**
   * Cursor X position
   */
  uint32_t x;
  /**
   * Cursor Y position
   */
  uint32_t y;
  /**
   * Cursor X hotspot
   */
  uint32_t hpx;
  /**
   * Cursor Y hotspot
   */
  uint32_t hpy;
  /**
   * Cursor image width
   */
  uint32_t width;
  /**
   * Cursor image height
   */
  uint32_t height;
  /**
   * Cursor image format
   */
  uint32_t tex_fmt;
  /**
   * Raw image data
   */
  ProtobufCBinaryData data;
  /**
   * row length in bytes of the shape
   */
  uint32_t pitch;
};
#define LP_MSG__CURSOR_DATA__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&lp_msg__cursor_data__descriptor) \
    , 0, 0, 0, 0, 0, 0, 0, 0, {0,NULL}, 0 }


typedef enum {
  LP_MSG__MESSAGE_WRAPPER__WDATA__NOT_SET = 0,
  LP_MSG__MESSAGE_WRAPPER__WDATA_CURSOR_DATA = 1
    PROTOBUF_C__FORCE_ENUM_TO_BE_INT_SIZE(LP_MSG__MESSAGE_WRAPPER__WDATA__CASE)
} LpMsg__MessageWrapper__WdataCase;

struct  LpMsg__MessageWrapper
{
  ProtobufCMessage base;
  LpMsg__MessageWrapper__WdataCase wdata_case;
  union {
    LpMsg__CursorData *cursor_data;
  };
};
#define LP_MSG__MESSAGE_WRAPPER__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&lp_msg__message_wrapper__descriptor) \
    , LP_MSG__MESSAGE_WRAPPER__WDATA__NOT_SET, {0} }


/* LpMsg__CursorData methods */
void   lp_msg__cursor_data__init
                     (LpMsg__CursorData         *message);
size_t lp_msg__cursor_data__get_packed_size
                     (const LpMsg__CursorData   *message);
size_t lp_msg__cursor_data__pack
                     (const LpMsg__CursorData   *message,
                      uint8_t             *out);
size_t lp_msg__cursor_data__pack_to_buffer
                     (const LpMsg__CursorData   *message,
                      ProtobufCBuffer     *buffer);
LpMsg__CursorData *
       lp_msg__cursor_data__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   lp_msg__cursor_data__free_unpacked
                     (LpMsg__CursorData *message,
                      ProtobufCAllocator *allocator);
/* LpMsg__MessageWrapper methods */
void   lp_msg__message_wrapper__init
                     (LpMsg__MessageWrapper         *message);
size_t lp_msg__message_wrapper__get_packed_size
                     (const LpMsg__MessageWrapper   *message);
size_t lp_msg__message_wrapper__pack
                     (const LpMsg__MessageWrapper   *message,
                      uint8_t             *out);
size_t lp_msg__message_wrapper__pack_to_buffer
                     (const LpMsg__MessageWrapper   *message,
                      ProtobufCBuffer     *buffer);
LpMsg__MessageWrapper *
       lp_msg__message_wrapper__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   lp_msg__message_wrapper__free_unpacked
                     (LpMsg__MessageWrapper *message,
                      ProtobufCAllocator *allocator);
/* --- per-message closures --- */

typedef void (*LpMsg__CursorData_Closure)
                 (const LpMsg__CursorData *message,
                  void *closure_data);
typedef void (*LpMsg__MessageWrapper_Closure)
                 (const LpMsg__MessageWrapper *message,
                  void *closure_data);

/* --- services --- */


/* --- descriptors --- */

extern const ProtobufCMessageDescriptor lp_msg__cursor_data__descriptor;
extern const ProtobufCMessageDescriptor lp_msg__message_wrapper__descriptor;

PROTOBUF_C__END_DECLS


#endif  /* PROTOBUF_C_lp_5fmsg_2eproto__INCLUDED */