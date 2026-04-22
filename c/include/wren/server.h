#pragma once

#include "wren/proto.h"

int wren_send_response(struct conn *c, uint32_t req_id, const char *payload, size_t payload_len);

void wren_register(uint16_t method_id, handler_fn fn);
void wren_dispatch(struct conn *c, const struct msg_header *hdr, const char *payload, size_t payload_len);