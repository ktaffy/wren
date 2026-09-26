import pathlib
import shutil
import subprocess
import tempfile
import unittest

from tests.test_emit import load as load_py
from wrengen.emit_c import emit_c
from wrengen.parser import parse

REPO = pathlib.Path(__file__).resolve().parents[2]
C_INCLUDE = REPO / "c" / "include"
RUNTIME = [REPO / "c" / "src" / "codec.c", REPO / "c" / "src" / "proto.c"]
CC = shutil.which("gcc") or shutil.which("cc")

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
    header, source = emit_c(parse(schema_src), "gen")
    with tempfile.TemporaryDirectory() as tmp:
        d = pathlib.Path(tmp)
        (d / "gen.h").write_text(header)
        (d / "gen.c").write_text(source)
        (d / "main.c").write_text(DRIVER_PRELUDE + main_body)
        exe = d / "main"
        build = subprocess.run(
            [CC, "-std=c11", "-D_GNU_SOURCE", "-Wall", "-Wextra", "-Werror",
             "-I", str(C_INCLUDE), "-I", str(d), "-o", str(exe),
             str(d / "main.c"), str(d / "gen.c"), *map(str, RUNTIME)],
            capture_output=True, text=True)
        if build.returncode != 0:
            raise AssertionError(f"C build failed:\n{build.stderr}")
        run = subprocess.run([str(exe)], capture_output=True, text=True, check=True)
    results = {}
    for line in run.stdout.splitlines():
        name, hexs, done, same = line.split(":")
        results[name] = (hexs, done == "1", same == "1")
    return results

@unittest.skipUnless(CC, "no C compiler available")
class TestEmitCStructs(unittest.TestCase):
    def test_calc_structs_match_python(self):
        src = (pathlib.Path(__file__).parent / "fixtures" / "calc.wren").read_text()
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