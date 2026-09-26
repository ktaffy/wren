import unittest

from wren import WrenError
from wren.codec import Reader, Writer, WrenDecodeError

ROUND_TRIP = [
    ("?", True), ("?", False),
    ("B", 255), ("b", -128),
    ("H", 65535), ("h", -32768),
    ("I", 2**32 - 1), ("i", -2**31),
    ("Q", 2**64 - 1), ("q", -2**63),
    ("f", 1.5), ("d", 3.141592653589793),
]

class TestWriter(unittest.TestCase):
    def test_put_uses_network_byte_order(self):
        w = Writer()
        w.put("I", 7)
        self.assertEqual(w.getvalue(), b"\x00\x00\x00\x07")

    def test_string_length_counts_bytes_not_chars(self):
        w = Writer()
        w.put_string("hé")
        self.assertEqual(w.getvalue(), b"\x00\x00\x00\x03h\xc3\xa9")

    def test_out_of_range_raises_value_error(self):
        with self.assertRaises(ValueError):
            Writer().put("B", 256)

class TestReader(unittest.TestCase):
    def test_round_trip_every_fixed_primitive(self):
        for fmt, value in ROUND_TRIP:
            with self.subTest(fmt=fmt, value=value):
                w = Writer()
                w.put(fmt, value)
                r = Reader(w.getvalue())
                self.assertEqual(r.get(fmt), value)
                r.expect_end()

    def test_sequential_reads_advance_cursor(self):
        w = Writer()
        w.put("I", 3)
        w.put_string("ab")
        w.put("d", 1.0)
        r = Reader(w.getvalue())
        self.assertEqual(r.get("I"), 3)
        self.assertEqual(r.get_string(), "ab")
        self.assertEqual(r.get("d"), 1.0)
        r.expect_end()

    def test_short_buffer_raises(self):
        with self.assertRaises(WrenDecodeError):
            Reader(b"\x00\x00").get("I")

    def test_bytes_length_beyond_buffer_raises(self):
        # Claims 5 bytes, only 2 follow.
        with self.assertRaises(WrenDecodeError):
            Reader(b"\x00\x00\x00\x05ab").get_bytes()

    def test_trailing_bytes_rejected(self):
        r = Reader(b"\x00\x00\x00\x01\xff")
        r.get("I")
        with self.assertRaises(WrenDecodeError):
            r.expect_end()

    def test_invalid_utf8_rejected(self):
        with self.assertRaises(WrenDecodeError):
            Reader(b"\x00\x00\x00\x01\xff").get_string()

    def test_decode_error_is_a_wren_error(self):
        self.assertTrue(issubclass(WrenDecodeError, WrenError))

if __name__ == "__main__":
    unittest.main()