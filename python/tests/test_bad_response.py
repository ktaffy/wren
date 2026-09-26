import socket
import struct
import threading
import unittest

from wren import Client, WrenTransportError

HEADER_FORMAT = "!IBBHI"
HEADER_SIZE = 12
MSG_TYPE_RESPONSE = 2
MSG_TYPE_ERROR = 3


def _recv_exact(sock, n):
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("peer closed")
        buf += chunk
    return buf


class ScriptedServer:
    def __init__(self, replies):
        self._replies = replies
        self._listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._listener.bind(("127.0.0.1", 0))
        self._listener.listen(1)
        self.port = self._listener.getsockname()[1]
        self._thread = threading.Thread(target=self._serve, daemon=True)
        self._thread.start()

    def _serve(self):
        try:
            conn, _ = self._listener.accept()
            with conn:
                for msg_type, payload in self._replies:
                    req = _recv_exact(conn, HEADER_SIZE)
                    length, _type, _flags, _method, req_id = struct.unpack(
                        HEADER_FORMAT, req
                    )
                    _recv_exact(conn, length - HEADER_SIZE)
                    header = struct.pack(
                        HEADER_FORMAT,
                        HEADER_SIZE + len(payload),
                        msg_type,
                        0,
                        0,
                        req_id,
                    )
                    conn.sendall(header + payload)
        except OSError:
            pass

    def close(self):
        self._listener.close()
        self._thread.join(timeout=1.0)


class TestMalformedResponse(unittest.TestCase):

    def _connect(self, replies):
        server = ScriptedServer(replies)
        self.addCleanup(server.close)
        client = Client("127.0.0.1", server.port, timeout=1.0)
        self.addCleanup(client.close)
        return client

    def test_short_error_payload_raises_transport_error(self):
        c = self._connect([(MSG_TYPE_ERROR, struct.pack("!I", 42))])
        with self.assertRaises(WrenTransportError):
            c.call(method_id=1)

    def test_connection_usable_after_short_error_payload(self):
        c = self._connect([
            (MSG_TYPE_ERROR, struct.pack("!I", 42)),
            (MSG_TYPE_RESPONSE, struct.pack("!I", 7)),
        ])
        with self.assertRaises(WrenTransportError):
            c.call(method_id=1)
        resp = c.call(method_id=1)
        self.assertEqual(struct.unpack("!I", resp)[0], 7)


if __name__ == "__main__":
    unittest.main()