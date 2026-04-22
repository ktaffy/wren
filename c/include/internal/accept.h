#pragma once

/**
 * Drain the accept queue on @p listen_fd, registering each new connection
 * with @p epoll_fd for edge-triggered read events (EPOLLIN | EPOLLET |
 * EPOLLRDHUP). Assumes @p listen_fd is non-blocking; loops until accept4
 * returns EAGAIN/EWOULDBLOCK.
 *
 * Per-client errors (ECONNABORTED, EPROTO, EPERM) are logged and skipped
 * so one bad client can't take down the drain. EINTR is retried.
 *
 * @return 0 on clean drain (queue emptied), -1 on fatal error such as
 *         EMFILE/ENFILE or an unexpected errno. Caller decides whether
 *         to exit.
 */
int handle_accept(int epoll_fd, int listen_fd);