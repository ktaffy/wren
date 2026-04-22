#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * Socket option for create_listener(). Fields map directly to setsockopt(2).
 * Only int-valued options are supported.
 */
struct sock_opt {
    int level;
    int name;
    int value;
};

/**
 * Create a non-blocking IPv4 TCP listener on @p port (host byte order),
 * bound to INADDR_ANY. Applies @p opts in order before bind(); pass NULL/0
 * for none.
 *
 * @return listening fd on success, -1 on failure with errno set.
 *         Caller must close(). The fd is closed on internal failure.
 */
int create_sock(uint16_t port, int backlog, const struct sock_opt *opts, size_t n_opts);