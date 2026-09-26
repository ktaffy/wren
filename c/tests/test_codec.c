#include "wren/codec.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int checks, failures;

#define CHECK(cond)                                                         \
    do {                                                                    \
        checks++;                                                           \
        if (!(cond)) {                                                      \
            failures++;                                                     \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n",                    \
                    __FILE__, __LINE__, #cond);                             \
        }                                                                   \
    } while (0)

static void test_network_byte_order(void) {
    wren_writer_t w;
    wren_writer_init(&w);
    wren_write_u32(&w, 7);
    wren_write_bytes(&w, (wren_bytes_t){NULL, 0});
    CHECK(w.ok && w.len == 8);
    CHECK(memcmp(w.buf, "\x00\x00\x00\x07" "\x00\x00\x00\x00", 8) == 0);
    wren_writer_free(&w);
}

static void test_round_trip_primitives(void) {
    wren_writer_t w;
    wren_writer_init(&w);
    wren_write_bool(&w, true);
    wren_write_u8(&w, UINT8_MAX);
    wren_write_i8(&w, INT8_MIN);
    wren_write_u16(&w, UINT16_MAX);
    wren_write_i16(&w, INT16_MIN);
    wren_write_u32(&w, UINT32_MAX);
    wren_write_i32(&w, INT32_MIN);
    wren_write_u64(&w, UINT64_MAX);
    wren_write_i64(&w, INT64_MIN);
    wren_write_f32(&w, 1.5f);
    wren_write_f64(&w, 3.141592653589793);
    wren_write_bytes(&w, (wren_bytes_t){"hi", 2});
    CHECK(w.ok);

    wren_reader_t r;
    wren_reader_init(&r, w.buf, w.len);
    bool b; uint8_t u8; int8_t i8; uint16_t u16; int16_t i16;
    uint32_t u32; int32_t i32; uint64_t u64; int64_t i64;
    float f32; double f64; wren_bytes_t s;
    wren_read_bool(&r, &b);
    wren_read_u8(&r, &u8);
    wren_read_i8(&r, &i8);
    wren_read_u16(&r, &u16);
    wren_read_i16(&r, &i16);
    wren_read_u32(&r, &u32);
    wren_read_i32(&r, &i32);
    wren_read_u64(&r, &u64);
    wren_read_i64(&r, &i64);
    wren_read_f32(&r, &f32);
    wren_read_f64(&r, &f64);
    wren_read_bytes(&r, &s);

    CHECK(b == true);
    CHECK(u8 == UINT8_MAX && i8 == INT8_MIN);
    CHECK(u16 == UINT16_MAX && i16 == INT16_MIN);
    CHECK(u32 == UINT32_MAX && i32 == INT32_MIN);
    CHECK(u64 == UINT64_MAX && i64 == INT64_MIN);
    CHECK(f32 == 1.5f && f64 == 3.141592653589793);
    CHECK(s.len == 2 && memcmp(s.data, "hi", 2) == 0);
    CHECK(s.data == w.buf + w.len - 2);
    CHECK(wren_reader_done(&r));
    wren_writer_free(&w);
}

static void test_failure_is_sticky_and_zeroes(void) {
    wren_reader_t r;
    wren_reader_init(&r, "\x00\x00", 2);
    uint32_t v = 123;
    wren_read_u32(&r, &v);
    CHECK(!r.ok && v == 0);
    uint8_t b = 9;
    wren_read_u8(&r, &b);
    CHECK(!r.ok && b == 0);
    CHECK(!wren_reader_done(&r));
}

static void test_bytes_length_beyond_buffer(void) {
    wren_reader_t r;
    wren_reader_init(&r, "\x00\x00\x00\x05" "ab", 6);
    wren_bytes_t s;
    wren_read_bytes(&r, &s);
    CHECK(!r.ok && s.data == NULL && s.len == 0);
}

static void test_count_guard(void) {
    wren_reader_t r;
    uint32_t n;
    wren_reader_init(&r, "\x00\x00\x03\xe8" "\x00\x00\x00\x01", 8);
    wren_read_count(&r, 4, &n);
    CHECK(!r.ok && n == 0);
    wren_reader_init(&r, "\x00\x00\x00\x01" "\x00\x00\x00\x01", 8);
    wren_read_count(&r, 4, &n);
    CHECK(r.ok && n == 1);
}

static void test_trailing_bytes_not_done(void) {
    wren_reader_t r;
    wren_reader_init(&r, "\x00\x00\x00\x01" "\xff", 5);
    uint32_t v;
    wren_read_u32(&r, &v);
    CHECK(r.ok && v == 1 && !wren_reader_done(&r));
}

static void test_arena(void) {
    wren_arena_t a = {0};
    const size_t align = _Alignof(max_align_t);
    char *p1 = wren_arena_alloc(&a, 3, 1);
    char *p2 = wren_arena_alloc(&a, 1, 8);
    CHECK(p1 && p2 && p1 != p2);
    CHECK((uintptr_t)p1 % align == 0 && (uintptr_t)p2 % align == 0);
    CHECK(p1[0] == 0 && p1[2] == 0);
    char *big = wren_arena_alloc(&a, 100000, 1);
    CHECK(big && big[99999] == 0);
    CHECK(wren_arena_alloc(&a, SIZE_MAX, 2) == NULL);
    wren_arena_free(&a);
    CHECK(a.head == NULL);
}

int main(void) {
    test_network_byte_order();
    test_round_trip_primitives();
    test_failure_is_sticky_and_zeroes();
    test_bytes_length_beyond_buffer();
    test_count_guard();
    test_trailing_bytes_not_done();
    test_arena();
    printf("test_codec: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}