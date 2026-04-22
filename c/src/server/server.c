#include "internal/server_internal.h"
#include "internal/net.h"
#include "internal/epoll_util.h"
#include "internal/accept.h"
#include "internal/conn.h"
#include "wren/server.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#define BACKLOG 128
#define MAX_EVENTS 64

struct wren_server {
    int listen_fd;
    int epoll_fd;
    uint16_t port;
    handler_fn handlers[MAX_METHODS];
};

wren_server_t *wren_server_create(uint16_t port) {
    wren_server_t *s = calloc(1, sizeof(*s));
    if (!s)
        return NULL;

    s->port = port;
    s->listen_fd = -1;
    s->epoll_fd = -1;

    struct sock_opt opts[] = {{SOL_SOCKET, SO_REUSEADDR, 1}};
    s->listen_fd = net_create_sock(port, BACKLOG, opts, 1);
    if (s->listen_fd < 0)
        goto fail;

    s->epoll_fd = epoll_create1(0);
    if (s->epoll_fd < 0) {
        perror("server: epoll_create1");
        goto fail;
    }

    if (epoll_add(s->epoll_fd, s->listen_fd, EPOLLIN, NULL) < 0) {
        goto fail;
    }

    return s;

fail:
    wren_server_destroy(s);
    return NULL;
}

int wren_server_register(wren_server_t *s, uint16_t method_id, handler_fn fn) {
    return srv_register(s, method_id, fn);
}

int wren_server_run(wren_server_t *s) {
    if (!s)
        return -1;
    printf("listening on port: %u\n", s->port);

    struct epoll_event events[MAX_EVENTS];
    for (;;) {
        int n = epoll_wait(s->epoll_fd, events, MAX_EVENTS, -1);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("server: epoll_wait");
            return -1;
        }
        for (int i = 0; i < n; i++) {
            if (events[i].data.ptr == NULL) {
                if (handle_accept(s) < 0)
                    return -1;
            }
            else {
                handle_conn_event(events[i].data.ptr, events[i].events);
            }
        }
    }
    return 0;
}

void wren_server_destroy(wren_server_t *s) {
    if (!s)
        return;
    if (s->epoll_fd >= 0)
        close(s->epoll_fd);
    if (s->listen_fd >= 0)
        close(s->listen_fd);
    free(s);
}

handler_fn srv_handler_get(struct wren_server *s, uint16_t method_id)
{
    if (!s || method_id >= MAX_METHODS)
        return NULL;
    return s->handlers[method_id];
}

void srv_handler_set(struct wren_server *s, uint16_t method_id, handler_fn fn) {
    s->handlers[method_id] = fn;
}

int srv_listen_fd(struct wren_server *s) {
    return s ? s->listen_fd : -1;
}

int srv_epoll_fd(struct wren_server *s) {
    return s ? s->epoll_fd : -1;
}