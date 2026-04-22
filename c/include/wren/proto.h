#pragma once

#include <stdint.h>
#include <stddef.h>

#define MSG_HEADER_SIZE 12
#define MAX_METHODS 256

#define MSG_TYPE_CALL 1
#define MSG_TYPE_RESPONSE 2
#define MSG_TYPE_ERROR 3

struct msg_header {
    uint32_t length;
    uint8_t type;
    uint8_t flags;
    uint16_t method_id;
    uint32_t req_id;
};

typedef struct wren_call wren_call_t;

/**
 * Server-side handler signature. Registered with wren_server_register
 * and invoked once per incoming CALL. Arguments are read from the call
 * via wren_call_read_*; the response is sent via wren_call_reply_*.
 *
 * The handler should call exactly one reply function before returning.
 */
typedef void (*handler_fn)(wren_call_t *call);

void msg_parse_h(const char *buf, struct msg_header *out);

size_t proto_enc_u8(char *buf, uint8_t v);
size_t proto_enc_u16(char *buf, uint16_t v);
size_t proto_enc_u32(char *buf, uint32_t v);
size_t proto_enc_u64(char *buf, uint64_t v);
size_t proto_enc_f32(char *buf, float v);
size_t proto_enc_f64(char *buf, double v);
size_t proto_enc_bytes(char *buf, const char *data, uint32_t len);

size_t proto_dec_u8(const char *buf, size_t avail, uint8_t *out);
size_t proto_dec_u16(const char *buf, size_t avail, uint16_t *out);
size_t proto_dec_u32(const char *buf, size_t avail, uint32_t *out);
size_t proto_dec_u64(const char *buf, size_t avail, uint64_t *out);
size_t proto_dec_f32(const char *buf, size_t avail, float *out);
size_t proto_dec_f64(const char *buf, size_t avail, double *out);
size_t proto_dec_bytes(const char *buf, size_t avail, const char **out_data, uint32_t *out_len);