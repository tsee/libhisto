# libhisto Work Items & Roadmap

## Status Legend
- `[ ]` Not Started
- `[~]` In Progress / Blocked
- `[x]` Completed

---

## Phase A: Research & High-Level Goals
- [x] **A1** Write `AGENTS.md` and coding standards documentation
- [x] **A2** Settle on license to use (MIT)
- [x] **A3** Analyze existing prior art software and distill requirements and techniques
- [x] **A4** Settle on build system and project structure (Modern CMake)



---

## Phase B: Design & Documentation
- [x] **B1** Core Data Structures & Types Specification
- [x] **B2** Public C API Specification

- [x] **B3** Numerical Behavior & Error Handling Strategy
- [x] **B4** Serialization & Wire Formats Specification
- [x] **B5** Statistical & Analytical Formulae Specification


---

## Phase C: Implementation
- [x] **C1** Internal memory & lifecycle routines (`histo_create_uniform`, `histo_create_variable`, `histo_destroy`, `histo_clone`, `histo_reset`) & Unit Tests
- [x] **C2** Boundary-guarded bin lookup & ingestion routines (`histo_find_bin`, `histo_fill`, `histo_fill_w`, `histo_fill_n`, `histo_fill_strided`, `histo_fill_bin`) & IEEE-754 edge-case Unit Tests
- [x] **C3** Bin querying & statistical analysis routines (`histo_bin_*`, `histo_mean`, `histo_variance`, `histo_std_dev`, `histo_quantile`, `histo_median`, `histo_integral`, `histo_get_stats`) & Unit Tests
- [x] **C4** Histogram arithmetic & transformations (`histo_add`, `histo_subtract`, `histo_multiply`, `histo_divide`, `histo_scale`, `histo_normalize`, `histo_rebin`, `histo_slice`, `histo_cdf`) & Unit Tests
- [x] **C5** Canonical Little-Endian binary serialization & deserialization (`histo_serialize_binary*`, `histo_deserialize_binary`) & roundtrip Integration Tests

- [x] **C6** End-to-end integration tests, sanitizer runs (ASan/UBSan), and cynical edge-case audit
- [x] **C7** Benchmarks & example applications
- [x] **C8** Multithreaded concurrency tests & dynamic analysis tooling (ASan, TSan, MSan, Valgrind Memcheck)
- [x] **C9** Doxygen API docstrings, User Manual (`docs/manual.md`), and GitHub `README.md`
- [x] **C10** Format migration and backward compatibility logic (`histo_migrate_binary`) & Tests

---

## Phase D: Advanced Analytical & Summary Capabilities
- [x] **D1** Higher-Order Statistical Moments (`histo_central_moment`, `histo_skewness`, `histo_kurtosis`, `histo_excess_kurtosis`) & Edge-Case Unit Tests
- [x] **D2** Peak, Mode & Shape Estimation (`histo_mode_bin`, `histo_mode_continuous`, `histo_fwhm`, `histo_rms`) & Sub-bin Interpolation Tests
- [x] **D3** Robust Non-Parametric Dispersion (`histo_iqr`, `histo_mad`, `histo_trimmed_mean`, `histo_winsorized_mean`) & Unit Tests
- [x] **D4** Two-Sample Statistical Distance & Hypothesis Testing (`histo_cmp_chi2`, `histo_cmp_ks`, `histo_cmp_wasserstein_1d`, `histo_cmp_kl_divergence`, `histo_cmp_bhattacharyya`) & Comparison Integration Tests
- [x] **D5** Update Doxygen documentation, user manual, and benchmarks for new analytical routines

---

## Phase E: Unix CLI Toolkit
- [x] **E1** Build Target & CLI Dispatcher Framework (`tools/`, `histo` multi-call binary, option parsing & stream utilities)
- [x] **E2** Streaming Ingestion Tool `histo-fill` (Text/CSV/binary streams, auto-ranging, variable edges, snapshot streaming, JSON/binary emission)
- [x] **E3** Terminal Visualization Tool `histo-plot` (ASCII & Unicode blocks, ANSI color gradients, error whiskers, live `--watch` mode)
- [x] **E4** Inspection & Comparison Tools `histo-stats` and `histo-cmp` (Moments, robust dispersion tables, two-sample hypothesis testing)
- [x] **E5** CLI Integration Test Suite (`tests/integration/test_cli.c`), Sanitizer Verification (ASan/UBSan/Valgrind), and Documentation Updates

---

## Phase F: High-Performance Acceleration & Streaming Sketches
- [x] **F1** Runtime-Detected AVX2 / AVX-512 Vector Acceleration (`histo_fill_n` SIMD kernel)
- [x] **F2** Online Dynamic Quantile Sketches (DDSketch relative-error bounded streaming sketch in `include/histo/sketch.h` & `src/sketch.c`)
- [x] **F3** Sparklines in Terminal Visualization Toolkit (`histo-plot -S / --sparkline`)

---

## Phase G: Fuzzing & Security Hardening
- [x] **G1** Coverage-guided & standalone fuzzing harnesses (`fuzz_deserialize_binary`, `fuzz_deserialize_json`, `fuzz_sketch_binary`, `fuzz_fill`)
- [x] **G2** Modern CMake integration (`LIBHISTO_ENABLE_FUZZING`, LLVM `libFuzzer` Clang support & portable standalone mutation engine)
- [x] **G3** Curated seed corpora in `tests/fuzz/corpus/`, corpus generator tool, and 100% leak-free ASan/UBSan verification
