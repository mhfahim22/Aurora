# Aurora Benchmarks

## HTTP Server Benchmark (Phase 38.2)

Aurora's embedded HTTP server is benchmarked against Node.js and Go to verify the
`>= 50% of Go performance` target.

### How to Run

```powershell
# Requires hey (or wrk). Auto-detected; falls back to PowerShell loop.
scripts/bench_http.ps1 -Duration 10 -Connections 50
```

### Method

1. Compile `benchmarks/bench_http.aura` to a native executable (AOT via
   `aurorac --emit-obj` + MinGW link against `libaurora_runtime`).
2. Start the Aurora server on `:8080` and warm up.
3. Run an N-second load test against `/hello` (100 concurrent keep-alive
   connections) with `hey`.
4. Run the identical load against the Node.js (`:8081`) and Go (`:8082`)
   comparison servers.
5. Record `Requests/sec` and p99 latency from each.

### Results

_Machine: Windows, MinGW g++ 15.2.0, Go 1.26, Node current. Runtime: 10 s,
100 concurrent keep-alive connections, single JSON endpoint._

_Last run: `scripts/bench_http.ps1`, Phase 38.2._

| Runtime | Requests/sec | Avg latency | p99 latency |
|---------|-------------|-------------|-------------|
| Aurora  | ~1,180      | ~85 ms      | ~183 ms     |
| Node.js | ~4,250      | ~24 ms      | ~42 ms      |
| Go      | ~8,340      | ~12 ms      | ~40 ms      |

### Analysis

- **Aurora ~1,180–1,500 req/s ≈ 14–17% of Go (~8,300–8,950 req/s), below the 50% target.**
- Explained by the **thread-per-connection** accept model in
  `aurora_server_accept_loop` (`server.cpp`). Single-connection latency is
  ~1.8 ms/request (competitive), but under 100 concurrent keep-alive
  connections Windows thread creation + per-connection OS overhead adds
  ~60 ms of latency, collapsing throughput.
- Node.js and Go use event loops / goroutines respectively and scale almost
  linearly with concurrency; Aurora's 1-thread-per-connection model does not.
- A worker-pool attempt (shared `AuroraWorkerPool`) performed *worse* (65 req/s)
  because blocking keep-alive `recv` holds a worker for the whole connection —
  so it was reverted in favor of the thread-per-connection default.

### Recommendation

To meet the `>= 50% of Go` target, replace per-connection threads with a
proper **event-driven / epoll(IO completion port on Windows)** accept loop with
a fixed worker pool that reads and dispatches non-blocking sockets, rather than
blocking `recv` inside one thread per connection. This is the documented gap for
future optimization; the current figures are published here verbatim.

### Target

- Aurora must reach **≥ 50% of Go** requests/sec on the same hardware.
  - **Status: not yet met (17%).**
- Aurora JSON endpoint must be **≥ 2× faster than an interpreted runtime**.
  - Node.js is JIT-compiled; Aurora trails on this workload. The single-connection
    latency (~1.8 ms) is competitive, but concurrency scaling is the blocker.

## Aurora HTTP Benchmark Results

Run: 2026-08-07 22:37
Load: 10s, 100 connections, path /hello

### Raw Output
```
Aurora:

Summary:
  Total:	10.0671 secs
  Slowest:	0.4316 secs
  Fastest:	0.0011 secs
  Average:	0.0845 secs
  Requests/sec:	1179.6798
  
  Total data:	391908 bytes
  Size/request:	33 bytes

Response time histogram:
  0.001 [1]	|
  0.044 [687]	|ΓûáΓûáΓûáΓûá
  0.087 [6570]	|ΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûá
  0.130 [3915]	|ΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûá
  0.173 [477]	|ΓûáΓûáΓûá
  0.216 [171]	|Γûá
  0.259 [22]	|
  0.302 [6]	|
  0.346 [23]	|
  0.389 [2]	|
  0.432 [2]	|


Latency distribution:
  10%% in 0.0703 secs
  25%% in 0.0755 secs
  50%% in 0.0835 secs
  75%% in 0.0907 secs
  90%% in 0.0985 secs
  95%% in 0.1413 secs
  99%% in 0.1833 secs

Details (average, fastest, slowest):
  DNS+dialup:	0.0002 secs, 0.0000 secs, 0.0257 secs
  DNS-lookup:	0.0004 secs, 0.0000 secs, 0.0229 secs
  req write:	0.0000 secs, 0.0000 secs, 0.0092 secs
  resp wait:	0.0835 secs, 0.0009 secs, 0.4312 secs
  resp read:	0.0001 secs, 0.0000 secs, 0.0062 secs

Status code distribution:
  [200]	11876 responses




--- Node.js (:8081) ---
 Summary:   Total:	10.0205 secs   Slowest:	0.1298 secs   Fastest:	0.0013 secs   Average:	0.0235 secs   Requests/sec:	4251.6893      Total data:	724268 bytes   Size/request:	17 bytes  Response time histogram:   0.001 [1]	|   0.014 [36]	|   0.027 [36970]	|ΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûá   0.040 [5077]	|ΓûáΓûáΓûáΓûáΓûá   0.053 [445]	|   0.066 [27]	|   0.078 [13]	|   0.091 [4]	|   0.104 [28]	|   0.117 [0]	|   0.130 [3]	|   Latency distribution:   10%% in 0.0196 secs   25%% in 0.0210 secs   50%% in 0.0225 secs   75%% in 0.0247 secs   90%% in 0.0282 secs   95%% in 0.0314 secs   99%% in 0.0424 secs  Details (average, fastest, slowest):   DNS+dialup:	0.0000 secs, 0.0000 secs, 0.0273 secs   DNS-lookup:	0.0000 secs, 0.0000 secs, 0.0258 secs   req write:	0.0000 secs, 0.0000 secs, 0.0077 secs   resp wait:	0.0234 secs, 0.0013 secs, 0.1023 secs   resp read:	0.0000 secs, 0.0000 secs, 0.0074 secs  Status code distribution:   [200]	42604 responses   
--- Go (:8082) ---
 Summary:   Total:	10.0076 secs   Slowest:	0.1600 secs   Fastest:	0.0006 secs   Average:	0.0120 secs   Requests/sec:	8339.6506      Total data:	2837640 bytes   Size/request:	34 bytes  Response time histogram:   0.001 [1]	|   0.017 [66313]	|ΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûá   0.032 [15157]	|ΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûáΓûá   0.048 [1576]	|Γûá   0.064 [271]	|   0.080 [61]	|   0.096 [52]	|   0.112 [28]	|   0.128 [0]	|   0.144 [0]	|   0.160 [1]	|   Latency distribution:   10%% in 0.0038 secs   25%% in 0.0067 secs   50%% in 0.0104 secs   75%% in 0.0153 secs   90%% in 0.0214 secs   95%% in 0.0264 secs   99%% in 0.0404 secs  Details (average, fastest, slowest):   DNS+dialup:	0.0000 secs, 0.0000 secs, 0.0221 secs   DNS-lookup:	0.0000 secs, 0.0000 secs, 0.0202 secs   req write:	0.0000 secs, 0.0000 secs, 0.0677 secs   resp wait:	0.0107 secs, 0.0005 secs, 0.1021 secs   resp read:	0.0009 secs, 0.0000 secs, 0.0823 secs  Status code distribution:   [200]	83460 responses   
```

### Summary
- Aurora HTTP server compiled from Aurora source (benchmarks/bench_http.aura)
- Target: >= 50% of Go performance (currently ~17%)
- Re-run anytime with: scripts/bench_http.ps1
