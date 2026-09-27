## Setup

- CPU: Intel(R) Core(TM) Ultra 9 275HX
- Cores: 24, server pinned: taskset -c 0
- Kernel: 6.6.87.2-microsoft-standard-WSL2
- Environment: WSL2 (virtualized and windows host shares the CPU)
- Compiler: cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
- Each result: median of 3 runs, 10s measured after 2s warm-up, TCP loopback

| Method |   Payload | Conns | Throughput (req/s) | p50 (µs) | p99 (µs) |
| ------ | --------: | ----: | -----------------: | -------: | -------: |
| noop   |         - |     1 |             34,811 |     24.8 |     57.4 |
| noop   |         - |    10 |            217,962 |     44.7 |     65.4 |
| noop   |         - |    50 |            213,200 |    228.2 |    324.5 |
| noop   |         - |   100 |            209,001 |    462.2 |    650.7 |
| add    |         - |     1 |             37,150 |     24.7 |     56.7 |
| add    |         - |    10 |            209,577 |     46.2 |     73.7 |
| add    |         - |    50 |            205,678 |    232.4 |    340.4 |
| add    |         - |   100 |            204,841 |    470.1 |    662.4 |
| echo   |      64 B |    10 |            209,178 |     46.2 |     74.8 |
| echo   |    4096 B |    10 |            194,123 |     50.2 |     78.3 |
| echo   |   65536 B |    10 |             79,213 |    119.8 |    207.3 |
| echo   | 1048576 B |    10 |              4,964 |   1972.1 |   2610.7 |
