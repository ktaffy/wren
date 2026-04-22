# wren TODO

## Done
- [x] Protocol spec v0.2 locked (see PROTOCOL.md)
- [x] C server implementation compliant with v0.2 (within 4 KiB buffer limit)
- [x] Python client library v0.1 (proves protocol is language-agnostic)
- [x] Cross-language interop verified via 16-test battery covering primitives,
      arrays, strings, errors, composites, buffer limits, and concurrency

## C implementation gaps vs spec v0.2
- [ ] Dynamic in_buf growth for larger messages (currently capped at 4 KiB;
      spec recommends 64 MiB). Oversized messages are currently rejected
      cleanly with "msg too large" close, which is correct but limiting.
- [ ] Send ERROR responses on dispatch failure (currently only logs stderr;
      client just times out or hangs)
- [ ] Consider closing connection on protocol violations (method_id 0,
      invalid length, etc) instead of leniently logging
- [ ] True server-side concurrency: current event loop blocks on long-running
      handlers (handler_concurrent_stress serializes calls). Consider worker
      thread pool or async handlers.

## Python implementation gaps
- [ ] Server side (currently client only)
- [ ] Typed encode/decode helpers so users don't hand-roll struct.pack calls
- [ ] recv() timeout (client hangs forever if server is unresponsive)
- [ ] Multiplexing (currently one call in flight per client)

## Framework pieces not yet built
- [ ] Schema language design — syntax, types, grammar  (← Option C, next)
- [ ] Schema parser — reads .wren files into an IR
- [ ] Codegen tool (wrengen) — emits per-language client/server stubs
- [ ] C client half — connect, multiplex in-flight calls by req_id, read
      responses (Option D)

## Performance work (after correctness is locked)
- [ ] TCP_NODELAY on all sockets
- [ ] Combine header + payload writes (writev or single buffer)
- [ ] Arena allocator for per-request memory (avoid malloc on hot path)
- [ ] Evaluate io_uring vs epoll
- [ ] Benchmarks — establish baseline loopback latency and throughput

## Stretch / future protocol versions
- [ ] Streaming responses — new message types, chunked frames (v0.3)
- [ ] Per-message compression flag
- [ ] Optional auth handshake