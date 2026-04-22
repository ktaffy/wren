"""wren RPC client."""

import socket
import struct

HEADER_SIZE = 12
HEADER_FORMAT = "!IBBHI"

MSG_TYPE_CALL = 1
MSG_TYPE_RESPONSE = 2
MSG_TYPE_ERROR = 3

class WrenError(Exception):
    """Base class for wren errors."""

class WrenTransportError(WrenError):
    """Network or protocol-level failure."""

class WrenCallError(WrenError):
    def __init__(self, code, message):
        self.code = code
        self.message = message
        super().__init__(f"wren call failed (code={code}): {message}")

class Client:
    def __init__(self, host, port):
        self._sock = socket.create_connection((host, port))
        self._next_req_id = 1

    def call(self, method_id, payload=b""):
        if method_id == 0:
            raise ValueError("method_id 0 is reserved")
        req_id = self._next_req_id
        self._next_req_id += 1

        # Build request/msg
        total_len = HEADER_SIZE + len(payload)
        header = struct.pack(
            HEADER_FORMAT,
            total_len,
            MSG_TYPE_CALL,
            0, #flags
            method_id,
            req_id,
        )
        self._send_all(header + payload)

        # read resp
        resp_header = self._recv_exact(HEADER_SIZE)
        length, msg_type, _flags, _method, resp_req_id = struct.unpack(
            HEADER_FORMAT, resp_header
        )
        if length < HEADER_SIZE:
            raise WrenTransportError(f"invalid length in respons: {length}")
        if resp_req_id != req_id:
            raise WrenTransportError(
                f"req_id mismatch: sent {req_id}, got {resp_req_id}"
            )
        resp_payload = self._recv_exact(length - HEADER_SIZE)
        if msg_type == MSG_TYPE_RESPONSE:
            return resp_payload
        elif msg_type == MSG_TYPE_ERROR:
            code, message = self._parse_error(resp_payload)
            raise WrenCallError(code, message)
        else:
            raise WrenTransportError(f"unexpected message type: {msg_type}")
    
    def close(self):
        self._sock.close()
    
    def _send_all(self, data):
        try:
            self._sock.sendall(data)
        except OSError as e:
            raise WrenTransportError(f"send failed: {e}") from e
        
    def _recv_exact(self, n):
        buf = bytearray()
        while len(buf) < n:
            try:
                chunk = self._sock.recv(n - len(buf))
            except OSError as e:
                raise WrenTransportError(f"recv failed: {e}") from e
            if not chunk:
                raise WrenTransportError("connection closed mid-message")
            buf.extend(chunk)
        return bytes(buf)
    
    def _parse_error(self, payload):
        if len(payload) < 0:
            raise WrenTransportError("error payload too short")
        code, msg_len = struct.unpack("!II", payload[:8])
        if len(payload) < 8 + msg_len:
            raise WrenTransportError("error message truncated")
        message = payload[8:8 + msg_len].decode("utf-8", errors="replace")
        return code, message