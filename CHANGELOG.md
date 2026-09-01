# Changelog

All notable changes to the `libhisto` project (C core library, CLI toolkit, Python bindings, and Perl bindings) will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Added
- **Zero-Overhead Modern C++ Interface** (`include/histo/*.hpp`, `#include <histo/histo.hpp>`):
  - Strict **Google C++ Style Guide** compliance: exception-free architecture (`-fno-exceptions`), explicit single ownership RAII, move semantics, and `std::span` zero-copy ingestion.
  - Exception-free `libhisto::Status` and `libhisto::Result<T>` outcome monads with `HISTO_ASSIGN_OR_RETURN` and `HISTO_RETURN_IF_ERROR` macros.
  - Owning `libhisto::Histogram` (1D) and `libhisto::Histogram2D` (2D) handles with direct constructors, `std::initializer_list` and `std::span` ingestion, operators (`+=`, `*=`, `<<`), and binary serialization.
  - Zero-overhead non-owning views `libhisto::HistogramView` and `libhisto::Histogram2DView` providing zero-copy interoperability over existing C `histo_t*` / `histo2d_t*` pointers (`sizeof(View) == sizeof(void*)`).
  - Range-based `for` loop bin iterators (`for (const auto& bin : h)`).
  - Streaming sketches (`libhisto::DDSketch`), continuous Kernel Density Estimation (`libhisto::KDE`), and non-linear curve fitting (`libhisto::Fit`).
  - Decoupled C++ test suite (`tests/cpp/`, `make test-cpp`) that builds only when a C++ compiler is detected, ensuring basic C builds require zero C++ compiler dependencies.

---

## [0.3.0] - 2026-08-29

### Added
- **Interactive Terminal Monitor (`histo top`) High-Performance Overhaul**:
  - Raw binary stream ingestion (`--binary-f64` / `--binary`) supporting >75,000,000 samples/sec with 1D and 2D weighted streaming.
  - High-speed memory-mapped shared memory binary ring buffer (`histo top --shm=<path>`).
  - Reservoir batch sampling and branchless ring updates reducing lock acquisition frequency by 64x.
  - Pre-scaling factor support (`--scale-input=<factor>` / `-S <factor>`).
- **bpftrace Output Stream Auto-Detection & Importer** (`tools/src/cli_common.c`):
  - Native parsing for log2 (`@ = hist(...)`) and linear (`@ = lhist(...)`) bpftrace tables and BCC arrow format.
  - Unit multiplier decoding (`ms`, `us`, `ns`, `K`, `M`, `G`, `T`, `KiB`, `MiB`).
- **Universal Histogram Interface (UHI) Protocol in Python** (`bindings/python/histo/uhi.py`):
  - Added full UHI protocol support across 1D and 2D histograms, including `HistogramAxis`, `.axes`, `.values()`, `.variances()`, `.counts()`, discrete/circular traits, and SciPy / boost-histogram converters.
- **Node.js / TypeScript Bindings** (`bindings/node/`):
  - High-performance N-API native addon with TypeScript definitions, zero-copy TypedArray ingestion, statistical moments, curve fitting, KDE, DDSketch, and wire serialization.
- **Declarative, Table-Driven CLI Parser** (`tools/src/cli_opt.c`, `tools/include/cli_opt.h`):
  - Replaced ad-hoc parsing across all CLI subcommands with a portable, type-safe option parser supporting attached values, negative floating-point arguments, flag bundling, and auto-generated help.
- **Production eBPF Tracing Recipes** (`examples/ebpf/`):
  - Added latency profiling scripts for VFS read, block I/O heatmaps, TCP RTT, run queue latency, and page faults.

### Fixed & Improved Portability
- **Native Windows MSVC & Windows Console Support**:
  - Added `<intrin.h>` `__cpuid` / `__cpuidex` dynamic AVX2 / AVX-512 detection under MSVC.
  - Implemented Windows Console API raw mode and virtual terminal processing (`ENABLE_VIRTUAL_TERMINAL_PROCESSING`).
  - Added MSVC C-mode `inline` shims and suppressed floating-point constant overflow warnings on `-INFINITY`.
- **AArch64 / ARM SIMD Compilation**:
  - Added `CheckCCompilerFlag` in CMake to prevent passing `-mavx2` / `-mfma` to ARM compilers.
- **Perl Backward Compatibility (`Math-Histo`)**:
  - Bundled `Devel::PPPort` `ppport.h` providing compatibility shims (`av_top_index`, `hv_stores`, etc.) for Perl 5.8 through 5.40+.


---

## [0.2.1] - 2026-08-25 (Perl Bindings: Math::Histo, Math::Histo::PDL)

### Fixed
- **Perl Bindings (`Math-Histo`, `Math-Histo-PDL`)**:
  - Fixed test suite failures on extended-precision Perl builds configured with `long double` or `__float128` (`-Dusequadmath`) NV types ([GH #1](https://github.com/tsee/libhisto/issues/1)).
  - Replaced strict string equality assertions in `Test2::V0` tests with numerical tolerance checks via `within()`.
  - Resolved bin-boundary aliasing in `t/02_stats.t` Gaussian mode estimation.

---

## [0.2.0] - 2026-08-21

### Added
- **Expanded Curve Fitting Models & Gradient Evaluators** (`include/histo/fit.h`, `src/fit.c`):
  - Added 6 new built-in parametric models: Log-Normal, Gaussian + Linear Background, Weibull, Gamma (Erlang), Poisson, and Laplace.
  - Added public analytical Jacobian gradient evaluator `histo_fit_eval_gradient()` and model evaluator `histo_fit_eval()`.
  - Added Python bindings `histo.fit.eval_model()` and `histo.fit.eval_gradient()`.
  - Added Perl bindings `Math::Histo::Fit->eval()` and `Math::Histo::Fit->eval_grad()`.
  - Added ASCII plot overlays and multi-model CLI support in `histo fit`.
- **Perl Math::Histo::PDL Distribution** (`bindings/perl/Math-Histo-PDL/`):
  - Zero-copy 1D and 2D piddle ingestion via `get_dataref` directly into C `fill_packed_f64`.
  - Export 1D/2D histograms to PDL matrices, coordinates, bin edges, bin centers, and error arrays.
  - High-level builders (`hist1d`, `hist2d`, `pdl_to_histo`, `histo_to_pdl`) and OO methods on `Math::Histo` and `Math::Histo::2D`.
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
- **Top-of-File Comment & File Cohesion Standard** (`AGENTS.md`):
  - Standardized concise (max 2 lines, $\le 80$ chars) purpose/scope comments across all 80 C source and header files.

### Fixed
- Fixed Weibull analytical gradient calculation $\partial f/\partial \lambda$ in `src/fit.c`.
- Synchronized `sum_w2` tracking across AVX2, AVX-512, and NEON SIMD backends with scalar behavior.
- Fixed ThreadSanitizer data race in TUI background ingestion during dynamic column switching.
- Fixed Valgrind uninitialized memory check in terminal mouse escape sequence parser.
- Fixed 2D JSON serialization total entries count preservation in `src/serialize_2d.c`.
- Fixed POD formatting syntax errors in `Math::Histo::PDL`.
- Fixed Alien-libhisto in-tree build header synchronization cache.

### Added
- Automated version check and bump synchronization script (`tools/scripts/bump_version.py`).
- Comprehensive release guide (`docs/release_guide.md`) and AGENTS policy integration.
- SIMD vector acceleration for weighted batch fills (`sum_w2` tracking) across AVX2, AVX-512, and ARM NEON.

### Fixed
- Fixed DDSketch quantile rank calculation on fractional / continuous sample weights (`src/sketch.c`).
- Symmetrized DDSketch negative bin representative value reconstruction (`src/sketch.c`).
- Fixed signed modulo wrapping for negative logarithmic bucket indices in DDSketch store (`src/sketch.c`).
---

## [0.1.0] - 2026-08-20

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
