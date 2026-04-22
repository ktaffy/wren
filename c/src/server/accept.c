#include "internal/accept.h"
#include "internal/conn.h"
#include "internal/epoll_util.h"
#include "internal/server_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <errno.h>

int handle_accept(struct wren_server *s) {
    int listen_fd = srv_listen_fd(s);
    int epoll_fd = srv_epoll_fd(s);

    for (;;) {
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        int conn_fd = accept4(listen_fd, (struct sockaddr *)&addr, &addr_len, SOCK_NONBLOCK);
        if (conn_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
            if (errno == EINTR) continue;
            if (errno == ECONNABORTED || errno == EPROTO || errno == EPERM) {
                perror("accept: per-connection");
                continue;
            }
            perror("accept: fatal");
            return -1;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
        printf("connect: fd=%d %s:%u\n", conn_fd, ip, ntohs(addr.sin_port));

        struct conn *c = calloc(1, sizeof(*c));
        if (!c) {
            perror("calloc: conn");
            close(conn_fd);
            continue;
        }

        c->in_buf = malloc(CONN_IN_BUF_INITIAL);
        if (!c->in_buf) {
            perror("malloc: in_buf");
            free(c);
            close(conn_fd);
            continue;
        }
        c->in_cap = CONN_IN_BUF_INITIAL;
        c->in_head = 0;
        c->in_tail = 0;

        c->fd = conn_fd;
        c->epoll_fd = epoll_fd;
        c->server = s;

        if (epoll_add(epoll_fd, conn_fd, EPOLLIN | EPOLLET | EPOLLRDHUP, c) < 0) {
            perror("accept: epoll_add");
            free(c->in_buf);
            free(c);
            close(conn_fd);
            continue;
        }
    }
}