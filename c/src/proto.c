#include "wren/proto.h"

#include <arpa/inet.h>
#include <string.h>

void msg_parse_h(const char *buf, struct msg_header *out)
{
    uint32_t length, req_id;
    uint16_t method_id;

    memcpy(&length, buf + 0, 4);
    out->type = (uint8_t)buf[4];
    out->flags = (uint8_t)buf[5];
    memcpy(&method_id, buf + 6, 2);
    memcpy(&req_id, buf + 8, 4);

    out->length = ntohl(length);
    out->method_id = ntohs(method_id);
    out->req_id = ntohl(req_id);
}

size_t proto_enc_u8(char *buf, uint8_t v)
{
    buf[0] = (char)v;
    return 1;
}
size_t proto_enc_u16(char *buf, uint16_t v)
{
    uint16_t n = htons(v);
    memcpy(buf, &n, 2);
    return 2;
}
size_t proto_enc_u32(char *buf, uint32_t v)
{
    uint32_t n = htonl(v);
    memcpy(buf, &n, 4);
    return 4;
}
size_t proto_enc_u64(char *buf, uint64_t v)
{
    uint32_t hi = htonl((uint32_t)(v >> 32));
    uint32_t lo = htonl((uint32_t)(v & 0xFFFFFFFF));
    memcpy(buf, &hi, 4);
    memcpy(buf + 4, &lo, 4);
    return 8;
}
size_t proto_enc_f32(char *buf, float v)
{
    uint32_t bits;
    memcpy(&bits, &v, 4);
    bits = htonl(bits);
    memcpy(buf, &bits, 4);
    return 4;
}
size_t proto_enc_f64(char *buf, double v)
{
    uint64_t bits;
    memcpy(&bits, &v, 8);
    uint32_t hi = htonl((uint32_t)(bits >> 32));
    uint32_t lo = htonl((uint32_t)(bits & 0xFFFFFFFF));
    memcpy(buf, &hi, 4);
    memcpy(buf + 4, &lo, 4);
    return 8;
}
size_t proto_enc_bytes(char *buf, const char *data, uint32_t len)
{
    proto_enc_u32(buf, len);
    memcpy(buf + 4, data, len);
    return 4 + len;
}

size_t proto_dec_u8(const char *buf, size_t avail, uint8_t *out)
{
    if (avail < 1)
        return 0;
    *out = (uint8_t)buf[0];
    return 1;
}
size_t proto_dec_u16(const char *buf, size_t avail, uint16_t *out)
{
    if (avail < 2)
        return 0;
    uint16_t n;
    memcpy(&n, buf, 2);
    *out = ntohs(n);
    return 2;
}
size_t proto_dec_u32(const char *buf, size_t avail, uint32_t *out)
{
    if (avail < 4)
        return 0;
    uint32_t n;
    memcpy(&n, buf, 4);
    *out = ntohl(n);
    return 4;
}
size_t proto_dec_u64(const char *buf, size_t avail, uint64_t *out)
{
    if (avail < 8)
        return 0;
    uint32_t hi, lo;
    memcpy(&hi, buf, 4);
    memcpy(&lo, buf + 4, 4);
    *out = ((uint64_t)ntohl(hi) << 32) | ntohl(lo);
    return 8;
}
size_t proto_dec_f32(const char *buf, size_t avail, float *out)
{
    if (avail < 4)
        return 0;
    uint32_t bits;
    memcpy(&bits, buf, 4);
    bits = ntohl(bits);
    memcpy(out, &bits, 4);
    return 4;
}
size_t proto_dec_f64(const char *buf, size_t avail, double *out)
{
    if (avail < 8)
        return 0;
    uint32_t hi, lo;
    memcpy(&hi, buf, 4);
    memcpy(&lo, buf + 4, 4);
    uint64_t bits = ((uint64_t)ntohl(hi) << 32) | ntohl(lo);
    memcpy(out, &bits, 8);
    return 8;
}
size_t proto_dec_bytes(const char *buf, size_t avail, const char **out_data, uint32_t *out_len)
{
    uint32_t len;
    size_t n = proto_dec_u32(buf, avail, &len);
    if (n == 0)
        return 0;
    if (avail - n < len)
        return 0;
    *out_data = buf + n;
    *out_len = len;
    return n + len;
}