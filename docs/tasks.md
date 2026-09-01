# libhisto Task Tracker & Roadmap

This document tracks active work items, planned enhancements, and development roadmap tasks for `libhisto`.

## Status Legend
- `[ ]` Not Started
- `[~]` In Progress / Blocked
- `[x]` Completed

---

## Active & Upcoming Roadmap (v0.2.0+)

### 1. Performance & Hardware Acceleration
- [ ] **SIMD-1**: Profile and optimize 2D SIMD batch fills on ARM64 NEON with strided memory access.
- [ ] **SIMD-2**: AVX-512 2D batch fill kernel tuning for wide grid histograms.
- [ ] **SIMD-3**: OpenMP / POSIX thread parallel map-reduce batch ingestion helper (`histo_fill_n_parallel`).

### 2. Analytical & Statistical Enhancements
- [ ] **STAT-1**: Binned multi-dimensional statistical metrics (2D Kolmogorov-Smirnov / Peacock 2D test).
- [x] **STAT-2**: Non-parametric kernel density estimation (KDE) engine (`histo_kde`, Gaussian, Epanechnikov, Silverman/Scott bandwidth).
- [x] **STAT-3**: Automated optimal bin width heuristics (Freedman-Diaconis, Scott, Sturges, Doane, Knuth's Bayesian rule).

### 3. Bindings & Ecosystem
- [ ] **BIND-1**: Explore R and Julia foreign function interface (FFI) bindings for `libhisto`.
- [ ] **BIND-2**: Add Arrow / Polars zero-copy buffer interop in Python bindings.
- [ ] **BIND-3**: CPAN release of `Math-Histo` v0.1.1 and `Alien-libhisto` v0.1.1.

### 4. CLI Toolkit & Ergonomics
- [ ] **CLI-1**: Add `--csv-header` and column name selection for CSV batch streams in `histo-fill`.
- [x] **CLI-2**: Interactive terminal monitoring & TUI exploration mode (`histo top` / `histo-top`).
- [x] **CLI-3**: Multi-preset colormap & palette engine across CLI tools (`histo plot`, `histo top`) with 8 standard perceptually uniform and scientific presets (`viridis`, `plasma`, `inferno`, `magma`, `turbo`, `cividis`, `grayscale`, `rainbow`).
- [x] **DOC-1**: Dedicated user manual for `histo top` ([`docs/top_manual.md`](top_manual.md)) and interactive CLI showcase in `README.md` and `docs/manual.md`.
- [x] **TEST-1**: Python 3 multi-architecture and portability test harness (`test_container.py`), Makefile targets (`test-matrix-local`, `portability`), and developer manual expansion.

---

## Milestone History
- **v0.1.0** (2026-08-20): Initial release of core 1D/2D C library, Levenberg-Marquardt fitting, DDSketch, SIMD (AVX2, AVX-512, NEON), CLI toolkit, Python and Perl bindings.
