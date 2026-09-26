#include "wren/codec.h"
#include "wren/proto.h"

#include <stdlib.h>
#include <string.h>

#define ARENA_BLOCK_MIN 4096

struct wren_arena_block {
    struct wren_arena_block *next;
    size_t used;
    size_t cap;
    max_align_t data[];
};

void *wren_arena_alloc(wren_arena_t *a, size_t count, size_t size) {
    const size_t align = _Alignof(max_align_t);
    if (size && count > SIZE_MAX / size)
        return NULL;
    size_t n = count * size;
    if (n > SIZE_MAX - align)
        return NULL;
    n = (n + align - 1) & ~(align - 1);
    if (n == 0)
        n = align;

    struct wren_arena_block *b = a->head;
    if (!b || b->cap - b->used < n) {
        size_t cap = n > ARENA_BLOCK_MIN ? n : ARENA_BLOCK_MIN;
        if (cap > SIZE_MAX - sizeof(*b))
            return NULL;
        b = malloc(sizeof(*b) + cap);
        if (!b)
            return NULL;
        b->next = a->head;
        b->used = 0;
        b->cap = cap;
        a->head = b;
    }
    void *p = (char *)b->data + b->used;
    b->used += n;
    memset(p, 0, n);
    return p;
}

void wren_arena_free(wren_arena_t *a) {
    struct wren_arena_block *b = a->head;
    while (b) {
        struct wren_arena_block *next = b->next;
        free(b);
        b = next;
    }
    a->head = NULL;
}

void wren_reader_init(wren_reader_t *r, const char *buf, size_t len) {
    r->buf = buf;
    r->len = len;
    r->pos = 0;
    r->ok = true;
}

bool wren_reader_done(const wren_reader_t *r) {
    return r->ok && r->pos == r->len;
}

#define DEFINE_READ(name, type)                                             \
    void wren_read_##name(wren_reader_t *r, type *out) {                    \
        size_t n = r->ok ? proto_dec_##name(r->buf + r->pos,                \
                                            r->len - r->pos, out) : 0;      \
        if (n == 0) {                                                       \
            r->ok = false;                                                  \
            *out = 0;                                                       \
            return;                                                         \
        }                                                                   \
        r->pos += n;                                                        \
    }

#define DEFINE_READ_SIGNED(name, type, uname, utype)                        \
    void wren_read_##name(wren_reader_t *r, type *out) {                    \
        utype u;                                                            \
        wren_read_##uname(r, &u);                                           \
        *out = (type)u;                                                     \
    }

DEFINE_READ(u8, uint8_t)
DEFINE_READ(u16, uint16_t)
DEFINE_READ(u32, uint32_t)
DEFINE_READ(u64, uint64_t)
DEFINE_READ(f32, float)
DEFINE_READ(f64, double)
DEFINE_READ_SIGNED(i8, int8_t, u8, uint8_t)
DEFINE_READ_SIGNED(i16, int16_t, u16, uint16_t)
DEFINE_READ_SIGNED(i32, int32_t, u32, uint32_t)
DEFINE_READ_SIGNED(i64, int64_t, u64, uint64_t)

void wren_read_bool(wren_reader_t *r, bool *out) {
    uint8_t v;
    wren_read_u8(r, &v);
    *out = (v != 0);   /* PROTOCOL.md: any nonzero byte is true */
}

void wren_read_bytes(wren_reader_t *r, wren_bytes_t *out) {
    size_t n = r->ok ? proto_dec_bytes(r->buf + r->pos, r->len - r->pos,
                                       &out->data, &out->len) : 0;
    if (n == 0) {
        r->ok = false;
        out->data = NULL;
        out->len = 0;
        return;
    }
    r->pos += n;
}

void wren_read_count(wren_reader_t *r, size_t elem_min_size, uint32_t *out) {
    wren_read_u32(r, out);
    if (elem_min_size == 0)
        elem_min_size = 1;   /* arrays of empty structs: bound by bytes left */
    if (r->ok && (size_t)*out > (r->len - r->pos) / elem_min_size) {
        r->ok = false;
        *out = 0;
    }
}

void *wren_read_alloc(wren_reader_t *r, wren_arena_t *a, size_t count, size_t size) {
    if (!r->ok)
        return NULL;
    void *p = wren_arena_alloc(a, count, size);
    if (!p)
        r->ok = false;
    return p;
}

void wren_writer_init(wren_writer_t *w) {
    w->buf = NULL;
    w->len = 0;
    w->cap = 0;
    w->ok = true;
}

void wren_writer_free(wren_writer_t *w) {
    free(w->buf);
    wren_writer_init(w);
}

static char *reserve(wren_writer_t *w, size_t n) {
    if (!w->ok)
        return NULL;
    if (n > SIZE_MAX - w->len) {
        w->ok = false;
        return NULL;
    }
    if (w->cap - w->len < n) {
        size_t cap = w->cap ? w->cap : 64;
        while (cap - w->len < n) {
            if (cap > SIZE_MAX / 2) {
                w->ok = false;
                return NULL;
            }
            cap *= 2;
        }
        char *buf = realloc(w->buf, cap);
        if (!buf) {
            w->ok = false;
            return NULL;
        }
        w->buf = buf;
        w->cap = cap;
    }
    char *p = w->buf + w->len;
    w->len += n;
    return p;
}

#define DEFINE_WRITE(name, type, size)                                      \
    void wren_write_##name(wren_writer_t *w, type v) {                      \
        char *p = reserve(w, size);                                         \
        if (p)                                                              \
            proto_enc_##name(p, v);                                         \
    }

#define DEFINE_WRITE_SIGNED(name, type, uname, utype)                       \
    void wren_write_##name(wren_writer_t *w, type v) {                      \
        wren_write_##uname(w, (utype)v);                                    \
    }

DEFINE_WRITE(u8, uint8_t, 1)
DEFINE_WRITE(u16, uint16_t, 2)
DEFINE_WRITE(u32, uint32_t, 4)
DEFINE_WRITE(u64, uint64_t, 8)
DEFINE_WRITE(f32, float, 4)
DEFINE_WRITE(f64, double, 8)
DEFINE_WRITE_SIGNED(i8, int8_t, u8, uint8_t)
DEFINE_WRITE_SIGNED(i16, int16_t, u16, uint16_t)
DEFINE_WRITE_SIGNED(i32, int32_t, u32, uint32_t)
DEFINE_WRITE_SIGNED(i64, int64_t, u64, uint64_t)

void wren_write_bool(wren_writer_t *w, bool v) {
    wren_write_u8(w, v ? 1 : 0);
}

void wren_write_bytes(wren_writer_t *w, wren_bytes_t v) {
    char *p = reserve(w, 4 + (size_t)v.len);
    if (!p)
        return;
    proto_enc_u32(p, v.len);
    if (v.len)
        memcpy(p + 4, v.data, v.len);
}