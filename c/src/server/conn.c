#include "internal/conn.h"
#include "internal/epoll_util.h"
#include "internal/server_internal.h"
#include "wren/proto.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/uio.h>

void handle_conn_event(struct conn *c, uint32_t events) {
    const char *close_rsn = NULL;

    if (events & (EPOLLERR | EPOLLHUP))
        close_rsn = "err/hup";

    if (!close_rsn && (events & EPOLLIN)) {
        for (;;) {
            size_t tail_space = c->in_cap - c->in_tail;

            if (tail_space == 0) {
                size_t unread = c->in_tail - c->in_head;
                if (c->in_head > 0) {
                    memmove(c->in_buf, c->in_buf + c->in_head, unread);
                    c->in_tail = unread;
                    c->in_head = 0;
                    tail_space = c->in_cap - c->in_tail;
                }
                if (tail_space == 0) {
                    if (c->in_cap >= CONN_IN_BUF_MAX) {
                        close_rsn = "buf full";
                        break;
                    }
                    size_t new_cap = c->in_cap * 2;
                    if (new_cap > CONN_IN_BUF_MAX)
                        new_cap = CONN_IN_BUF_MAX;
                    char *new_buf = realloc(c->in_buf, new_cap);
                    if (!new_buf) {
                        close_rsn = "oom growing in_buf";
                        break;
                    }
                    c->in_buf = new_buf;
                    c->in_cap = new_cap;
                    tail_space = c->in_cap - c->in_tail;
                }
            }

            ssize_t bytes_read = read(c->fd, c->in_buf + c->in_tail, tail_space);
            if (bytes_read > 0) {
                c->in_tail += bytes_read;
            }
            else if (bytes_read == 0) {
                close_rsn = "peer closed";
                break;
            }
            else {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                if (errno == EINTR)
                    continue;
                perror("conn: read");
                close_rsn = "read error";
                break;
            }

            while (c->in_tail - c->in_head >= MSG_HEADER_SIZE) {
                struct msg_header hdr;
                proto_dec_header(c->in_buf + c->in_head, &hdr);

                if (hdr.length < MSG_HEADER_SIZE) {
                    close_rsn = "invalid length";
                    break;
                }
                if (hdr.length > CONN_IN_BUF_MAX) {
                    close_rsn = "msg too large";
                    break;
                }
                if (c->in_tail - c->in_head < hdr.length) {
                    if (hdr.length > c->in_cap) {
                        if (c->in_head > 0) {
                            memmove(c->in_buf, c->in_buf + c->in_head,
                                    c->in_tail - c->in_head);
                            c->in_tail -= c->in_head;
                            c->in_head = 0;
                        }
                        if (hdr.length > c->in_cap) {
                            char *new_buf = realloc(c->in_buf, hdr.length);
                            if (!new_buf) {
                                close_rsn = "oom growing in_buf";
                                break;
                            }
                            c->in_buf = new_buf;
                            c->in_cap = hdr.length;
                        }
                    }
                    break;
                }
                if (hdr.type != MSG_TYPE_CALL) {
                    close_rsn = "unexpected message type";
                    break;
                }

                const char *payload = c->in_buf + c->in_head + MSG_HEADER_SIZE;
                size_t payload_len = hdr.length - MSG_HEADER_SIZE;
                srv_dispatch(c->server, c, &hdr, payload, payload_len);

                c->in_head += hdr.length;
            }
            if (close_rsn)
                break;
        }
    }
    if (!close_rsn && (events & EPOLLOUT)) {
        while (c->out_sent < c->out_len) {
            ssize_t n = send(c->fd,
                             c->out_buf + c->out_sent,
                             c->out_len - c->out_sent,
                             MSG_NOSIGNAL);
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                if (errno == EINTR)
                    continue;
                perror("conn: send (flush)");
                close_rsn = "send error";
                break;
            }
            c->out_sent += n;
        }
        if (!close_rsn && c->out_sent == c->out_len) {
            free(c->out_buf);
            c->out_buf = NULL;
            c->out_len = 0;
            c->out_sent = 0;
            epoll_mod(c->epoll_fd, c->fd, EPOLLIN | EPOLLET | EPOLLRDHUP, c);
        }
    }
    if (!close_rsn && (events & EPOLLRDHUP)) {
        close_rsn = "peer shutdown";
    }
    if (close_rsn) {
        printf("disconnect: fd=%d (%s)\n", c->fd, close_rsn);
        free(c->in_buf);
        free(c->out_buf);
        close(c->fd);
        free(c);
    }
}

int conn_write(struct conn *c, const char *data, size_t len) {
    if (c->out_len > c->out_sent) {
        char *grown = realloc(c->out_buf, c->out_len + len);
        if (!grown) return -1;
        memcpy(grown + c->out_len, data, len);
        c->out_buf = grown;
        c->out_len += len;
        return 0;
    }
    ssize_t n = send(c->fd, data, len, MSG_NOSIGNAL);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) n = 0;
        else return -1;
    }
    if ((size_t)n == len) return 0;
    if ((size_t)n < len) {
        size_t remaining = len - n;
        c->out_buf = malloc(remaining);
        if (!c->out_buf) return -1;
        memcpy(c->out_buf, data + n, remaining);
        c->out_len = remaining;
        c->out_sent = 0;
        return epoll_mod(c->epoll_fd, c->fd, EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP, c);
    }
    return 0;
}

int conn_writev(struct conn *c, const struct iovec *iov, int iovcnt) {
    size_t total = 0;
    for (int i = 0; i < iovcnt; i++) total += iov[i].iov_len;
    if (total == 0) return 0;

    if (c->out_len > c->out_sent) {
        char *grown = realloc(c->out_buf, c->out_len + total);
        if (!grown) return -1;
        size_t off = c->out_len;
        for (int i = 0; i < iovcnt; i++) {
            memcpy(grown + off, iov[i].iov_base, iov[i].iov_len);
            off += iov[i].iov_len;
        }
        c->out_buf = grown;
        c->out_len += total;
        return 0;
    }
    ssize_t n;
    for (;;) {
        n = writev(c->fd, iov, iovcnt);
        if (n >= 0) break;
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) { n = 0; break; }
        return -1;
    }
    if ((size_t)n == total) return 0;

    size_t sent = (size_t)n;
    size_t remaining = total - sent;

    c->out_buf = malloc(remaining);
    if (!c->out_buf) return -1;

    size_t skip = sent;
    size_t off = 0;
    for (int i = 0; i < iovcnt; i++) {
        if (skip >= iov[i].iov_len) {
            skip -= iov[i].iov_len;
            continue;
        }
        size_t start = skip;
        size_t len = iov[i].iov_len - start;
        memcpy(c->out_buf + off, (const char *)iov[i].iov_base + start, len);
        off += len;
        skip = 0;
    }
    c->out_len = remaining;
    c->out_sent = 0;
    return epoll_mod(c->epoll_fd, c->fd, EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP, c);
}