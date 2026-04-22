#include "internal/epoll_util.h"

#include <stdio.h>
#include <sys/epoll.h>

int epoll_add(int epfd, int fd, uint32_t events, void *ptr) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.ptr = ptr;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        perror("epoll_ctl: ADD");
        return -1;
    }
    return 0;
}
int epoll_mod(int epfd, int fd, uint32_t events, void *ptr) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.ptr = ptr;
    if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
        perror("epoll_ctl: MOD");
        return -1;
    }
    return 0;
}
int epoll_del(int epfd, int fd) {
    if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL) < 0) {
        perror("epoll_ctl: DEL");
        return -1;
    }
    return 0;
}