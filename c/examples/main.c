#include "wren/server.h"
#include "wren/call.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

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

static void handle_add(wren_call_t *call) {
    uint32_t a, b;
    wren_call_read_u32(call, &a);
    wren_call_read_u32(call, &b);
    wren_call_reply_u32(call, a + b);
}

static void handle_echo_bytes(wren_call_t *call) {
    const char *data;
    uint32_t len;
    wren_call_read_bytes(call, &data, &len);
    wren_call_reply_bytes(call, data, len);
}

static void handle_noop(wren_call_t *call) {
    wren_call_reply_empty(call);
}

static void handle_pi(wren_call_t *call) {
    wren_call_reply_f64(call, 3.141592653589793);
}

static void handle_mul_i64(wren_call_t *call) {
    int64_t a, b;
    wren_call_read_i64(call, &a);
    wren_call_read_i64(call, &b);
    wren_call_reply_i64(call, a * b);
}

static void handle_make_vec3(wren_call_t *call) {
    float x, y, z;
    wren_call_read_f32(call, &x);
    wren_call_read_f32(call, &y);
    wren_call_read_f32(call, &z);

    // Multi-field response: build raw and send.
    char out[12];
    size_t off = 0;
    off += proto_enc_f32(out + off, x * 2);
    off += proto_enc_f32(out + off, y * 2);
    off += proto_enc_f32(out + off, z * 2);
    wren_call_reply_raw(call, out, off);
}

static void handle_sum_array(wren_call_t *call) {
    uint32_t count;
    wren_call_read_u32(call, &count);

    uint64_t total = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t v;
        wren_call_read_u32(call, &v);
        total += v;
    }
    wren_call_reply_u64(call, total);
}

static void handle_divide(wren_call_t *call) {
    uint32_t a, b;
    wren_call_read_u32(call, &a);
    wren_call_read_u32(call, &b);

    if (b == 0) {
        wren_call_reply_error(call, 42, "division by zero");
        return;
    }
    wren_call_reply_u32(call, a / b);
}

static void handle_concat(wren_call_t *call) {
    const char *s1, *s2;
    uint32_t n1, n2;
    wren_call_read_bytes(call, &s1, &n1);
    wren_call_read_bytes(call, &s2, &n2);

    uint32_t total = n1 + n2;
    char *buf = malloc(total);
    if (!buf)
    {
        wren_call_reply_error(call, 1, "out of memory");
        return;
    }
    memcpy(buf, s1, n1);
    memcpy(buf + n1, s2, n2);
    wren_call_reply_bytes(call, buf, total);
    free(buf);
}

static void handle_is_even(wren_call_t *call) {
    uint32_t v;
    wren_call_read_u32(call, &v);
    wren_call_reply_bool(call, v % 2 == 0);
}

static void handle_matmul_4x4(wren_call_t *call) {
    float a[16], b[16], r[16];
    for (int i = 0; i < 16; i++)
        wren_call_read_f32(call, &a[i]);
    for (int i = 0; i < 16; i++)
        wren_call_read_f32(call, &b[i]);

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            float sum = 0;
            for (int k = 0; k < 4; k++)
                sum += a[i * 4 + k] * b[k * 4 + j];
            r[i * 4 + j] = sum;
        }
    }

    char out[64];
    size_t off = 0;
    for (int i = 0; i < 16; i++)
        off += proto_enc_f32(out + off, r[i]);
    wren_call_reply_raw(call, out, off);
}

static void handle_parse_csv(wren_call_t *call) {
    const char *text;
    uint32_t text_len;
    wren_call_read_bytes(call, &text, &text_len);

    char *out = malloc(64 * 1024);
    if (!out) {
        wren_call_reply_error(call, 1, "out of memory");
        return;
    }
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

    wren_call_reply_raw(call, out, out_off);
    free(out);
}

static void handle_concurrent_stress(wren_call_t *call) {
    uint32_t sleep_ms;
    wren_call_read_u32(call, &sleep_ms);
    usleep(sleep_ms * 1000);
    wren_call_reply_u64(call, sleep_ms);
}

static const char *fake_db[][3] = {
    {"1", "alice", "admin"},
    {"2", "bob", "user"},
    {"3", "charlie", "user"},
    {"4", "diana", "admin"},
    {"5", "eve", "user"},
};
static const int fake_db_rows = 5;

static void handle_query_db(wren_call_t *call) {
    const char *sql;
    uint32_t sql_len;
    wren_call_read_bytes(call, &sql, &sql_len);

    uint32_t param_count;
    wren_call_read_u32(call, &param_count);

    for (uint32_t i = 0; i < param_count; i++) {
        const char *p;
        uint32_t pl;
        wren_call_read_bytes(call, &p, &pl);
    }

    int admin_only = 0;
    for (uint32_t i = 0; i + 5 <= sql_len; i++) {
        if (memcmp(sql + i, "admin", 5) == 0) {
            admin_only = 1;
            break;
        }
    }

    char *out = malloc(2048);
    if (!out) {
        wren_call_reply_error(call, 1, "out of memory");
        return;
    }
    size_t out_off = 0;

    size_t row_count_offset = out_off;
    out_off += 4;
    uint32_t row_count = 0;

    for (int i = 0; i < fake_db_rows; i++) {
        if (admin_only && strcmp(fake_db[i][2], "admin") != 0)
            continue;

        out_off += proto_enc_u32(out + out_off, 3);
        for (int j = 0; j < 3; j++) {
            uint32_t flen = (uint32_t)strlen(fake_db[i][j]);
            out_off += proto_enc_bytes(out + out_off, fake_db[i][j], flen);
        }
        row_count++;
    }

    uint32_t rc_n = htonl(row_count);
    memcpy(out + row_count_offset, &rc_n, 4);

    wren_call_reply_raw(call, out, out_off);
    free(out);
}

static void handle_echo_large(wren_call_t *call) {
    const char *data;
    uint32_t len;
    wren_call_read_bytes(call, &data, &len);
    wren_call_reply_bytes(call, data, len);
}

int main() {
    wren_server_t *s = wren_server_create(PORT);
    if (!s) {
        fprintf(stderr, "failed to create server\n");
        return 1;
    }

    wren_server_register(s, METHOD_ADD, handle_add);
    wren_server_register(s, METHOD_ECHO_BYTES, handle_echo_bytes);
    wren_server_register(s, METHOD_NOOP, handle_noop);
    wren_server_register(s, METHOD_PI, handle_pi);
    wren_server_register(s, METHOD_MUL_I64, handle_mul_i64);
    wren_server_register(s, METHOD_MAKE_VEC3, handle_make_vec3);
    wren_server_register(s, METHOD_SUM_ARRAY, handle_sum_array);
    wren_server_register(s, METHOD_DIVIDE, handle_divide);
    wren_server_register(s, METHOD_CONCAT, handle_concat);
    wren_server_register(s, METHOD_IS_EVEN, handle_is_even);
    wren_server_register(s, METHOD_MATMUL_4X4, handle_matmul_4x4);
    wren_server_register(s, METHOD_PARSE_CSV, handle_parse_csv);
    wren_server_register(s, METHOD_CONCURRENT_STRESS, handle_concurrent_stress);
    wren_server_register(s, METHOD_QUERY_DB, handle_query_db);
    wren_server_register(s, METHOD_ECHO_LARGE, handle_echo_large);

    int rc = wren_server_run(s);
    wren_server_destroy(s);
    return rc;
}