#pragma once

#include <stdint.h>

/**
 * Register @p fd with the epoll instance @p epoll_fd for the given
 * @p events. Stores ptr to conn struct in ev.data.ptr so epoll_wait returns it directly.
 *
 * @return 0 on success, -1 on failure (errno set by epoll_ctl, message
 *         printed via perror).
 */
int epoll_add(int epfd, int fd, uint32_t events, void *ptr);

/**
 * Modify the event mask for an fd already registered with @p epoll_fd.
 * Used to add/remove interest (e.g. adding EPOLLOUT when writes block).
 *
 * @return 0 on success, -1 on failure. Fails with ENOENT if @p fd was
 *         never added.
 */
int epoll_mod(int epfd, int fd, uint32_t events, void *ptr);

/**
 * Remove @p fd from @p epoll_fd's interest list. Safe to call before
 * close(); close() also removes the fd implicitly if it's the last
 * reference, but explicit removal is defensive against dup'd fds.
 *
 * @return 0 on success, -1 on failure.
 */
int epoll_del(int epfd, int fd);