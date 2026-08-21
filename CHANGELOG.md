# Changelog

All notable changes to the `libhisto` project (C core library, CLI toolkit, Python bindings, and Perl bindings) will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Added
- **Automated Optimal Bin Width Heuristics** (`include/histo/histo.h`, `src/histo.c`):
  - Freedman-Diaconis rule (`histo_estimate_bins_fd`).
  - Scott's normal reference rule (`histo_estimate_bins_scott`).
  - Sturges' rule (`histo_estimate_bins_sturges`).
  - Doane's skewness-adjusted rule (`histo_estimate_bins_doane`).
  - Knuth's Bayesian optimal binning rule (`histo_estimate_bins_knuth`).
  - Unified estimator (`histo_estimate_bins`) and automated histogram constructor (`histo_create_auto`).
  - Python binding `Histogram.auto(data, rule="auto")`.
  - Perl binding `Math::Histo->create_auto($data, rule => 'auto')`.
  - CLI `histo-fill --auto-bins[=RULE]` (or `-a`) and auto-binning fallback in `histo plot`.
- **Real-Time Interactive Terminal Monitor (`histo top` / `histo-top`)** (`tools/src/cmd_top.c`, `tools/src/tui_engine.c`, `tools/src/tui_term.c`):
    - 2-thread decoupled architecture: unblocked ingestion worker draining `stdin` at wire speed while UI renders off atomic deep-copy snapshots.
    - View freeze mode (`Space`): freezes display frame for inspection without pipe backpressure or dropped samples.
    - In-memory rolling reservoir sample cache ($100,000$ samples) for on-the-fly lossless dynamic rebinning (`r`/`R` or `:bins <N>`).
    - Interactive Command Prompt (`:`): Tab-completion, history navigation (`↑`/`↓`), inline validation for `:bins`, `:range`, `:fit`, `:kde`, `:scale`, `:export`, `:clear`.
    - Live multi-column switching (`Tab` / `Shift-Tab` / `1..9` / `:col <N>` / `:xcol <N>` / `:ycol <N>`) buffering up to 16 columns per sample.
    - Rolling sample windowing (`w` / `:window <N>`) and real-time exponential decay attenuation (`:decay <lambda>`).
    - SGR-1006 mouse protocol support with mouse wheel zoom in/out and click-to-inspect bar tooltips.
    - Auto-fit bins to terminal height (`B` / `:bins auto`) dynamically maintaining 1 bin per viewport row on terminal resize.
    - Minimalist compact header mode (`H` / `:compact`) reclaiming terminal rows for maximal bar drawing height.
    - Interactive snapshot export (`s` / `:save [path]` / `:export [path]`) saving binary `.histo` or formatted `.json` on the fly.
    - Two-panel interactive online help modal (`?`) with complete keyboard shortcuts and `:command` reference.
    - Multi-axis logarithmic scaling (`l`/`L`): Log Y (counts), Log X (coordinates), Log-Log, and Log Z (2D TrueColor heatmaps).
    - High-contrast Monochrome mode (`C` / `-M` / `--mono` / `NO_COLOR`) and 24-bit TrueColor support.
    - Portable threading abstraction (`tools/include/tui_thread.h`) supporting POSIX `pthread`, Windows Win32 API, and single-threaded fallback.


---

## [0.1.0] - 2026-08-20

### Added
- Automated version check and bump synchronization script (`tools/scripts/bump_version.py`).
- Comprehensive release guide (`docs/release_guide.md`) and AGENTS policy integration.
- SIMD vector acceleration for weighted batch fills (`sum_w2` tracking) across AVX2, AVX-512, and ARM NEON.

### Fixed
- Fixed DDSketch quantile rank calculation on fractional / continuous sample weights (`src/sketch.c`).
- Symmetrized DDSketch negative bin representative value reconstruction (`src/sketch.c`).
- Fixed signed modulo wrapping for negative logarithmic bucket indices in DDSketch store (`src/sketch.c`).

### Added
- **Core 1D Histogramming Engine**:
  - High-performance cache-aligned 1D histograms with uniform and variable binning.
  - Sub-3ns scalar fill, SIMD vectorization (AVX2, AVX-512, ARM NEON).
  - Online Welford exact moment tracking (`mean`, `variance`, `std_dev`, central moments $M_k$, `skewness`, `kurtosis`).
  - Robust dispersion metrics (`median`, `iqr`, `mad`, `trimmed_mean`, `winsorized_mean`).
  - Peak and mode estimation (`mode_bin`, continuous 3-point parabolic `mode_continuous`, `fwhm`, `rms`).
  - Two-sample comparison metrics ($\chi^2$, Kolmogorov-Smirnov, 1D Wasserstein / EMD, Kullback-Leibler divergence, Bhattacharyya distance).
  - Arithmetic operations (`add`, `subtract`, `multiply`, `divide`, `scale`, `normalize`, `rebin`, `slice`, `cdf`).
  - Endian-safe canonical binary wire format V1/V2 with format migration and JSON serialization.
- **2D Histogramming Engine (`histo2d`)**:
  - All 4 binning grid combinations (Uniform-Uniform, Uniform-Variable, Variable-Uniform, Variable-Variable).
  - Online bivariate Welford covariance $C_{xy}$ and Pearson correlation $\rho_{xy}$ tracking.
  - 9-region partitioned out-of-bounds guard tracking.
  - Projections (`project_x`, `project_y`), slices (`slice_x`, `slice_y`), and profile histograms (`profile_x`, `profile_y`).
  - Format V3 binary wire serialization.
- **Non-Linear Curve Fitting Engine (`histo_fit`)**:
  - Levenberg-Marquardt optimizer with adaptive damping $\lambda$ and QR decomposition.
  - Linear least squares for polynomials.
  - Built-in models: Gaussian, Exponential decay, Polynomial (up to degree 10), Breit-Wigner, Power law.
  - Custom user callbacks with finite-difference numerical gradients.
  - Loss functions: Weighted $\chi^2$, Poisson MLE (Cash / Baker-Cousins), unweighted least squares.
  - Box constraints, fixed parameter masks, parameter standard errors, covariance/correlation matrices, $\chi^2$ p-values (upper incomplete gamma).
- **Online Dynamic Quantile Sketches (`histo_sketch`)**:
  - Logarithmic $\epsilon$-relative error dynamic quantile sketch based on DDSketch.
  - Mergeable streaming histograms supporting positive, negative, and zero values.
- **Multi-Call CLI Toolkit (`histo`)**:
  - Subcommands: `histo-fill`, `histo-plot` (Unicode braille/sparklines/heatmaps), `histo-stats`, `histo-fit`, `histo-cmp`.
- **Perl Bindings (`bindings/perl/`)**:
  - `Alien::libhisto`: Hermetic CPAN alien package for fetching/building the C library.
  - `Math::Histo`: Full XS wrapper covering 1D, 2D, Curve Fitting, DDSketch, and batch SIMD ingestion.
- **Python Bindings (`bindings/python/`)**:
  - Native CPython extension (`_libhisto`) + idiomatic Python package (`histo` / `pyhisto`).
  - Buffer Protocol zero-copy ingestion for NumPy arrays and array-like objects.
  - Universal Histogram Interface (UHI) protocol compliance (`.axes`, `.values()`, `.variances()`, `.counts()`, `.kind`).
  - Hermetic `sdist` bundling for PyPI wheel and tarball distribution.
- **Comprehensive Quality & Fuzzing Harness**:
  - 100% test coverage across Unit, Integration, Sanitizers (ASan, UBSan, TSan), Valgrind Memcheck.
  - LLVM libFuzzer and portable standalone fuzz regression targets (`tests/fuzz/`).
  - Complete Doxygen API documentation (`make docs`).
