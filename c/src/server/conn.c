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

void handle_conn_event(struct conn *c, uint32_t events) {
    const char *close_rsn = NULL;

    if (events & (EPOLLERR | EPOLLHUP)) close_rsn = "err/hup";

    if (!close_rsn && (events & EPOLLIN)) {
        for (;;) {
            size_t space = sizeof(c->in_buf) - c->in_len;
            if (space == 0) {
                perror("inbuf full");
                close_rsn = "buf full";
                break;
            }
            ssize_t bytes_read = read(c->fd, c->in_buf + c->in_len, space);
            if (bytes_read > 0) {
                c->in_len += bytes_read;
            } else if (bytes_read == 0) {
                close_rsn = "peer closed";
                break;
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                if (errno == EINTR) continue;
                perror("conn: read");
                close_rsn = "read error";
                break;
            }

            while(c->in_len >= MSG_HEADER_SIZE) {
                struct msg_header hdr;
                proto_dec_header(c->in_buf, &hdr);
                if (hdr.length < MSG_HEADER_SIZE) {
                    close_rsn = "invalid length";
                    break;
                }
                if (hdr.length > sizeof(c->in_buf)) {
                    close_rsn = "msg too large";
                    break;
                }
                if (c->in_len < hdr.length) break;
                if (hdr.type == MSG_TYPE_CALL) {
                    const char *payload = c->in_buf + MSG_HEADER_SIZE;
                    size_t payload_len = hdr.length - MSG_HEADER_SIZE;
                    srv_dispatch(c->server, c, &hdr, payload, payload_len);
                }
                size_t remaining = c->in_len - hdr.length;
                // TODO(Stage 2): memmove per message is O(n) per frame. A ring buffer
                //             would make this amortized O(1) but adds complexity.
                memmove(c->in_buf, c->in_buf + hdr.length, remaining);
                c->in_len = remaining;
            }
            if (close_rsn) break;
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