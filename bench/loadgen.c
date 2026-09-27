/*
 * wren load generator. Opens N connections, each a closed loop, and reports throughput and latency.
 *
 * Usage: loadgen [-H host] [-p port] [-c conns] [-d secs] [-w warmup]
 *                [-m noop|add|echo] [-s echo_bytes]
 */
#include "wren/proto.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define METHOD_ADD  1
#define METHOD_ECHO 2
#define METHOD_NOOP 3

struct worker {
    pthread_t thread;
    int fd;
    uint64_t *lat;
    size_t n, cap;
    int failed;
};

static const char *method_name = "noop";
static uint32_t echo_size = 64;
static uint16_t method_id;
static char *request;
static size_t request_len;
static size_t reply_payload_len;
static uint64_t measure_start_ns, end_ns;
static pthread_barrier_t start_barrier;

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

static int send_all(int fd, const char *buf, size_t len) {
    while (len) {
        ssize_t n = send(fd, buf, len, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        buf += n;
        len -= (size_t)n;
    }
    return 0;
}

static int recv_all(int fd, char *buf, size_t len) {
    while (len) {
        ssize_t n = recv(fd, buf, len, 0);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return -1;
        buf += n;
        len -= (size_t)n;
    }
    return 0;
}

static int connect_to(const char *host, uint16_t port) {
    struct addrinfo hints = {0}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char portstr[8];
    snprintf(portstr, sizeof portstr, "%u", port);
    int rc = getaddrinfo(host, portstr, &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "loadgen: %s: %s\n", host, gai_strerror(rc));
        return -1;
    }
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0 || connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("loadgen: connect");
        if (fd >= 0)
            close(fd);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    return fd;
}

static int record(struct worker *w, uint64_t ns) {
    if (w->n == w->cap) {
        size_t cap = w->cap ? w->cap * 2 : 4096;
        uint64_t *lat = realloc(w->lat, cap * sizeof *lat);
        if (!lat)
            return -1;
        w->lat = lat;
        w->cap = cap;
    }
    w->lat[w->n++] = ns;
    return 0;
}

static void *run_worker(void *arg) {
    struct worker *w = arg;
    char *req = malloc(request_len);
    char *resp = malloc(MSG_HEADER_SIZE + reply_payload_len);
    if (!req || !resp)
        w->failed = 1;
    else
        memcpy(req, request, request_len);

    pthread_barrier_wait(&start_barrier);

    uint32_t req_id = 0;
    while (!w->failed) {
        uint64_t t0 = now_ns();
        if (t0 >= end_ns)
            break;

        uint32_t id_n = htonl(++req_id);
        memcpy(req + 8, &id_n, 4);
        struct msg_header h;
        if (send_all(w->fd, req, request_len) < 0 ||
            recv_all(w->fd, resp, MSG_HEADER_SIZE) < 0) {
            w->failed = 1;
            break;
        }
        proto_dec_header(resp, &h);
        if (h.type != MSG_TYPE_RESPONSE || h.req_id != req_id ||
            h.length != MSG_HEADER_SIZE + reply_payload_len ||
            recv_all(w->fd, resp + MSG_HEADER_SIZE, reply_payload_len) < 0) {
            w->failed = 1;
            break;
        }
        uint64_t t1 = now_ns();

        uint32_t v;
        if (method_id == METHOD_ADD &&
            (proto_dec_u32(resp + MSG_HEADER_SIZE, 4, &v) != 4 || v != 7))
            w->failed = 1;
        if (method_id == METHOD_ECHO &&
            (proto_dec_u32(resp + MSG_HEADER_SIZE, 4, &v) != 4 || v != echo_size))
            w->failed = 1;

        if (!w->failed && t0 >= measure_start_ns && record(w, t1 - t0) < 0)
            w->failed = 1;
    }
    free(req);
    free(resp);
    return NULL;
}

static int cmp_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

static double pct_us(const uint64_t *v, size_t n, size_t per_mille) {
    size_t rank = (n * per_mille + 999) / 1000;
    if (rank == 0)
        rank = 1;
    return (double)v[rank - 1] / 1000.0;
}

static void usage(void) {
    fprintf(stderr, "usage: loadgen [-H host] [-p port] [-c conns] [-d secs] "
                    "[-w warmup] [-m noop|add|echo] [-s echo_bytes]\n");
    exit(2);
}

int main(int argc, char **argv) {
    const char *host = "127.0.0.1";
    long port = 8080, conns = 1;
    double secs = 10.0, warmup = 2.0;

    int opt;
    while ((opt = getopt(argc, argv, "H:p:c:d:w:m:s:")) != -1) {
        switch (opt) {
        case 'H': host = optarg; break;
        case 'p': port = strtol(optarg, NULL, 10); break;
        case 'c': conns = strtol(optarg, NULL, 10); break;
        case 'd': secs = strtod(optarg, NULL); break;
        case 'w': warmup = strtod(optarg, NULL); break;
        case 'm': method_name = optarg; break;
        case 's': echo_size = (uint32_t)strtoul(optarg, NULL, 10); break;
        default: usage();
        }
    }
    if (port < 1 || port > 65535 || conns < 1 || conns > 10000 ||
        secs <= 0 || warmup < 0)
        usage();

    size_t payload_len;
    char *payload;
    if (strcmp(method_name, "noop") == 0) {
        method_id = METHOD_NOOP;
        payload_len = 0;
        reply_payload_len = 0;
        payload = NULL;
    } else if (strcmp(method_name, "add") == 0) {
        method_id = METHOD_ADD;
        payload_len = 8;
        reply_payload_len = 4;
        payload = malloc(payload_len);
        proto_enc_u32(payload, 3);
        proto_enc_u32(payload + 4, 4);
    } else if (strcmp(method_name, "echo") == 0) {
        method_id = METHOD_ECHO;
        payload_len = 4 + (size_t)echo_size;
        reply_payload_len = payload_len;
        payload = malloc(payload_len);
        proto_enc_u32(payload, echo_size);
        memset(payload + 4, 'x', echo_size);
    } else {
        usage();
    }
    if (!strcmp(method_name, "noop") == 0 && !payload) {
        perror("loadgen: malloc");
        return 1;
    }

    request_len = MSG_HEADER_SIZE + payload_len;
    request = malloc(request_len);
    if (!request) {
        perror("loadgen: malloc");
        return 1;
    }
    proto_enc_u32(request, (uint32_t)request_len);
    proto_enc_u8(request + 4, MSG_TYPE_CALL);
    proto_enc_u8(request + 5, 0);
    proto_enc_u16(request + 6, method_id);
    proto_enc_u32(request + 8, 0);
    if (payload_len)
        memcpy(request + MSG_HEADER_SIZE, payload, payload_len);
    free(payload);

    struct worker *workers = calloc((size_t)conns, sizeof *workers);
    if (!workers) {
        perror("loadgen: calloc");
        return 1;
    }
    for (long i = 0; i < conns; i++) {
        workers[i].fd = connect_to(host, (uint16_t)port);
        if (workers[i].fd < 0)
            return 1;
    }

    pthread_barrier_init(&start_barrier, NULL, (unsigned)conns + 1);
    for (long i = 0; i < conns; i++)
        pthread_create(&workers[i].thread, NULL, run_worker, &workers[i]);
    uint64_t t = now_ns();
    measure_start_ns = t + (uint64_t)(warmup * 1e9);
    end_ns = measure_start_ns + (uint64_t)(secs * 1e9);
    pthread_barrier_wait(&start_barrier);

    size_t total = 0;
    int failed = 0;
    for (long i = 0; i < conns; i++) {
        pthread_join(workers[i].thread, NULL);
        failed |= workers[i].failed;
        total += workers[i].n;
        close(workers[i].fd);
    }
    if (failed) {
        fprintf(stderr, "loadgen: a connection failed or got a wrong reply\n");
        return 1;
    }
    if (total == 0) {
        fprintf(stderr, "loadgen: no calls completed in the measurement window\n");
        return 1;
    }

    uint64_t *all = malloc(total * sizeof *all);
    if (!all) {
        perror("loadgen: malloc");
        return 1;
    }
    size_t off = 0;
    for (long i = 0; i < conns; i++) {
        memcpy(all + off, workers[i].lat, workers[i].n * sizeof *all);
        off += workers[i].n;
        free(workers[i].lat);
    }
    qsort(all, total, sizeof *all, cmp_u64);

    printf("%s,%u,%ld,%zu,%.3f,%.0f,%.1f,%.1f,%.1f\n",
           method_name, method_id == METHOD_ECHO ? echo_size : 0, conns,
           total, secs, (double)total / secs,
           pct_us(all, total, 500), pct_us(all, total, 990),
           (double)all[total - 1] / 1000.0);
    free(all);
    free(workers);
    free(request);
    return 0;
}