#include "wren/server.h"
#include "internal/conn.h"

#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>

static handler_fn handlers[MAX_METHODS];

int wren_send_response(struct conn *c, uint32_t req_id, const char *payload, size_t payload_len)
{
    char header[MSG_HEADER_SIZE];
    uint32_t total_len = MSG_HEADER_SIZE + (uint32_t)payload_len;

    uint32_t len_n = htonl(total_len);
    uint16_t method_n = 0;
    uint32_t req_n = htonl(req_id);

    memcpy(header + 0, &len_n, 4);
    header[4] = MSG_TYPE_RESPONSE;
    header[5] = 0;
    memcpy(header + 6, &method_n, 2);
    memcpy(header + 8, &req_n, 4);

    if (conn_write(c, header, MSG_HEADER_SIZE) < 0)
        return -1;
    if (payload_len > 0)
    {
        if (conn_write(c, payload, payload_len) < 0)
            return -1;
    }
    return 0;
}

void wren_register(uint16_t method_id, handler_fn fn)
{
    if (method_id == 0)
    {
        fprintf(stderr, "register: method_id 0 is reserved\n");
        return;
    }
    if (method_id < MAX_METHODS)
    {
        handlers[method_id] = fn;
    }
}

// TODO: send ERROR response to client; currently the client will hang
//       or time out waiting for a reply that never comes.
void wren_dispatch(struct conn *c, const struct msg_header *hdr, const char *payload, size_t payload_len)
{
    if (hdr->method_id == 0)
    {
        fprintf(stderr, "dispatch: method_id 0 is reserved\n");
        return;
    }
    if (hdr->method_id >= MAX_METHODS || !handlers[hdr->method_id])
    {
        fprintf(stderr, "dispatch: no handler for method %u\n", hdr->method_id);
        return;
    }
    handlers[hdr->method_id](c, hdr->req_id, payload, payload_len);
}