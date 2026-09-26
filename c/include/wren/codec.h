// wren/codec.h
/**
 * Typed encode/decode helpers used by wrengen-generated C code.
 *
 * wren_reader_t walks a payload with a cursor. wren_writer_t builds one in a
 * growable buffer. after the first failure every further
 * operation is a no-op, so generated code checks ok once at the end.
 * Failed reads zero their output, so a failed count can never drive a loop.
 *
 * wren_arena_t owns memory for decoded variable-length arrays. Everything
 * allocated from one arena is released together by wren_arena_free.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *data;
    uint32_t len;
} wren_bytes_t;

typedef struct wren_arena_block wren_arena_block_t;

typedef struct {
    wren_arena_block_t *head;
} wren_arena_t;

void *wren_arena_alloc(wren_arena_t *a, size_t count, size_t size);
void wren_arena_free(wren_arena_t *a);

typedef struct {
    const char *buf;
    size_t len;
    size_t pos;
    bool ok;
} wren_reader_t;

void wren_reader_init(wren_reader_t *r, const char *buf, size_t len);
bool wren_reader_done(const wren_reader_t *r);

void wren_read_bool(wren_reader_t *r, bool *out);
void wren_read_u8(wren_reader_t *r, uint8_t *out);
void wren_read_u16(wren_reader_t *r, uint16_t *out);
void wren_read_u32(wren_reader_t *r, uint32_t *out);
void wren_read_u64(wren_reader_t *r, uint64_t *out);
void wren_read_i8(wren_reader_t *r, int8_t *out);
void wren_read_i16(wren_reader_t *r, int16_t *out);
void wren_read_i32(wren_reader_t *r, int32_t *out);
void wren_read_i64(wren_reader_t *r, int64_t *out);
void wren_read_f32(wren_reader_t *r, float *out);
void wren_read_f64(wren_reader_t *r, double *out);
void wren_read_bytes(wren_reader_t *r, wren_bytes_t *out);

void wren_read_count(wren_reader_t *r, size_t elem_min_size, uint32_t *out);
void *wren_read_alloc(wren_reader_t *r, wren_arena_t *a, size_t count, size_t size);

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
    bool ok;
} wren_writer_t;

void wren_writer_init(wren_writer_t *w);
void wren_writer_free(wren_writer_t *w);

void wren_write_bool(wren_writer_t *w, bool v);
void wren_write_u8(wren_writer_t *w, uint8_t v);
void wren_write_u16(wren_writer_t *w, uint16_t v);
void wren_write_u32(wren_writer_t *w, uint32_t v);
void wren_write_u64(wren_writer_t *w, uint64_t v);
void wren_write_i8(wren_writer_t *w, int8_t v);
void wren_write_i16(wren_writer_t *w, int16_t v);
void wren_write_i32(wren_writer_t *w, int32_t v);
void wren_write_i64(wren_writer_t *w, int64_t v);
void wren_write_f32(wren_writer_t *w, float v);
void wren_write_f64(wren_writer_t *w, double v);
void wren_write_bytes(wren_writer_t *w, wren_bytes_t v);