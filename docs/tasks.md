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


