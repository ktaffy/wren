import struct

from .client import WrenError

class WrenDecodeError(WrenError):
    """Payload bytes did not match the expected layout"""

class Writer:
    def __init__(self):
        self._buf = bytearray()

    def put(self, fmt, value):
        try:
            self._buf += struct.pack("!" + fmt, value)
        except struct.error as e:
            raise ValueError(f"cannot encode {value!r} as '{fmt}': {e}") from e

    def put_bytes(self, data):
        self.put("I", len(data))
        self._buf += data

    def put_string(self, text):
        self.put_bytes(text.encode("utf-8"))

    def getvalue(self):
        return bytes(self._buf)

class Reader:
    def __init__(self, data):
        self._data = data
        self._pos = 0

    def _need(self, n):
        left = len(self._data) - self._pos
        if left < n:
            raise WrenDecodeError(
                f"need {n} bytes at offset {self._pos}, only {left} left")

    def get(self, fmt):
        fmt = "!" + fmt
        size = struct.calcsize(fmt)
        self._need(size)
        (value,) = struct.unpack_from(fmt, self._data, self._pos)
        self._pos += size
        return value

    def get_bytes(self):
        n = self.get("I")
        self._need(n)
        value = bytes(self._data[self._pos:self._pos + n])
        self._pos += n
        return value

    def get_string(self):
        raw = self.get_bytes()
        try:
            return raw.decode("utf-8")
        except UnicodeDecodeError as e:
            raise WrenDecodeError(f"invalid UTF-8 in string: {e}") from e

    def expect_end(self):
        left = len(self._data) - self._pos
        if left:
            raise WrenDecodeError(f"{left} unexpected trailing bytes")