import struct
import unittest
import threading
import time

from wren import Client, WrenCallError, WrenTransportError

HOST = "localhost"
PORT = 8080
METHOD_ADD = 1

def _parse_rows(resp):
    offset = 0
    (row_count,) = struct.unpack("!I", resp[offset:offset+4])
    offset += 4
    rows = []
    for _ in range(row_count):
        (field_count,) = struct.unpack("!I", resp[offset:offset+4])
        offset += 4
        fields = []
        for _ in range(field_count):
            (flen,) = struct.unpack("!I", resp[offset:offset+4])
            offset += 4
            fields.append(resp[offset:offset+flen].decode())
            offset += flen
        rows.append(fields)
    return rows

class TestClient(unittest.TestCase):

    def test_echo_bytes(self):
        c = Client(HOST, PORT)
        try:
            data = b"hello, wren!"
            payload = struct.pack("!I", len(data)) + data
            resp = c.call(method_id=2, payload=payload)
            (n,) = struct.unpack("!I", resp[:4])
            self.assertEqual(resp[4:4+n], data)
        finally:
            c.close()

    def test_noop(self):
        c = Client(HOST, PORT)
        try:
            resp = c.call(method_id=3, payload=b"")
            self.assertEqual(resp, b"")
        finally:
            c.close()

    def test_pi(self):
        c = Client(HOST, PORT)
        try:
            resp = c.call(method_id=4, payload=b"")
            (val,) = struct.unpack("!d", resp)
            self.assertAlmostEqual(val, 3.141592653589793)
        finally:
            c.close()

    def test_mul_i64_negative(self):
        c = Client(HOST, PORT)
        try:
            # -7 * 6 = -42
            payload = struct.pack("!qq", -7, 6)
            resp = c.call(method_id=5, payload=payload)
            (result,) = struct.unpack("!q", resp)
            self.assertEqual(result, -42)
        finally:
            c.close()

    def test_make_vec3(self):
        c = Client(HOST, PORT)
        try:
            payload = struct.pack("!fff", 1.5, 2.5, 3.5)
            resp = c.call(method_id=6, payload=payload)
            x, y, z = struct.unpack("!fff", resp)
            self.assertAlmostEqual(x, 3.0)
            self.assertAlmostEqual(y, 5.0)
            self.assertAlmostEqual(z, 7.0)
        finally:
            c.close()

    def test_sum_array(self):
        c = Client(HOST, PORT)
        try:
            values = [1, 2, 3, 4, 5, 100, 1000]
            payload = struct.pack("!I", len(values)) + struct.pack(f"!{len(values)}I", *values)
            resp = c.call(method_id=7, payload=payload)
            (total,) = struct.unpack("!Q", resp)
            self.assertEqual(total, sum(values))
        finally:
            c.close()

    def test_divide_success(self):
        c = Client(HOST, PORT)
        try:
            payload = struct.pack("!II", 20, 4)
            resp = c.call(method_id=8, payload=payload)
            (result,) = struct.unpack("!I", resp)
            self.assertEqual(result, 5)
        finally:
            c.close()

    def test_divide_by_zero_returns_error(self):
        c = Client(HOST, PORT)
        try:
            payload = struct.pack("!II", 10, 0)
            with self.assertRaises(WrenCallError) as cm:
                c.call(method_id=8, payload=payload)
            self.assertEqual(cm.exception.code, 42)
            self.assertIn("division by zero", cm.exception.message)
        finally:
            c.close()

    def test_concat(self):
        c = Client(HOST, PORT)
        try:
            s1 = "hello, ".encode()
            s2 = "world".encode()
            payload = (struct.pack("!I", len(s1)) + s1 +
                    struct.pack("!I", len(s2)) + s2)
            resp = c.call(method_id=9, payload=payload)
            (n,) = struct.unpack("!I", resp[:4])
            self.assertEqual(resp[4:4+n].decode(), "hello, world")
        finally:
            c.close()

    def test_is_even(self):
        c = Client(HOST, PORT)
        try:
            for v, expected in [(0, 1), (1, 0), (2, 1), (99, 0), (100, 1)]:
                payload = struct.pack("!I", v)
                resp = c.call(method_id=10, payload=payload)
                self.assertEqual(resp[0], expected)
        finally:
            c.close()

    def test_matmul_4x4(self):
        """Multiply a matrix by the identity — should get the same matrix back."""
        c = Client(HOST, PORT)
        try:
            # Input A: arbitrary values. B: identity.
            A = [1.0, 2.0, 3.0, 4.0,
                5.0, 6.0, 7.0, 8.0,
                9.0, 10.0, 11.0, 12.0,
                13.0, 14.0, 15.0, 16.0]
            I = [1.0, 0.0, 0.0, 0.0,
                0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                0.0, 0.0, 0.0, 1.0]
            payload = struct.pack("!32f", *A, *I)
            resp = c.call(method_id=11, payload=payload)
            result = struct.unpack("!16f", resp)
            for got, want in zip(result, A):
                self.assertAlmostEqual(got, want, places=5)
        finally:
            c.close()

    def test_parse_csv(self):
        """Parse a small CSV and verify the row/field structure round-trips."""
        c = Client(HOST, PORT)
        try:
            text = "a,b,c\n1,2,3\nfoo,bar,baz"
            text_b = text.encode()
            payload = struct.pack("!I", len(text_b)) + text_b
            resp = c.call(method_id=12, payload=payload)

            # Parse response
            offset = 0
            (row_count,) = struct.unpack("!I", resp[offset:offset+4])
            offset += 4
            rows = []
            for _ in range(row_count):
                (field_count,) = struct.unpack("!I", resp[offset:offset+4])
                offset += 4
                fields = []
                for _ in range(field_count):
                    (flen,) = struct.unpack("!I", resp[offset:offset+4])
                    offset += 4
                    fields.append(resp[offset:offset+flen].decode())
                    offset += flen
                rows.append(fields)

            self.assertEqual(rows, [
                ["a", "b", "c"],
                ["1", "2", "3"],
                ["foo", "bar", "baz"],
            ])
        finally:
            c.close()

    def test_concurrent_stress(self):
        """Launch several concurrent clients, each making a slow call.
        All calls should complete correctly (though they will serialize
        on this single-threaded server)."""
        results = []
        errors = []

        def worker(sleep_ms):
            try:
                c = Client(HOST, PORT)
                payload = struct.pack("!I", sleep_ms)
                resp = c.call(method_id=13, payload=payload)
                (got,) = struct.unpack("!Q", resp)
                results.append((sleep_ms, got))
                c.close()
            except Exception as e:
                errors.append(e)

        threads = []
        for ms in [50, 50, 50, 50, 50]:
            t = threading.Thread(target=worker, args=(ms,))
            threads.append(t)
            t.start()

        for t in threads:
            t.join(timeout=5.0)

        self.assertEqual(errors, [])
        self.assertEqual(len(results), 5)
        for sent, got in results:
            self.assertEqual(sent, got)

    def test_query_db_all(self):
        """Query without 'admin' keyword returns all 5 rows."""
        c = Client(HOST, PORT)
        try:
            sql = b"SELECT * FROM users"
            payload = struct.pack("!I", len(sql)) + sql + struct.pack("!I", 0)
            resp = c.call(method_id=14, payload=payload)
            rows = _parse_rows(resp)
            self.assertEqual(len(rows), 5)
            # First row should be ["1", "alice", "admin"]
            self.assertEqual(rows[0], ["1", "alice", "admin"])
        finally:
            c.close()

    def test_query_db_admin_only(self):
        """Query with 'admin' keyword returns only admin rows."""
        c = Client(HOST, PORT)
        try:
            sql = b"SELECT * FROM users WHERE role='admin'"
            payload = struct.pack("!I", len(sql)) + sql + struct.pack("!I", 0)
            resp = c.call(method_id=14, payload=payload)
            rows = _parse_rows(resp)
            # alice and diana are admins
            self.assertEqual(len(rows), 2)
            self.assertTrue(all(row[2] == "admin" for row in rows))
        finally:
            c.close()

    def test_echo_large_fails(self):
        """Sending a 10KB payload to a server with 4 KiB buffer must fail,
        and it must fail cleanly (no hang, no crash)."""
        c = Client(HOST, PORT)
        try:
            data = b"x" * 10_000
            payload = struct.pack("!I", len(data)) + data
            with self.assertRaises(WrenTransportError):
                c.call(method_id=15, payload=payload)
        finally:
            c.close()

if __name__ == "__main__":
    unittest.main()