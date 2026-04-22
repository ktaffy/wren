#pragma once

#include <stdint.h>
#include <stddef.h>

// TODO(Stage 2): fixed 4 KiB limit. Spec recommends 64 MiB per message.
//                  Requires heap-allocated, growable in_buf.
#define CONN_IN_BUF_SIZE 4096

struct wren_server;

/**
 * Per-connection state. One is allocated in handle_accept for each
 * accepted client and freed in handle_conn_event when the connection
 * closes. The pointer is stored in epoll's ev.data.ptr so epoll_wait
 * hands it back directly on every event.
 */
struct conn {
    int fd;
    int epoll_fd;
    struct wren_server *server;

    char in_buf[CONN_IN_BUF_SIZE];
    size_t in_len;

    char *out_buf;
    size_t out_len;
    size_t out_sent;
};

/**
 * Handle an epoll event on a connection fd. Drains the read buffer
 * (edge-triggered, so loops until EAGAIN), echoes bytes back, and
 * closes the fd on hangup, error, or peer shutdown.
 *
 * Handles EPOLLERR, EPOLLHUP, EPOLLIN, and EPOLLRDHUP from @p events.
 * Short writes are not yet buffered — bytes past the first send() call
 * can be lost under backpressure (see TODO in conn.c).
 *
 * @param fd     Connection fd, registered edge-triggered with epoll.
 * @param events Event mask from epoll_wait for this fd.
 */
void handle_conn_event(struct conn *c, uint32_t events);

/**
 * Send bytes to the connection. Sends directly if possible; if the kernel
 * can't accept everything, queues the remainder and arms EPOLLOUT so the
 * event loop will flush later.
 *
 * @return 0 on success (sent or queued), -1 on fatal error.
 */
int conn_write(struct conn *c, const char *data, size_t len);