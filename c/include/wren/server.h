#pragma once

#include "wren/proto.h"

/**
 * Opaque server handle. Create with wren_server_create, destroy with
 * wren_server_destroy. All public server functions operate on this handle.
 *
 * The struct definition is private to server.c; callers never touch its
 * fields directly.
 */
typedef struct wren_server wren_server_t;

/**
 * Create a new wren server listening on the given TCP port.
 *
 * Allocates the server, creates a non-blocking listening socket bound to
 * INADDR_ANY on @p port, and initializes an epoll instance. Does not start
 * accepting connections — call wren_server_run for that.
 *
 * @param port  TCP port to listen on (host byte order).
 * @return Pointer to a new server on success, NULL on failure (port in
 *         use, out of resources, etc). Caller must destroy with
 *         wren_server_destroy.
 */
wren_server_t *wren_server_create(uint16_t port);

/**
 * Register a handler for a method_id on the server.
 *
 * Must be called before wren_server_run. Method IDs must be in the range
 * 1..MAX_METHODS-1; method_id 0 is reserved per the wren protocol spec.
 *
 * @param s          Server handle.
 * @param method_id  Non-zero method identifier.
 * @param fn         Handler function. Called by the server when a CALL
 *                   with this method_id arrives.
 * @return 0 on success, -1 on error (NULL server, reserved or
 *         out-of-range method_id).
 */
int wren_server_register(wren_server_t *s, uint16_t method_id, handler_fn fn);

/**
 * Run the server's event loop.
 *
 * Blocks the calling thread until the loop exits (e.g., on unrecoverable
 * error). Accepts incoming connections, reads framed messages, and
 * dispatches CALL messages to registered handlers.
 *
 * @param s  Server handle.
 * @return 0 on clean shutdown (currently unreachable — server runs
 *         forever), -1 on fatal error.
 */
int wren_server_run(wren_server_t *s);

/**
 * Destroy a server and release all associated resources.
 *
 * Closes the listening socket and epoll instance, frees the server
 * struct. Safe to call with a NULL pointer. Does not close active client
 * connections (TODO: track and close cleanly).
 *
 * @param s  Server to destroy. May be NULL.
 */
void wren_server_destroy(wren_server_t *s);

/**
 * Send a RESPONSE message on a connection.
 *
 * Called from within a handler to reply to the current call. The response
 * type is set to MSG_TYPE_RESPONSE; the req_id echoes the value from the
 * original CALL. Handler signatures will pass req_id in; stage 2 will
 * wrap this behind a higher-level wren_call_reply API.
 *
 * @param c            Connection to send on.
 * @param req_id       Request ID from the incoming CALL.
 * @param payload      Response payload bytes (may be NULL if payload_len is 0).
 * @param payload_len  Length of payload in bytes.
 * @return 0 on success, -1 on error (send failed, out of memory).
 */
int wren_send_response(struct conn *c, uint32_t req_id, const char *payload, size_t payload_len);