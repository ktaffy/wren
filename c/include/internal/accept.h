#pragma once

struct wren_server;

/**
 * Drain the accept queue on the server's listen fd, registering each
 * new connection with the server's epoll instance for edge-triggered
 * read events (EPOLLIN | EPOLLET | EPOLLRDHUP). Stores a pointer back
 * to the server on each new connection so the dispatch layer can reach
 * it from within conn.c.
 *
 * Per-client errors (ECONNABORTED, EPROTO, EPERM) are logged and skipped
 * so one bad client can't take down the drain. EINTR is retried.
 *
 * @param s  Server whose listen queue to drain.
 * @return 0 on clean drain, -1 on fatal error (EMFILE/ENFILE/unexpected).
 */
int handle_accept(struct wren_server *s);