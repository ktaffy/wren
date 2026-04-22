#include "wren/server.h"
#include "internal/net.h"
#include "internal/epoll_util.h"
#include "internal/accept.h"
#include "internal/conn.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#define BACKLOG 128
#define MAX_EVENTS 64
#define PORT 8080

#define METHOD_ADD 1
#define METHOD_ECHO_BYTES 2
#define METHOD_NOOP 3
#define METHOD_PI 4
#define METHOD_MUL_I64 5
#define METHOD_MAKE_VEC3 6
#define METHOD_SUM_ARRAY 7
#define METHOD_DIVIDE 8
#define METHOD_CONCAT 9
#define METHOD_IS_EVEN 10
#define METHOD_MATMUL_4X4 11
#define METHOD_PARSE_CSV 12
#define METHOD_CONCURRENT_STRESS 13
#define METHOD_QUERY_DB 14
#define METHOD_ECHO_LARGE 15

static void handle_add(struct conn *c, uint32_t req_id, const char *payload, size_t payload_len) {
    uint32_t a, b;
    size_t off = 0;
    size_t n;

    n = proto_dec_u32(payload + off, payload_len - off, &a);
    if (n == 0) {
        fprintf(stderr, "add: bad payload (a)\n");
        return;
    }
    off += n;

    n = proto_dec_u32(payload + off, payload_len - off, &b);
    if (n == 0) {
        fprintf(stderr, "add: bad payload (b)\n");
        return;
    }
    off += n;

    uint32_t result = a + b;
    printf("add: req=%u %u + %u = %u\n", req_id, a, b, result);

    char out[4];
    proto_enc_u32(out, result);
    wren_send_response(c, req_id, out, sizeof(out));
}

static void handle_echo_bytes(struct conn *c, uint32_t req_id, const char *payload, size_t payload_len) {
    const char *data;
    uint32_t len;
    if (proto_dec_bytes(payload, payload_len, &data, &len) == 0) {
        fprintf(stderr, "echo_bytes: bad payload\n");
        return;
    }

    // Response is just the same bytes, re-encoded.
    char *out = malloc(4 + len);
    proto_enc_bytes(out, data, len);
    wren_send_response(c, req_id, out, 4 + len);
    free(out);
}

static void handle_noop(struct conn *c, uint32_t req_id, const char *payload, size_t payload_len) {
    (void)payload;
    (void)payload_len;
    wren_send_response(c, req_id, NULL, 0);
}

static void handle_pi(struct conn *c, uint32_t req_id, const char *payload, size_t payload_len) {
    (void)payload;
    (void)payload_len;
    char out[8];
    proto_enc_f64(out, 3.141592653589793);
    wren_send_response(c, req_id, out, 8);
}

static void handle_mul_i64(struct conn *c, uint32_t req_id, const char *payload, size_t payload_len) {
    uint64_t ua, ub;
    size_t off = 0, n;
    n = proto_dec_u64(payload + off, payload_len - off, &ua);
    if (!n) return;
    off += n;
    n = proto_dec_u64(payload + off, payload_len - off, &ub);
    if (!n) return;
    off += n;

    int64_t a = (int64_t)ua;
    int64_t b = (int64_t)ub;
    int64_t result = a * b;

    char out[8];
    proto_enc_u64(out, (uint64_t)result);
    wren_send_response(c, req_id, out, 8);
}

static void handle_make_vec3(struct conn *c, uint32_t req_id, const char *payload, size_t payload_len) {
    float x, y, z;
    size_t off = 0, n;
    n = proto_dec_f32(payload + off, payload_len - off, &x);
    if (!n) return;
    off += n;
    n = proto_dec_f32(payload + off, payload_len - off, &y);
    if (!n) return;
    off += n;
    n = proto_dec_f32(payload + off, payload_len - off, &z);
    if (!n) return;
    off += n;

    // Multiple return values = struct on the wire = three f32s in order
    char out[12];
    off = 0;
    off += proto_enc_f32(out + off, x * 2);
    off += proto_enc_f32(out + off, y * 2);
    off += proto_enc_f32(out + off, z * 2);
    wren_send_response(c, req_id, out, 12);
}

static void handle_sum_array(struct conn *c, uint32_t req_id, const char *payload, size_t payload_len) {
    // Variable-length array: [count: u32][u32 × count]
    uint32_t count;
    size_t off = 0, n;
    n = proto_dec_u32(payload + off, payload_len - off, &count);
    if (!n) return;
    off += n;

    uint64_t total = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t v;
        n = proto_dec_u32(payload + off, payload_len - off, &v);
        if (!n) return;
        off += n;
        total += v;
    }

    char out[8];
    proto_enc_u64(out, total);
    wren_send_response(c, req_id, out, 8);
}

static void handle_divide(struct conn *c, uint32_t req_id, const char *payload, size_t payload_len) {
    uint32_t a, b;
    size_t off = 0, n;
    n = proto_dec_u32(payload + off, payload_len - off, &a);
    if (!n) return;
    off += n;
    n = proto_dec_u32(payload + off, payload_len - off, &b);
    if (!n) return;
    off += n;

    if (b == 0) {
        // Build an ERROR response: [code: u32][msg_len: u32][msg]
        const char *msg = "division by zero";
        uint32_t msg_len = (uint32_t)strlen(msg);
        char err[8 + 64];
        size_t eoff = 0;
        eoff += proto_enc_u32(err + eoff, 42); // code = 42 (app-specific)
        eoff += proto_enc_bytes(err + eoff, msg, msg_len);

        // Send ERROR manually (wren_send_response is hardcoded to type=RESPONSE)
        char header[MSG_HEADER_SIZE];
        uint32_t total = MSG_HEADER_SIZE + (uint32_t)eoff;
        uint32_t len_n = htonl(total);
        uint16_t method_n = 0;
        uint32_t req_n = htonl(req_id);
        memcpy(header + 0, &len_n, 4);
        header[4] = MSG_TYPE_ERROR;
        header[5] = 0;
        memcpy(header + 6, &method_n, 2);
        memcpy(header + 8, &req_n, 4);
        conn_write(c, header, MSG_HEADER_SIZE);
        conn_write(c, err, eoff);
        return;
    }

    uint32_t result = a / b;
    char out[4];
    proto_enc_u32(out, result);
    wren_send_response(c, req_id, out, 4);
}

static void handle_concat(struct conn *c, uint32_t req_id, const char *payload, size_t payload_len) {
    const char *s1, *s2;
    uint32_t n1, n2;
    size_t off = 0, n;

    n = proto_dec_bytes(payload + off, payload_len - off, &s1, &n1);
    if (!n) return;
    off += n;
    n = proto_dec_bytes(payload + off, payload_len - off, &s2, &n2);
    if (!n) return;

    uint32_t total = n1 + n2;
    char *out = malloc(4 + total);
    proto_enc_u32(out, total);
    memcpy(out + 4, s1, n1);
    memcpy(out + 4 + n1, s2, n2);
    wren_send_response(c, req_id, out, 4 + total);
    free(out);
}

static void handle_is_even(struct conn *c, uint32_t req_id, const char *payload, size_t payload_len) {
    uint32_t v;
    size_t n = proto_dec_u32(payload, payload_len, &v);
    if (!n) return;

    char out[1];
    proto_enc_u8(out, (v % 2 == 0) ? 1 : 0);
    wren_send_response(c, req_id, out, 1);
}

static void handle_matmul_4x4(struct conn *c, uint32_t req_id, const char *payload, size_t payload_len){
    if (payload_len < 128){
        fprintf(stderr, "matmul: payload too short\n");
        return;
    }
    float a[16], b[16], r[16];
    size_t off = 0, n;
    for (int i = 0; i < 16; i++){
        n = proto_dec_f32(payload + off, payload_len - off, &a[i]);
        if (!n)
            return;
        off += n;
    }
    for (int i = 0; i < 16; i++){
        n = proto_dec_f32(payload + off, payload_len - off, &b[i]);
        if (!n)
            return;
        off += n;
    }
    for (int i = 0; i < 4; i++){
        for (int j = 0; j < 4; j++) {
            float sum = 0;
            for (int k = 0; k < 4; k++) {
                sum += a[i * 4 + k] * b[k * 4 + j];
            }
            r[i * 4 + j] = sum;
        }
    }
    char out[64];
    size_t out_off = 0;
    for (int i = 0; i < 16; i++) {
        out_off += proto_enc_f32(out + out_off, r[i]);
    }
    wren_send_response(c, req_id, out, 64);
}

static void handle_parse_csv(struct conn *c, uint32_t req_id, const char *payload, size_t payload_len) {
    const char *text;
    uint32_t text_len;
    if (proto_dec_bytes(payload, payload_len, &text, &text_len) == 0) {
        fprintf(stderr, "parse_csv: bad input\n");
        return;
    }

    // First pass: count rows and fields per row
    // Simple CSV: rows separated by '\n', fields by ','
    // No quoting, no escaping — realistic CSV is more complex but this is
    // a protocol test, not a CSV parser test.

    char *out = malloc(64 * 1024);
    if (!out)
        return;
    size_t out_off = 0;

    size_t row_count_offset = out_off;
    out_off += 4;
    uint32_t row_count = 0;

    uint32_t field_count = 0;
    size_t field_count_offset = out_off;
    out_off += 4;

    const char *field_start = text;
    const char *p = text;
    const char *end = text + text_len;

    while (p <= end) {
        if (p == end || *p == ',' || *p == '\n') {
            uint32_t flen = (uint32_t)(p - field_start);
            out_off += proto_enc_bytes(out + out_off, field_start, flen);
            field_count++;
            if (p == end || *p == '\n') {
                uint32_t fc_n = htonl(field_count);
                memcpy(out + field_count_offset, &fc_n, 4);
                row_count++;

                if (p == end)
                    break;
                field_count = 0;
                field_count_offset = out_off;
                out_off += 4;
                field_start = p + 1;
            } else {
                field_start = p + 1;
            }
        }
        p++;
    }
    uint32_t rc_n = htonl(row_count);
    memcpy(out + row_count_offset, &rc_n, 4);

    wren_send_response(c, req_id, out, out_off);
    free(out);
}

static void handle_concurrent_stress(struct conn *c, uint32_t req_id, const char *payload, size_t payload_len) {
    uint32_t sleep_ms;
    size_t n = proto_dec_u32(payload, payload_len, &sleep_ms);
    if (!n)
        return;

    // WARNING: usleep blocks the entire server thread.
    // This is intentional — the test is: do multiple clients' calls
    // still all complete? (If the event loop is broken, they'd serialize
    // or hang. We expect them to serialize given a single-threaded server,
    // but none should be lost.)
    usleep(sleep_ms * 1000);

    char out[8];
    // Return the sleep duration as proof we actually slept.
    proto_enc_u64(out, sleep_ms);
    wren_send_response(c, req_id, out, 8);
}

static const char *fake_db[][3] = {
    {"1", "alice", "admin"},
    {"2", "bob", "user"},
    {"3", "charlie", "user"},
    {"4", "diana", "admin"},
    {"5", "eve", "user"},
};
static const int fake_db_rows = 5;
static void handle_query_db(struct conn *c, uint32_t req_id, const char *payload, size_t payload_len) {
    // Payload: [sql: string] [param_count: u32] [param: bytes × N]
    const char *sql;
    uint32_t sql_len;
    size_t off = 0, n;

    n = proto_dec_bytes(payload + off, payload_len - off, &sql, &sql_len);
    if (!n)
        return;
    off += n;

    uint32_t param_count;
    n = proto_dec_u32(payload + off, payload_len - off, &param_count);
    if (!n)
        return;
    off += n;

    // Skip params (we don't use them in the fake implementation)
    for (uint32_t i = 0; i < param_count; i++) {
        const char *p;
        uint32_t pl;
        n = proto_dec_bytes(payload + off, payload_len - off, &p, &pl);
        if (!n)
            return;
        off += n;
    }

    // "Parse" the SQL: if it contains "admin", return admin rows only.
    // Otherwise return all rows. This is stubbed for testing.
    int admin_only = 0;
    for (uint32_t i = 0; i + 5 <= sql_len; i++) {
        if (memcmp(sql + i, "admin", 5) == 0) {
            admin_only = 1;
            break;
        }
    }

    // Build response: [row_count: u32] [ [field_count: u32] [field: string × 3] ] × row_count
    char *out = malloc(2048);
    if (!out)
        return;
    size_t out_off = 0;

    size_t row_count_offset = out_off;
    out_off += 4;
    uint32_t row_count = 0;

    for (int i = 0; i < fake_db_rows; i++) {
        if (admin_only && strcmp(fake_db[i][2], "admin") != 0)
            continue;
        out_off += proto_enc_u32(out + out_off, 3); // field count
        for (int j = 0; j < 3; j++) {
            uint32_t flen = (uint32_t)strlen(fake_db[i][j]);
            out_off += proto_enc_bytes(out + out_off, fake_db[i][j], flen);
        }
        row_count++;
    }

    uint32_t rc_n = htonl(row_count);
    memcpy(out + row_count_offset, &rc_n, 4);

    wren_send_response(c, req_id, out, out_off);
    free(out);
}

static void handle_echo_large(struct conn *c, uint32_t req_id, const char *payload, size_t payload_len) {
    const char *data;
    uint32_t len;
    if (proto_dec_bytes(payload, payload_len, &data, &len) == 0) {
        fprintf(stderr, "echo_large: bad payload\n");
        return;
    }

    char *out = malloc(4 + len);
    if (!out)
        return;
    proto_enc_bytes(out, data, len);
    wren_send_response(c, req_id, out, 4 + len);
    free(out);
}

int main() {
    int listen_fd = create_sock(PORT, BACKLOG, (struct sock_opt[]) {{SOL_SOCKET, SO_REUSEADDR, 1}} ,1);
    if (listen_fd < 0) return 1;
    printf("listening on port: %u\n", PORT);

    struct epoll_event events[MAX_EVENTS];
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("main: epoll_create1");
        return -1;
    }
    if (epoll_add(epoll_fd, listen_fd, EPOLLIN, NULL) < 0) return -1;

    wren_register(METHOD_ADD, handle_add);
    wren_register(METHOD_ECHO_BYTES, handle_echo_bytes);
    wren_register(METHOD_NOOP, handle_noop);
    wren_register(METHOD_PI, handle_pi);
    wren_register(METHOD_MUL_I64, handle_mul_i64);
    wren_register(METHOD_MAKE_VEC3, handle_make_vec3);
    wren_register(METHOD_SUM_ARRAY, handle_sum_array);
    wren_register(METHOD_DIVIDE, handle_divide);
    wren_register(METHOD_CONCAT, handle_concat);
    wren_register(METHOD_IS_EVEN, handle_is_even);
    wren_register(METHOD_MATMUL_4X4, handle_matmul_4x4);
    wren_register(METHOD_PARSE_CSV, handle_parse_csv);
    wren_register(METHOD_CONCURRENT_STRESS, handle_concurrent_stress);
    wren_register(METHOD_QUERY_DB, handle_query_db);
    wren_register(METHOD_ECHO_LARGE, handle_echo_large);

    for (;;) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("main: epoll_wait"); return -1;
        }
        for (int i = 0; i < n; i++) {
            if (events[i].data.ptr == NULL) {
                if (handle_accept(epoll_fd, listen_fd) < 0) return 1;
            } else {
                handle_conn_event(events[i].data.ptr, events[i].events);
            }
        }
    }
    return 0;
}