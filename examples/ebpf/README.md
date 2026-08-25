# Production Linux eBPF & Telemetry Streaming Recipes

This directory contains production-ready eBPF tracing recipes engineered for zero-friction integration with `libhisto`.

---

## Overview

`libhisto` features native stream auto-detection and parsing for `bpftrace` and BCC histogram outputs (both log2 `@ = hist(...)` and linear `@ = lhist(...)`), as well as real-time shared memory ring buffer ingestion (>50M events/sec).

```
   ┌──────────────────┐
   │ Linux Kernel     │
   │ eBPF / Tracepoint│
   └────────┬─────────┘
            │
            ▼
   ┌──────────────────┐
   │ bpftrace Script  │
   └────────┬─────────┘
            │ (pipe stdout or SHM ring buffer)
            ▼
   ┌─────────────────────────────────────────────────────────────┐
   │ libhisto CLI Ecosystem                                      │
   │                                                             │
   │  histo plot  ── Terminal Unicode/ASCII visualization        │
   │  histo stats ── Statistical moments, percentiles & errors   │
   │  histo fit   ── Gaussian, LogNormal, Exponential fit        │
   │  histo cmp   ── Wasserstein distance & divergence metrics   │
   │  histo fill  ── Serialize to portable JSON / Binary v2      │
   │  histo top   ── Real-time interactive TUI monitor           │
   └─────────────────────────────────────────────────────────────┘
```

---

## Production Recipes

| Recipe Script | Subsystem Traced | Output Format | Recommended libhisto Pipeline |
|---|---|---|---|
| [`vfs_read_latency.bt`](vfs_read_latency.bt) | Filesystem VFS read latency | Log2 `hist()` table | `sudo bpftrace -q vfs_read_latency.bt \| histo plot --scale-input 1e-3` |
| [`block_io_heatmap.bt`](block_io_heatmap.bt) | Block device I/O size vs latency | Streaming pairs & histograms | `sudo bpftrace -q block_io_heatmap.bt \| histo top --2d` |
| [`tcp_rtt.bt`](tcp_rtt.bt) | TCP smoothed RTT distribution | Log2 `hist()` table | `sudo bpftrace -q tcp_rtt.bt \| histo stats` |
| [`runqlat.bt`](runqlat.bt) | CPU scheduler runqueue wait time | Log2 `hist()` table | `sudo bpftrace -q runqlat.bt \| histo plot --log` |
| [`page_faults.bt`](page_faults.bt) | Kernel memory page fault latency | Log2 `hist()` table | `sudo bpftrace -q page_faults.bt \| histo fit --model=exponential` |

---

## Usage Examples

### 1. Visualizing Filesystem Read Latency in Microseconds

`bpftrace` records raw nanoseconds by default. Use `--scale-input` to convert units on the fly:

```bash
sudo bpftrace -q vfs_read_latency.bt | histo plot --scale-input 1e-3
```

### 2. Computing Statistical Summary & High-Nines Percentiles

Pipe directly into `histo stats` for detailed quantile calculations (P50, P90, P95, P99, P99.9), variance, skewness, and kurtosis:

```bash
sudo bpftrace -q runqlat.bt | histo stats
```

### 3. Fitting Parametric Models to Network Latency

Fit statistical distributions directly to live TCP round-trip times:

```bash
sudo bpftrace -q tcp_rtt.bt | histo fit --model=lognormal
```

### 4. Serializing eBPF Distributions to JSON

Archive or transport kernel histograms as standardized `libhisto` JSON artifacts:

```bash
sudo bpftrace -q vfs_read_latency.bt | histo fill --output=json --output-file=vfs_latency.json
```

### 5. Live 2D Block I/O Heatmap Monitoring

Stream block request size vs latency into `histo top`'s interactive 2D viewer:

```bash
sudo bpftrace -q block_io_heatmap.bt | histo top --2d
```

---

## High-Throughput Ingestion via Shared Memory (`--shm`)

For production telemetry streaming exceeding 50,000,000 events/sec, bypass pipe serialization using memory-mapped ring buffers:

```bash
# Attach histo top to a POSIX shared memory ring buffer
histo top --shm=/histo_shm

# Scale incoming raw nanoseconds to microseconds
histo top --shm=/histo_shm --scale-input=0.001
```

C producers can write to the ring buffer using `tools/include/tui_shm.h`:

```c
#include "tui_shm.h"

histo_shm_t shm;
histo_shm_create(&shm, "/histo_shm", 65536, sizeof(double), 0);

double measurement = 142.5; // e.g. latency sample
histo_shm_push(shm.ring, &measurement);
```

---

## Security & Least Privilege Execution

Running eBPF tools with full `root` privileges is discouraged in secure production environments. Modern Linux kernels (>= 5.8) support granular capability isolation.

### 1. Minimum Required Capabilities

- `CAP_BPF`: Load and verify eBPF programs, create BPF maps.
- `CAP_PERFMON`: Attach kprobes, uprobes, and tracepoints.
- `CAP_NET_ADMIN`: Inspect network sockets and TCP tracepoints (for `tcp_rtt.bt`).

### 2. Granting Ambient Capabilities without Sudo

To permit an unprivileged user or telemetry daemon (`telem_user`) to execute `bpftrace` without `sudo`:

```bash
# Set file capabilities on bpftrace binary
sudo setcap 'cap_bpf,cap_perfmon,cap_net_admin+ep' $(which bpftrace)

# Or launch with systemd capability bounding sets
# (In systemd service unit):
# CapabilityBoundingSet=CAP_BPF CAP_PERFMON CAP_NET_ADMIN
# AmbientCapabilities=CAP_BPF CAP_PERFMON CAP_NET_ADMIN
```
