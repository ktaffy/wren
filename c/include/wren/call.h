// wren/call.h
/**
 * Per-call API available to handlers: read arguments from the incoming
 * payload with wren_call_read_*, reply with wren_call_reply_*.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "wren/codec.h"

/**
 * A single in-flight RPC call on the server side.
 *
 * A wren_call_t is constructed by the server when a CALL message arrives
 * and passed to the registered handler. It holds the connection, the
 * request id, and a cursor into the payload so handlers can read
 * arguments sequentially without tracking offsets.
 */
typedef struct wren_call wren_call_t;

/** 
 * Each read advances an internal cursor. On the first failure, the call
 * enters an "error" state and subsequent reads become no-ops that return
 * -1. Handlers may either check each read's return value or check once
 * at the end via wren_call_ok().
 */
int wren_call_read_bool(wren_call_t *call, bool *out);
int wren_call_read_u8(wren_call_t *call, uint8_t *out);
int wren_call_read_u16(wren_call_t *call, uint16_t *out);
int wren_call_read_u32(wren_call_t *call, uint32_t *out);
int wren_call_read_u64(wren_call_t *call, uint64_t *out);
int wren_call_read_i8(wren_call_t *call, int8_t *out);
int wren_call_read_i16(wren_call_t *call, int16_t *out);
int wren_call_read_i32(wren_call_t *call, int32_t *out);
int wren_call_read_i64(wren_call_t *call, int64_t *out);
int wren_call_read_f32(wren_call_t *call, float *out);
int wren_call_read_f64(wren_call_t *call, double *out);

/**
 * Read a variable-length byte array. Zero-copy: @p out_data points
 * inside the call's payload buffer and is valid only for the duration
 * of the handler.
 */
int wren_call_read_bytes(wren_call_t *call, const char **out_data, uint32_t *out_len);

/**
 * Read a variable-length UTF-8 string. Same zero-copy semantics as
 * wren_call_read_bytes; not null-terminated.
 */
int wren_call_read_string(wren_call_t *call, const char **out_data, uint32_t *out_len);

/**
 * Return true if no read has failed yet on this call.
 */
bool wren_call_ok(wren_call_t *call);

/*
 * Each reply function sends a complete RESPONSE message and marks the
 * call as "replied". A handler must call exactly one reply function
 * (including wren_call_reply_error or wren_call_reply_empty) before
 * returning. Multiple calls on the same wren_call_t are a bug.
 */
int wren_call_reply_bool(wren_call_t *call, bool v);
int wren_call_reply_u8(wren_call_t *call, uint8_t v);
int wren_call_reply_u16(wren_call_t *call, uint16_t v);
int wren_call_reply_u32(wren_call_t *call, uint32_t v);
int wren_call_reply_u64(wren_call_t *call, uint64_t v);
int wren_call_reply_i8(wren_call_t *call, int8_t v);
int wren_call_reply_i16(wren_call_t *call, int16_t v);
int wren_call_reply_i32(wren_call_t *call, int32_t v);
int wren_call_reply_i64(wren_call_t *call, int64_t v);
int wren_call_reply_f32(wren_call_t *call, float v);
int wren_call_reply_f64(wren_call_t *call, double v);

/**
 * Send a variable-length byte array as the response.
 */
int wren_call_reply_bytes(wren_call_t *call, const char *data, uint32_t len);

/**
 * Send a variable-length UTF-8 string as the response.
 */
int wren_call_reply_string(wren_call_t *call, const char *data, uint32_t len);

/**
 * Send a zero-byte response. For methods that return void.
 */
int wren_call_reply_empty(wren_call_t *call);

/**
 * Send a pre-built payload as the response. For methods with multiple
 * return values or composite return types: the handler builds the
 * payload itself using proto_enc_* helpers, then passes it here.
 *
 * @param data  Payload bytes (may be NULL if len is 0).
 * @param len   Payload length in bytes.
 */
int wren_call_reply_raw(wren_call_t *call, const char *data, size_t len);

/**
 * Send an ERROR response instead of a RESPONSE.
 *
 * @param code     Application-defined error code. 0 means "unknown error".
 * @param message  UTF-8 error description. May be NULL or empty.
 */
int wren_call_reply_error(wren_call_t *call, uint32_t code, const char *message);

/** Context pointer given to wren_server_register_ctx for this method. */
void *wren_call_ctx(wren_call_t *call);

/** Arena for this call. Freed automatically after the handler returns. */
wren_arena_t *wren_call_arena(wren_call_t *call);

/** Raw payload bytes, valid until the handler returns. */
void wren_call_payload(wren_call_t *call, const char **data, size_t *len);

/** True once any reply function has sent a reply for this call. */
bool wren_call_replied(const wren_call_t *call);