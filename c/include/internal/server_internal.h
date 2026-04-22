#pragma once

#include "wren/proto.h"

struct wren_server;
struct conn;
struct wren_call;

/**
 * Invoke the given handler for an incoming CALL. Constructs an internal
 * wren_call for the duration of the handler.
 *
 * @param c            Connection the call arrived on.
 * @param req_id       Request id from the incoming header.
 * @param payload      Payload bytes.
 * @param payload_len  Payload length.
 * @param fn           Handler to invoke. Must not be NULL.
 */
void call_dispatch(struct conn *c, uint32_t req_id, const char *payload, size_t payload_len, handler_fn fn);

/**
 * Register a handler on the given server. See wren_server_register for
 * the public wrapper.
 *
 * @param s          Server to register on.
 * @param method_id  Non-zero method identifier (1..MAX_METHODS-1).
 * @param fn         Handler function pointer.
 * @return 0 on success, -1 on error.
 */
int srv_register(struct wren_server *s, uint16_t method_id, handler_fn fn);

/**
 * Dispatch a received CALL message to the registered handler.
 *
 * @param s            Server owning the handler table.
 * @param c            Connection the call arrived on.
 * @param hdr          Parsed message header.
 * @param payload      Pointer to payload bytes (inside conn's in_buf).
 * @param payload_len  Length of payload in bytes.
 */
void srv_dispatch(struct wren_server *s, struct conn *c, const struct msg_header *hdr, const char *payload, size_t payload_len);

/**
 * Return the handler registered at @p method_id, or NULL if none is
 * registered or the id is out of range.
 */
handler_fn srv_handler_get(struct wren_server *s, uint16_t method_id);

/**
 * Set the handler at @p method_id. Caller is responsible for bounds and
 * reservation checks (see srv_register for the validated entry point).
 */
void srv_handler_set(struct wren_server *s, uint16_t method_id, handler_fn fn);

/**
 * Return the listen socket fd. Used by handle_accept.
 */
int srv_listen_fd(struct wren_server *s);

/**
 * Return the epoll instance fd. Used by handle_accept.
 */
int srv_epoll_fd(struct wren_server *s);