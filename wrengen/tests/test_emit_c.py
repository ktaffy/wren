import pathlib
import shutil
import socket
import subprocess
import tempfile
import time
import unittest

from tests.test_emit import load as load_py
from wren import Client, WrenCallError
from wrengen.emit_c import emit_c
from wrengen.parser import parse

REPO = pathlib.Path(__file__).resolve().parents[2]
C_INCLUDE = REPO / "c" / "include"
LIB_SOURCES = (sorted((REPO / "c" / "src").glob("*.c"))
               + sorted((REPO / "c" / "src" / "server").glob("*.c")))
FIXTURE = pathlib.Path(__file__).parent / "fixtures" / "calc.wren"
CC = shutil.which("gcc") or shutil.which("cc")

def _cc(args):
    proc = subprocess.run([CC, *map(str, args)], capture_output=True, text=True)
    if proc.returncode != 0:
        raise AssertionError(f"C build failed:\n{proc.stderr}")

def build(d, schema_src, main_src):
    header, source = emit_c(parse(schema_src), "gen")
    (d / "gen.h").write_text(header)
    (d / "gen.c").write_text(source)
    (d / "main.c").write_text(main_src)
    _cc(["-std=c11", "-D_GNU_SOURCE", "-Wall", "-Wextra", "-Werror",
         "-I", C_INCLUDE, "-c", d / "gen.c", "-o", d / "gen.o"])
    exe = d / "main"
    _cc(["-D_GNU_SOURCE", "-Wall", "-I", C_INCLUDE, "-I", d, "-o", exe,
         d / "main.c", d / "gen.o", *LIB_SOURCES, "-lm"])
    return exe


DRIVER_PRELUDE = r"""
#include <stdio.h>
#include <string.h>
#include "gen.h"

/* Print "Name:hex:done:same" for one encode/decode/re-encode cycle. */
static void report(const char *name, const wren_writer_t *w1,
                   const wren_writer_t *w2, const wren_reader_t *r) {
    printf("%s:", name);
    for (size_t i = 0; i < w1->len; i++)
        printf("%02x", (unsigned char)w1->buf[i]);
    int same = w1->ok && w2->ok && w1->len == w2->len &&
               (w1->len == 0 || memcmp(w1->buf, w2->buf, w1->len) == 0);
    printf(":%d:%d\n", wren_reader_done(r), same);
}

#define ROUND_TRIP(T, value)                                              \
    do {                                                                  \
        wren_writer_t w1, w2;                                             \
        wren_writer_init(&w1);                                            \
        wren_writer_init(&w2);                                            \
        T##_encode(&w1, &(value));                                        \
        wren_reader_t r;                                                  \
        wren_reader_init(&r, w1.buf, w1.len);                             \
        wren_arena_t a = {0};                                             \
        T out;                                                            \
        T##_decode(&r, &a, &out);                                         \
        T##_encode(&w2, &out);                                            \
        report(#T, &w1, &w2, &r);                                         \
        wren_arena_free(&a);                                              \
        wren_writer_free(&w1);                                            \
        wren_writer_free(&w2);                                            \
    } while (0)
"""

def run_c(schema_src, main_body):
    with tempfile.TemporaryDirectory() as tmp:
        exe = build(pathlib.Path(tmp), schema_src, DRIVER_PRELUDE + main_body)
        run = subprocess.run([str(exe)], capture_output=True, text=True, check=True)
    results = {}
    for line in run.stdout.splitlines():
        name, hexs, done, same = line.split(":")
        results[name] = (hexs, done == "1", same == "1")
    return results

@unittest.skipUnless(CC, "no C compiler available")
class TestEmitCStructs(unittest.TestCase):
    def test_calc_structs_match_python(self):
        src = FIXTURE.read_text()
        py = load_py(src)
        got = run_c(src, r"""
            int main(void) {
                Point p = {1.5, 2.5};
                Row row = {1, {"alice", 5}, {"admin", 5}};
                ROUND_TRIP(Point, p);
                ROUND_TRIP(Row, row);
                return 0;
            }
        """)
        self.assertEqual(got["Point"], (py["Point"](1.5, 2.5).encode().hex(), True, True))
        self.assertEqual(got["Row"],
                         (py["Row"](1, "alice", "admin").encode().hex(), True, True))

    def test_nested_arrays_match_python(self):
        src = """
            struct Point { f64 x; f64 y; }
            struct Poly {
                Point[] vertices;
                f32[4] color;
                u32[][] grid;
                string[] tags;
                bytes blob;
                bool closed;
                u8[2][] pairs;
            }
        """
        py = load_py(src)
        expected = py["Poly"](
            [py["Point"](0.0, 0.0), py["Point"](1.5, 2.0)],
            [1.0, 0.5, 0.25, 0.0],
            [[1, 2], [], [3]],
            ["a", "é"],
            b"\x00\xff",
            True,
            [[1, 2], [3, 4]],
        ).encode().hex()
        got = run_c(src, r"""
            int main(void) {
                Point verts[] = {{0.0, 0.0}, {1.5, 2.0}};
                uint32_t g0[] = {1, 2}, g2[] = {3};
                wren_list_u32 rows[] = {{2, g0}, {0, NULL}, {1, g2}};
                wren_bytes_t tags[] = {{"a", 1}, {"\xc3\xa9", 2}};
                uint8_t pairs[][2] = {{1, 2}, {3, 4}};
                Poly poly = {
                    .vertices = {2, verts},
                    .color = {1.0f, 0.5f, 0.25f, 0.0f},
                    .grid = {3, rows},
                    .tags = {2, tags},
                    .blob = {"\x00\xff", 2},
                    .closed = true,
                    .pairs = {2, pairs},
                };
                ROUND_TRIP(Poly, poly);
                return 0;
            }
        """)
        self.assertEqual(got["Poly"], (expected, True, True))

    def test_signed_empty_and_fixed_of_lists_match_python(self):
        src = """
            struct Empty {}
            struct Misc { i8 a; i64 b; Empty e; u16[][2] halves; }
        """
        py = load_py(src)
        got = run_c(src, r"""
            int main(void) {
                uint16_t h0[] = {7};
                Empty e = {0};
                Misc m = {.a = -1, .b = INT64_MIN, .e = {0},
                          .halves = {{1, h0}, {0, NULL}}};
                ROUND_TRIP(Empty, e);
                ROUND_TRIP(Misc, m);
                return 0;
            }
        """)
        self.assertEqual(got["Empty"], ("", True, True))
        self.assertEqual(got["Misc"], (
            py["Misc"](-1, -2**63, py["Empty"](), [[7], []]).encode().hex(),
            True, True))


CALC_SERVER_MAIN = r"""
#include <math.h>
#include <stdlib.h>
#include "gen.h"

static int impl_add(wren_call_t *call, uint32_t a, uint32_t b, uint32_t *result) {
    (void)call;
    *result = a + b;
    return 0;
}

static int impl_multiply(wren_call_t *call, uint32_t a, uint32_t b, uint32_t *result) {
    /* ctx points at 1: a missing or wrong ctx crashes or changes the result. */
    *result = a * b * *(uint32_t *)Calc_ctx(call);
    return 0;
}

static int impl_distance(wren_call_t *call, const Point *p1, const Point *p2, double *result) {
    (void)call;
    *result = hypot(p2->x - p1->x, p2->y - p1->y);
    return 0;
}

static int impl_sum(wren_call_t *call, const wren_list_u32 *values, uint64_t *result) {
    (void)call;
    uint64_t total = 0;
    for (uint32_t i = 0; i < values->len; i++)
        total += values->items[i];
    *result = total;
    return 0;
}

static int impl_divmod(wren_call_t *call, uint32_t a, uint32_t b,
                       uint32_t *quotient, uint32_t *remainder) {
    if (b == 0) {
        wren_call_reply_error(call, 42, "division by zero");
        return -1;
    }
    *quotient = a / b;
    *remainder = a % b;
    return 0;
}

static int impl_list_admins(wren_call_t *call, wren_list_SRow *result) {
    static Row rows[] = {{1, {"alice", 5}, {"admin", 5}},
                         {4, {"diana", 5}, {"admin", 5}}};
    (void)call;
    result->len = 2;
    result->items = rows;
    return 0;
}

static int impl_log(wren_call_t *call, wren_bytes_t msg) {
    (void)call;
    (void)msg;
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    static uint32_t factor = 1;
    static Calc_handlers h = {
        .add = impl_add,
        .multiply = impl_multiply,
        .distance_between = impl_distance,
        .sum_array = impl_sum,
        .divmod = impl_divmod,
        .list_admins = impl_list_admins,
        .log_message = impl_log,
    };
    wren_server_t *s = wren_server_create((uint16_t)atoi(argv[1]));
    if (!s || Calc_register(s, &h, &factor) < 0)
        return 1;
    return wren_server_run(s);
}
"""

def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]

@unittest.skipUnless(CC, "no C compiler available")
class TestEmitCServer(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = FIXTURE.read_text()
        cls.py = load_py(src)
        cls.tmp = tempfile.TemporaryDirectory()
        exe = build(pathlib.Path(cls.tmp.name), src, CALC_SERVER_MAIN)
        cls.port = free_port()
        cls.proc = subprocess.Popen([str(exe), str(cls.port)],
                                    stdout=subprocess.DEVNULL,
                                    stderr=subprocess.DEVNULL)
        try:
            deadline = time.monotonic() + 5.0
            while True:
                if cls.proc.poll() is not None:
                    raise AssertionError("generated server exited on startup")
                try:
                    socket.create_connection(("127.0.0.1", cls.port), timeout=0.2).close()
                    break
                except OSError:
                    if time.monotonic() > deadline:
                        raise AssertionError("generated server never started listening")
                    time.sleep(0.05)
        except BaseException:
            cls.tearDownClass()
            raise

    @classmethod
    def tearDownClass(cls):
        cls.proc.terminate()
        cls.proc.wait(timeout=5)
        cls.tmp.cleanup()

    def setUp(self):
        self.calc = self.py["Calc"](Client("127.0.0.1", self.port, timeout=2.0))
        self.addCleanup(self.calc.close)

    def test_add(self):
        self.assertEqual(self.calc.add(3, 4), 7)

    def test_multiply_reads_handler_ctx(self):
        self.assertEqual(self.calc.multiply(6, 7), 42)

    def test_struct_arguments(self):
        P = self.py["Point"]
        self.assertEqual(self.calc.distance_between(P(0.0, 0.0), P(3.0, 4.0)), 5.0)

    def test_list_argument(self):
        self.assertEqual(self.calc.sum_array([1, 2, 3, 100]), 106)

    def test_tuple_return(self):
        self.assertEqual(self.calc.divmod(17, 5), (3, 2))

    def test_handler_error(self):
        with self.assertRaises(WrenCallError) as cm:
            self.calc.divmod(1, 0)
        self.assertEqual((cm.exception.code, cm.exception.message),
                         (42, "division by zero"))

    def test_list_of_structs_return(self):
        Row = self.py["Row"]
        self.assertEqual(self.calc.list_admins(),
                         [Row(1, "alice", "admin"), Row(4, "diana", "admin")])

    def test_void_method(self):
        self.assertIsNone(self.calc.log_message("hi"))

    def test_bad_arguments_rejected_before_handler(self):
        raw = Client("127.0.0.1", self.port, timeout=2.0)
        self.addCleanup(raw.close)
        with self.assertRaises(WrenCallError) as cm:
            raw.call(1, b"\x00")
        self.assertEqual(cm.exception.code, 1003)