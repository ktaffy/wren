# wren TODO

## Done
- [x] Protocol spec v0.2 locked (see PROTOCOL.md)
- [x] C server implementation compliant with v0.2 (up to 64 MiB per message)
- [x] Python client library v0.1 (proves protocol is language-agnostic)
- [x] Cross-language interop verified via 19-test battery covering primitives,
      arrays, strings, errors, composites, buffer limits, and concurrency
- [x] Refactor stage 1: wren_server_t opaque type, public API, event loop
      hidden from user code (examples/main.c reduced to ~20 lines)
- [x] Refactor stage 2: wren_call_t abstraction with wren_call_read_*/
      wren_call_reply_* helpers; handlers no longer touch struct conn or
      req_id directly (handle_add shrank from 15 lines to 5)
- [x] Refactor stage 3: naming consistency — proto_dec_header,
      net_create_sock, etc.; prefix rules followed throughout
- [x] Refactor stage 4: correctness gaps
      - [x] 4a: Send ERROR responses on dispatch failure
      - [x] 4b: Close connection on protocol violations (unexpected message type)
      - [x] 4c: Dynamic in_buf growth via lazy-compaction ring buffer (to 64 MiB)
- [x] Refactor stage 5: code quality pass — dead TODOs removed, stale
      comments updated, MAX_METHODS raised to full uint16 range
- [x] Refactor stage 6: performance pass — TCP_NODELAY on every connection,
      writev for combined header+payload sends (one syscall instead of two)

## Framework pieces not yet built
- [ ] Schema language design — syntax, types, grammar  (← next)
- [ ] Schema parser — reads .wren files into an IR
- [ ] Codegen tool (wrengen) — emits per-language client/server stubs
- [ ] C client half — connect, multiplex in-flight calls by req_id, read
      responses

## Python implementation gaps
- [ ] Server side (currently client only)
- [ ] Typed encode/decode helpers so users don't hand-roll struct.pack calls
- [ ] recv() timeout (client hangs forever if server is unresponsive)
- [ ] Multiplexing (currently one call in flight per client)

## Deferred / future work
- [ ] True server-side concurrency: current event loop blocks on long-running
      handlers (handle_concurrent_stress serializes calls). Worker thread
      pool or async handlers — architectural change, deserves its own refactor.
- [ ] Arena allocator for per-request memory (avoid malloc on hot path;
      revisit after benchmarks show allocator overhead is a bottleneck)
- [ ] Evaluate io_uring vs epoll
- [ ] Benchmarks — establish baseline loopback latency and throughput

## Stretch / future protocol versions
- [ ] Streaming responses — new message types, chunked frames (v0.3)
- [ ] Per-message compression flag
- [ ] Optional auth handshake