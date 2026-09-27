/*
 * Example calc server. Handlers are written against the typed signatures
 * wrengen generates from calc.wren (build/gen/calc.h)
 *
 * Usage: calc_server [port]    (default 8080)
 */
#include "calc.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CALC_ERR_DIV_ZERO 1   /* service-defined error code (PROTOCOL.md) */

struct calc_state {
    unsigned long long calls;
};

static struct calc_state *state(wren_call_t *call) {
    struct calc_state *s = Calc_ctx(call);
    s->calls++;
    return s;
}

static int calc_add(wren_call_t *call, uint32_t a, uint32_t b, uint32_t *result) {
    state(call);
    *result = a + b;
    return 0;
}

static int calc_multiply(wren_call_t *call, uint32_t a, uint32_t b, uint32_t *result) {
    state(call);
    *result = a * b;
    return 0;
}

static int calc_distance(wren_call_t *call, const Point *p1, const Point *p2, double *result) {
    state(call);
    *result = hypot(p2->x - p1->x, p2->y - p1->y);
    return 0;
}

static int calc_sum(wren_call_t *call, const wren_list_u32 *values, uint64_t *result) {
    state(call);
    uint64_t total = 0;
    for (uint32_t i = 0; i < values->len; i++)
        total += values->items[i];
    *result = total;
    return 0;
}

static int calc_divmod(wren_call_t *call, uint32_t a, uint32_t b,
                       uint32_t *quotient, uint32_t *remainder) {
    state(call);
    if (b == 0) {
        wren_call_reply_error(call, CALC_ERR_DIV_ZERO, "division by zero");
        return -1;
    }
    *quotient = a / b;
    *remainder = a % b;
    return 0;
}

static int calc_list_admins(wren_call_t *call, wren_list_SRow *result) {
    static Row admins[] = {
        {1, {"alice", 5}, {"admin", 5}},
        {4, {"diana", 5}, {"admin", 5}},
    };
    state(call);
    result->len = sizeof(admins) / sizeof(admins[0]);
    result->items = admins;
    return 0;
}

static int calc_log(wren_call_t *call, wren_bytes_t msg) {
    struct calc_state *s = state(call);
    /* wren strings are not null-terminated: print with an explicit length. */
    printf("[call %llu] %.*s\n", s->calls, (int)msg.len, msg.data);
    return 0;
}

static int parse_port(const char *text, uint16_t *out) {
    char *end;
    long v = strtol(text, &end, 10);
    if (*text == '\0' || *end != '\0' || v < 1 || v > 65535)
        return -1;
    *out = (uint16_t)v;
    return 0;
}

int main(int argc, char **argv) {
    uint16_t port = 8080;
    if (argc > 1 && parse_port(argv[1], &port) < 0) {
        fprintf(stderr, "usage: calc_server [port]\n");
        return 2;
    }

    static struct calc_state st;
    static Calc_handlers handlers = {
        .add = calc_add,
        .multiply = calc_multiply,
        .distance_between = calc_distance,
        .sum_array = calc_sum,
        .divmod = calc_divmod,
        .list_admins = calc_list_admins,
        .log_message = calc_log,
    };

    wren_server_t *s = wren_server_create(port);
    if (!s) {
        fprintf(stderr, "calc_server: cannot listen on port %u\n", port);
        return 1;
    }
    if (Calc_register(s, &handlers, &st) < 0) {
        fprintf(stderr, "calc_server: registration failed\n");
        wren_server_destroy(s);
        return 1;
    }
    int rc = wren_server_run(s);
    wren_server_destroy(s);
    return rc;
}