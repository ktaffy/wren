#include "internal/net.h"

#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int net_create_sock(uint16_t port, int backlog, const struct sock_opt *opts, size_t n_opts) {
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (fd < 0) {
        perror("net: socket");
        return -1;
    }

    for (size_t i = 0; i < n_opts; i++) {
        if (setsockopt(fd, opts[i].level, opts[i].name, &opts[i].value, sizeof(opts[i].value)) < 0) {
            perror("net: setsockopt");
            close(fd);
            return -1;
        }
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("net: bind");
        close(fd);
        return -1;
    }
    if (listen(fd, backlog) < 0) {
        perror("net: listen");
        close(fd);
        return -1;
    }
    return fd;
}