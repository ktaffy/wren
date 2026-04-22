#include "wren/call.h"
#include "wren/proto.h"
#include "internal/conn.h"
#include "internal/server_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

/**
 * Private layout. Constructed on the stack in srv_dispatch and passed
 * to the handler. Lifetime ends when the handler returns.
 */
struct wren_call {
    struct conn *conn;
    uint32_t req_id;

    const char *payload;
    size_t payload_len;
    size_t cursor;

    bool ok;
    bool replied;
};

/**
 * Build an internal wren_call for a newly-arrived CALL message.
 * Called by srv_dispatch. Not in the public API.
 */
static void call_init(struct wren_call *call, struct conn *c, uint32_t req_id, const char *payload, size_t payload_len) {
    call->conn = c;
    call->req_id = req_id;
    call->payload = payload;
    call->payload_len = payload_len;
    call->cursor = 0;
    call->ok = true;
    call->replied = false;
}

/**
 * Send a header + payload with the given message type. Shared by every
 * reply function in this file (including reply_error).
 */
static int send_msg(struct wren_call *call, uint8_t msg_type, const char *payload, size_t payload_len) {
    if (call->replied) {
        fprintf(stderr, "wren_call: double reply on req_id=%u\n", call->req_id);
        return -1;
    }
    call->replied = true;

    char header[MSG_HEADER_SIZE];
    uint32_t total_len = MSG_HEADER_SIZE + (uint32_t)payload_len;

    uint32_t len_n = htonl(total_len);
    uint16_t method_n = 0;
    uint32_t req_n = htonl(call->req_id);

    memcpy(header + 0, &len_n, 4);
    header[4] = msg_type;
    header[5] = 0; // flags
    memcpy(header + 6, &method_n, 2);
    memcpy(header + 8, &req_n, 4);

    if (conn_write(call->conn, header, MSG_HEADER_SIZE) < 0)
        return -1;
    if (payload_len > 0) {
        if (conn_write(call->conn, payload, payload_len) < 0)
            return -1;
    }
    return 0;
}

bool wren_call_ok(wren_call_t *call) { return call && call->ok; }

int wren_call_read_u8(wren_call_t *call, uint8_t *out) {
    if (!call->ok) return -1;
    size_t n = proto_dec_u8(call->payload + call->cursor, call->payload_len - call->cursor, out);
    if (!n) {
        call->ok = false;
        return -1;
    }
    call->cursor += n;
    return 0;
}

int wren_call_read_u16(wren_call_t *call, uint16_t *out) {
    if (!call->ok) return -1;
    size_t n = proto_dec_u16(call->payload + call->cursor, call->payload_len - call->cursor, out);
    if (!n) {
        call->ok = false;
        return -1;
    }
    call->cursor += n;
    return 0;
}

int wren_call_read_u32(wren_call_t *call, uint32_t *out) {
    if (!call->ok) return -1;
    size_t n = proto_dec_u32(call->payload + call->cursor, call->payload_len - call->cursor, out);
    if (!n) {
        call->ok = false;
        return -1;
    }
    call->cursor += n;
    return 0;
}

int wren_call_read_u64(wren_call_t *call, uint64_t *out) {
    if (!call->ok) return -1;
    size_t n = proto_dec_u64(call->payload + call->cursor, call->payload_len - call->cursor, out);
    if (!n) {
        call->ok = false;
        return -1;
    }
    call->cursor += n;
    return 0;
}

int wren_call_read_i8(wren_call_t *call, int8_t *out) { return wren_call_read_u8(call, (uint8_t *)out); }
int wren_call_read_i16(wren_call_t *call, int16_t *out) { return wren_call_read_u16(call, (uint16_t *)out); }
int wren_call_read_i32(wren_call_t *call, int32_t *out) { return wren_call_read_u32(call, (uint32_t *)out); }
int wren_call_read_i64(wren_call_t *call, int64_t *out) { return wren_call_read_u64(call, (uint64_t *)out); }

int wren_call_read_bool(wren_call_t *call, bool *out) {
    uint8_t v;
    if (wren_call_read_u8(call, &v) < 0) return -1;
    *out = (v != 0);
    return 0;
}

int wren_call_read_f32(wren_call_t *call, float *out) {
    if (!call->ok) return -1;
    size_t n = proto_dec_f32(call->payload + call->cursor, call->payload_len - call->cursor, out);
    if (!n) {
        call->ok = false;
        return -1;
    }
    call->cursor += n;
    return 0;
}

int wren_call_read_f64(wren_call_t *call, double *out) {
    if (!call->ok) return -1;
    size_t n = proto_dec_f64(call->payload + call->cursor, call->payload_len - call->cursor, out);
    if (!n) {
        call->ok = false;
        return -1;
    }
    call->cursor += n;
    return 0;
}

int wren_call_read_bytes(wren_call_t *call, const char **out_data, uint32_t *out_len) {
    if (!call->ok) return -1;
    size_t n = proto_dec_bytes(call->payload + call->cursor, call->payload_len - call->cursor, out_data, out_len);
    if (!n) {
        call->ok = false;
        return -1;
    }
    call->cursor += n;
    return 0;
}

int wren_call_read_string(wren_call_t *call, const char **out_data, uint32_t *out_len) {
    return wren_call_read_bytes(call, out_data, out_len);
}

int wren_call_reply_u8(wren_call_t *call, uint8_t v) {
    char buf[1];
    proto_enc_u8(buf, v);
    return send_msg(call, MSG_TYPE_RESPONSE, buf, 1);
}

int wren_call_reply_u16(wren_call_t *call, uint16_t v) {
    char buf[2];
    proto_enc_u16(buf, v);
    return send_msg(call, MSG_TYPE_RESPONSE, buf, 2);
}

int wren_call_reply_u32(wren_call_t *call, uint32_t v) {
    char buf[4];
    proto_enc_u32(buf, v);
    return send_msg(call, MSG_TYPE_RESPONSE, buf, 4);
}

int wren_call_reply_u64(wren_call_t *call, uint64_t v) {
    char buf[8];
    proto_enc_u64(buf, v);
    return send_msg(call, MSG_TYPE_RESPONSE, buf, 8);
}

int wren_call_reply_i8(wren_call_t *call, int8_t v) { return wren_call_reply_u8(call, (uint8_t)v); }
int wren_call_reply_i16(wren_call_t *call, int16_t v) { return wren_call_reply_u16(call, (uint16_t)v); }
int wren_call_reply_i32(wren_call_t *call, int32_t v) { return wren_call_reply_u32(call, (uint32_t)v); }
int wren_call_reply_i64(wren_call_t *call, int64_t v) { return wren_call_reply_u64(call, (uint64_t)v); }

int wren_call_reply_bool(wren_call_t *call, bool v) {
    return wren_call_reply_u8(call, v ? 1 : 0);
}

int wren_call_reply_f32(wren_call_t *call, float v) {
    char buf[4];
    proto_enc_f32(buf, v);
    return send_msg(call, MSG_TYPE_RESPONSE, buf, 4);
}

int wren_call_reply_f64(wren_call_t *call, double v) {
    char buf[8];
    proto_enc_f64(buf, v);
    return send_msg(call, MSG_TYPE_RESPONSE, buf, 8);
}

int wren_call_reply_bytes(wren_call_t *call, const char *data, uint32_t len) {
    char stack_buf[256];
    char *buf = (4 + len <= sizeof(stack_buf)) ? stack_buf : malloc(4 + len);
    if (!buf)
        return -1;

    proto_enc_bytes(buf, data, len);
    int rc = send_msg(call, MSG_TYPE_RESPONSE, buf, 4 + len);

    if (buf != stack_buf)
        free(buf);
    return rc;
}

int wren_call_reply_string(wren_call_t *call, const char *data, uint32_t len) {
    return wren_call_reply_bytes(call, data, len);
}

int wren_call_reply_empty(wren_call_t *call) {
    return send_msg(call, MSG_TYPE_RESPONSE, NULL, 0);
}

int wren_call_reply_raw(wren_call_t *call, const char *data, size_t len) {
    return send_msg(call, MSG_TYPE_RESPONSE, data, len);
}

int wren_call_reply_error(wren_call_t *call, uint32_t code, const char *message) {
    uint32_t msg_len = message ? (uint32_t)strlen(message) : 0;
    size_t total = 4 + 4 + msg_len;

    char stack_buf[256];
    char *buf = (total <= sizeof(stack_buf)) ? stack_buf : malloc(total);
    if (!buf) return -1;

    size_t off = 0;
    off += proto_enc_u32(buf + off, code);
    off += proto_enc_bytes(buf + off, message ? message : "", msg_len);

    int rc = send_msg(call, MSG_TYPE_ERROR, buf, off);

    if (buf != stack_buf)
        free(buf);
    return rc;
}

/**
 * Invoke a handler for an incoming CALL. Constructs the wren_call
 * locally (so wren_call's layout stays private to this file) and passes
 * it to the handler.
 *
 * Declared in server_internal.h; called by srv_dispatch.
 */
void call_dispatch(struct conn *c, uint32_t req_id, const char *payload, size_t payload_len, handler_fn fn)
{
    struct wren_call call;
    call_init(&call, c, req_id, payload, payload_len);
    fn(&call);
}