# wren TODO

## Done
- [x] Protocol spec v0.2 locked (see PROTOCOL.md)
- [x] C server implementation compliant with v0.2 (within 4 KiB buffer limit)
- [x] Python client library v0.1 (proves protocol is language-agnostic)
- [x] Cross-language interop verified via 16-test battery covering primitives,
      arrays, strings, errors, composites, buffer limits, and concurrency
- [x] Refactor stage 1: wren_server_t opaque type, public API, event loop
      hidden from user code (examples/main.c reduced to ~20 lines)
- [x] Refactor stage 2: wren_call_t abstraction with wren_call_read_*/
      wren_call_reply_* helpers; handlers no longer touch struct conn or
      req_id directly (handle_add shrank from 15 lines to 5)
- [x] Refactor stage 3: naming consistency — proto_dec_header,
      net_create_sock, etc.; prefix rules followed throughout
- [x] 4a: Send ERROR responses on dispatch failure

## Refactor stages remaining
- [ ] Stage 4: correctness gaps
      - [ ] 4b: Close connection on protocol violations (method_id 0,
            invalid length) instead of leniently logging
      - [ ] 4c: Dynamic in_buf growth for larger messages (currently 4 KiB,
            spec recommends 64 MiB)
      - [~] Deferred: true server-side concurrency (worker pool / async) —
            architectural, deserves its own dedicated refactor
- [ ] Stage 5: code quality pass — dead code, comments, consistent style
- [ ] Stage 6: performance pass — TCP_NODELAY, writev, consider arena

## Python implementation gaps
- [ ] Server side (currently client only)
- [ ] Typed encode/decode helpers so users don't hand-roll struct.pack calls
- [ ] recv() timeout (client hangs forever if server is unresponsive)
- [ ] Multiplexing (currently one call in flight per client)

## Framework pieces not yet built
- [ ] Schema language design — syntax, types, grammar  (← after refactor)
- [ ] Schema parser — reads .wren files into an IR
- [ ] Codegen tool (wrengen) — emits per-language client/server stubs
- [ ] C client half — connect, multiplex in-flight calls by req_id, read
      responses

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