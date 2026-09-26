import socket
import time
import unittest

from wren import Client, WrenTimeoutError, WrenTransportError

class TestTimeout(unittest.TestCase):
    def setUp(self):
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.bind(("127.0.0.1", 0))
        self.listener.listen(1)
        self.port = self.listener.getsockname()[1]

    def tearDown(self):
        self.listener.close()

    def test_timeout_on_silent_server(self):
        c = Client("127.0.0.1", self.port, timeout=0.2)
        try:
            start = time.monotonic()
            with self.assertRaises(WrenTimeoutError):
                c.call(method_id=1)
            self.assertLess(time.monotonic() - start, 1.0)
        finally:
            c.close()

    def test_connection_after_timeout(self):
        c = Client("127.0.0.1", self.port, timeout=0.2)
        try:
            with self.assertRaises(WrenTimeoutError):
                c.call(method_id=1)
            with self.assertRaises(WrenTransportError) as cm:
                c.call(method_id=1)
            self.assertNotIsInstance(cm.exception, WrenTimeoutError)
        finally:
            c.close()

    if __name__ == "__main__":
        unittest.main()
 