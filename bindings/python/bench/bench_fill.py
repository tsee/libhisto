#!/usr/bin/env python3
"""
Performance & Throughput Benchmark for Python bindings (histo / pyhisto).
"""

import time
import struct
import array
import histo

N_OPS = 2_000_000
N_PACKED = 5_000_000


def benchmark():
    print("=" * 80)
    print(" libhisto Python Ingestion & Processing Benchmark")
    print(f" Single fills: {N_OPS:,} iterations | Packed SIMD buffer: {N_PACKED:,} samples")
    print("=" * 80)

    # 1. 1D Uniform Fill (method call)
    h_uniform = histo.Histogram(bins=100, range=(0.0, 100.0))
    t0 = time.perf_counter()
    for i in range(N_OPS):
        h_uniform.fill(50.0)
    t1 = time.perf_counter()
    dt_uniform = t1 - t0
    mops_uniform = (N_OPS / dt_uniform) / 1e6

    # 2. 1D Weighted Fill with sumw2
    h_w2 = histo.Histogram(bins=100, range=(0.0, 100.0), track_sumw2=True)
    t0 = time.perf_counter()
    for i in range(N_OPS):
        h_w2.fill(50.0, weight=2.0)
    t1 = time.perf_counter()
    dt_w2 = t1 - t0
    mops_w2 = (N_OPS / dt_w2) / 1e6

    # 3. 1D Variable Bins (100 bins)
    edges = [float(i) for i in range(101)]
    h_var = histo.Histogram(edges=edges)
    t0 = time.perf_counter()
    for i in range(N_OPS):
        h_var.fill(50.5)
    t1 = time.perf_counter()
    dt_var = t1 - t0
    mops_var = (N_OPS / dt_var) / 1e6

    # 4. 1D Sequence batch fill (fill_n)
    h_seq = histo.Histogram(bins=100, range=(0.0, 100.0))
    seq_data = [50.0] * N_OPS
    t0 = time.perf_counter()
    h_seq.fill_n(seq_data)
    t1 = time.perf_counter()
    dt_seq = t1 - t0
    mops_seq = (N_OPS / dt_seq) / 1e6

    # 5. 1D Packed f64 Buffer (SIMD)
    h_packed = histo.Histogram(bins=100, range=(0.0, 100.0))
    packed_arr = array.array('d', [50.0] * N_PACKED)
    t0 = time.perf_counter()
    h_packed.fill_buffer(packed_arr)
    t1 = time.perf_counter()
    dt_packed = t1 - t0
    mops_packed = (N_PACKED / dt_packed) / 1e6

    # 6. 2D Uniform Fill (method call)
    h2d = histo.Histogram2D(xbins=20, xrange=(0.0, 100.0), ybins=20, yrange=(0.0, 100.0), track_sumw2=True)
    t0 = time.perf_counter()
    for i in range(N_OPS):
        h2d.fill(50.0, 50.0)
    t1 = time.perf_counter()
    dt_2d = t1 - t0
    mops_2d = (N_OPS / dt_2d) / 1e6

    # 7. 2D Packed f64 Buffer (SIMD)
    h2d_packed = histo.Histogram2D(xbins=20, xrange=(0.0, 100.0), ybins=20, yrange=(0.0, 100.0), track_sumw2=True)
    packed_2d_x = array.array('d', [50.0] * N_PACKED)
    packed_2d_y = array.array('d', [50.0] * N_PACKED)
    t0 = time.perf_counter()
    h2d_packed.fill_buffer(packed_2d_x, packed_2d_y)
    t1 = time.perf_counter()
    dt_2d_packed = t1 - t0
    mops_2d_packed = (N_PACKED / dt_2d_packed) / 1e6

    # 8. DDSketch Dynamic Insert
    sketch = histo.Sketch(alpha=0.01, max_bins=2048)
    t0 = time.perf_counter()
    for i in range(N_OPS):
        sketch.insert(50.0)
    t1 = time.perf_counter()
    dt_sketch = t1 - t0
    mops_sketch = (N_OPS / dt_sketch) / 1e6

    # 9. DDSketch Packed Buffer
    sketch_packed = histo.Sketch(alpha=0.01, max_bins=2048)
    t0 = time.perf_counter()
    sketch_packed.insert_buffer(packed_arr)
    t1 = time.perf_counter()
    dt_sketch_packed = t1 - t0
    mops_sketch_packed = (N_PACKED / dt_sketch_packed) / 1e6

    print(f"{'Operation':<35} | {'Count':>12} | {'Time (s)':>10} | {'Throughput':>15} | {'Latency':>12}")
    print("-" * 92)
    print(f"{'1D Uniform Fill (method call)':<35} | {N_OPS:>12,} | {dt_uniform:>10.4f} | {mops_uniform:>10.2f} Mops/s | {dt_uniform*1e9/N_OPS:>10.2f} ns/op")
    print(f"{'1D Weighted Fill + sumw2':<35} | {N_OPS:>12,} | {dt_w2:>10.4f} | {mops_w2:>10.2f} Mops/s | {dt_w2*1e9/N_OPS:>10.2f} ns/op")
    print(f"{'1D Variable Bins (100 bins)':<35} | {N_OPS:>12,} | {dt_var:>10.4f} | {mops_var:>10.2f} Mops/s | {dt_var*1e9/N_OPS:>10.2f} ns/op")
    print(f"{'1D Sequence Batch Fill (fill_n)':<35} | {N_OPS:>12,} | {dt_seq:>10.4f} | {mops_seq:>10.2f} Mops/s | {dt_seq*1e9/N_OPS:>10.2f} ns/op")
    print(f"{'1D Packed f64 Buffer (SIMD)':<35} | {N_PACKED:>12,} | {dt_packed:>10.4f} | {mops_packed:>10.2f} Mops/s | {dt_packed*1e9/N_PACKED:>10.2f} ns/op")
    print(f"{'2D Uniform Fill (method call)':<35} | {N_OPS:>12,} | {dt_2d:>10.4f} | {mops_2d:>10.2f} Mops/s | {dt_2d*1e9/N_OPS:>10.2f} ns/op")
    print(f"{'2D Packed f64 Buffer (SIMD)':<35} | {N_PACKED:>12,} | {dt_2d_packed:>10.4f} | {mops_2d_packed:>10.2f} Mops/s | {dt_2d_packed*1e9/N_PACKED:>10.2f} ns/op")
    print(f"{'DDSketch Dynamic Insert':<35} | {N_OPS:>12,} | {dt_sketch:>10.4f} | {mops_sketch:>10.2f} Mops/s | {dt_sketch*1e9/N_OPS:>10.2f} ns/op")
    print(f"{'DDSketch Packed Buffer':<35} | {N_PACKED:>12,} | {dt_sketch_packed:>10.4f} | {mops_sketch_packed:>10.2f} Mops/s | {dt_sketch_packed*1e9/N_PACKED:>10.2f} ns/op")
    print("=" * 92)


if __name__ == "__main__":
    benchmark()
