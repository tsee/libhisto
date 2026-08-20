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
- [ ] **STAT-2**: Non-parametric kernel density estimation (KDE) smoothing overlay for 1D histograms.
- [ ] **STAT-3**: Automated bin width optimization algorithms (Freedman-Diaconis, Scott's normal reference rule, Knuth's rule).

### 3. Bindings & Ecosystem
- [ ] **BIND-1**: Explore R and Julia foreign function interface (FFI) bindings for `libhisto`.
- [ ] **BIND-2**: Add Arrow / Polars zero-copy buffer interop in Python bindings.
- [ ] **BIND-3**: CPAN release of `Math-Histo` v0.1.1 and `Alien-libhisto` v0.1.1.

### 4. CLI Toolkit & Ergonomics
- [ ] **CLI-1**: Add `--csv-header` and column name selection for CSV batch streams in `histo-fill`.
- [ ] **CLI-2**: Interactive curses/terminal GUI mode for live histogram streaming (`histo-plot --interactive`).

---

## Milestone History
- **v0.1.0** (2026-08-20): Initial release of core 1D/2D C library, Levenberg-Marquardt fitting, DDSketch, SIMD (AVX2, AVX-512, NEON), CLI toolkit, Python and Perl bindings.
