#include "internal/server_internal.h"
#include "internal/conn.h"
#include "wren/server.h"

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

int srv_register(struct wren_server *s, uint16_t method_id, handler_fn fn, void *ctx) {
    if (!s)
        return -1;
    if (method_id == 0) {
        fprintf(stderr, "register: method_id 0 is reserved\n");
        return -1;
    }
    if (method_id >= MAX_METHODS)
        return -1;
    srv_handler_set(s, method_id, fn, ctx);
    return 0;
}

void srv_dispatch(struct wren_server *s, struct conn *c, const struct msg_header *hdr, const char *payload, size_t payload_len) {
    if (hdr->method_id == 0) {
        call_reply_error(c, hdr->req_id, WREN_ERR_METHOD_RESERVED, "method_id 0 is reserved");
        return;
    }
    void *ctx = NULL;
    handler_fn fn = srv_handler_get(s, hdr->method_id, &ctx);
    if (!fn) {
        call_reply_error(c, hdr->req_id, WREN_ERR_METHOD_NOT_FOUND, "no handler registered for method_id");
        return;
    }

    call_dispatch(c, hdr->req_id, payload, payload_len, fn, ctx);
}