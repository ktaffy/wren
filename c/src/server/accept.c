#include "internal/accept.h"
#include "internal/epoll_util.h"
#include "internal/conn.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <errno.h>

int handle_accept(int epoll_fd, int listen_fd) {
    for (;;) {
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        int conn_fd = accept4(listen_fd, (struct sockaddr *) &addr, &addr_len, SOCK_NONBLOCK);
        if (conn_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return 0; // queue drained
            if (errno == EINTR)
                continue; // retry
            if (errno == ECONNABORTED || errno == EPROTO || errno == EPERM) {
                perror("accept: per-connection"); // skip client
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
        c->fd = conn_fd;
        c->epoll_fd = epoll_fd;
        if (epoll_add(epoll_fd, conn_fd, EPOLLIN | EPOLLET | EPOLLRDHUP, c) < 0) {
            perror("accept: epoll_add");
            close(conn_fd);
            free(c);
            continue;
        }
    }
}