# AGENTS.md: Instructions & Guidelines for AI Agents

This document defines the operational workflow, engineering standards, and communication rules for AI agents working on the `libhisto` codebase.

---

## 1. Communication Style & Protocol

- **Productive Brevity**: Keep all responses concise, direct, and actionable.
- **Strictly Factual**: Never use unnecessary compliments, flattery, or conversational filler. State facts, technical trade-offs, results, and recommendations clearly.
- **Phase Discipline**: Strictly adhere to the project phases (**Phase A**: Research & Goals, **Phase B**: Design & Docs, **Phase C**: Implementation). **Never proceed to the next phase without explicit user confirmation.**
- **Documentation & Change Propagation**: For ALL code changes, agents MUST actively consider and implement respective updates to:
  - **Documentation**: Both inline Doxygen docstrings (`include/histo/*.h`, `src/`) and user documentation in [`docs/`](docs/) (including algorithmic complexity tables, manuals) and [`README.md`](README.md).
  - **Tests**: Unit tests, integration test suites, boundary edge cases, and concurrency tests.
  - **Benchmarks**: Micro-benchmarks and aggregate function benchmarks in [`bench/`](bench/) to ensure performance regressions are prevented.
  - **Serialization Logic**: **Most importantly**, any modification to data structures, histogram fields, metadata, accumulators, statistical moments, or feature flags MUST be evaluated for respective updates to the serialization and deserialization wire format ([`src/serialize.c`](src/serialize.c), [`docs/serialization_format.md`](docs/serialization_format.md), and binary roundtrip tests).
  - **Language Bindings**: Any modification, addition, or deprecation to the C public API (`include/histo/*.h`), struct fields, feature flags, or serialization formats MUST be evaluated and propagated to all active language bindings (`bindings/perl/`, `bindings/python/`, etc.).
- **Progress Tracking**: Keep [`docs/tasks.md`](docs/tasks.md) updated as tasks start (`[~]`) and complete (`[x]`).

---

## 2. Git & Commit Discipline

- **Frequent Turn Commits**: Do not leave uncommitted changes lying around. Typically create a commit after every turn, provided changes are self-contained, all tests pass, and standards are satisfied.
- **Atomic Commits**: Make small, self-contained commits focused on a single logical change.
- **Separate Binding Commits**: When updating language bindings due to core `libhisto` C API changes, **always update and commit the language bindings in separate commits** from the core C library commits (e.g., `feat(core): add XYZ` followed by `feat(bindings/perl): update XS wrapper for XYZ`).
- **Descriptive Messages**: Write clear, imperative commit messages (e.g., `feat(binning): implement uniform bin lookup with bounds check`).
- **Zero Broken Commits**: **Every commit containing code changes MUST pass the entire test suite cleanly before committing.** No broken states on any branch.



---

## 3. Testing & Verification Standards

- **Exhaustive Correctness Testing**: Every feature, error branch, numerical routine, and boundary condition must be covered by thorough, deterministic tests.
- **Edge-Case Vigilance**: Actively design, identify, and test edge cases at every step of development. This includes:
  - IEEE-754 specials: `NaN` (quiet/signaling), `+Inf`, `-Inf`, `-0.0`, subnormals/denormals, near-overflow extremes ($10^{308}$, $-10^{308}$).
  - Boundary transitions: exact lower/upper edges ($x = x_{\min}$, $x = x_{\max}$, $x = x_i \pm \epsilon$), non-power-of-two divisions (e.g. $N=3, 7$).
  - Storage extremes: zero-weight histograms, single-entry histograms, identical sample streams (testing variance floor), negative weights, sparse bins with plateaus.
  - Parameter bounds: `nbins = 0`, `nbins > HISTO_MAX_NBINS`, inverted ranges (`min >= max`), non-monotonic variable edges, duplicate edges.
- **Clear Separation**: Maintain a strict distinction between:
  - **Unit Tests**: Isolated testing of internal helper functions, individual bin lookup math, edge cases.
  - **Integration & API Tests**: End-to-end testing of public APIs, multi-step workflows, serialization roundtrips.
- **Pre-Commit Test Sufficiency Analysis**:
  - Before committing code, evaluate whether tests cover all newly introduced logic and edge cases.
  - If coverage or edge case behavior is in doubt, invoke a subagent as a cynical testing expert to rigorously audit test coverage and propose adversarial edge cases.
- **Sanitizers**: Code must pass AddressSanitizer (ASan) and UndefinedBehaviorSanitizer (UBSan) with zero warnings or leaks.


---

## 4. Code Standards & Portability

- **C Standard**: Strict **ISO C99**.
- **Cross-Platform & Multi-Compiler**:
  - Compilers: GCC, Clang, MSVC.
  - Platforms: Linux, Windows, macOS, BSD, embedded targets.
  - Clean compilation with strict flags (e.g., `-Wall -Wextra -Werror -pedantic`).
- **Endian Independence**:
  - Must run correctly on both Little-Endian and Big-Endian architectures.
  - Serialization / deserialization formats must enforce explicit, deterministic byte ordering (e.g., canonical little-endian or network byte order) to allow portable data interchange across machines.
- **Namespace Cleanliness**:
  - All public types, functions, constants, and macros must use a strict prefix (`histo_` or `HISTO_`) to avoid collisions in client applications.
- **Memory Safety & Lifecycle**:
  - Explicit ownership semantics: every allocated resource must have a clearly defined destructor/free path.
  - Zero memory leaks. Validate with ASan / Valgrind.
  - Defensive API checks: Validate pointer arguments against `NULL` and check numeric bounds before dereferencing or indexing.
- **Numerical Correctness**:
  - Explicit, deterministic handling of IEEE-754 floating-point edge cases (`NaN`, `+Inf`, `-Inf`, subnormals, negative zero).
  - Numerically stable algorithms for statistical accumulators (e.g., Welford's algorithm for online variance).
- **Top-of-File Comment & File Cohesion**:
  - Every C source (`.c`) and header (`.h`) file MUST begin with a concise comment describing its purpose and scope (maximum two 80-character lines).
  - If a file's scope and purpose cannot be cleanly expressed in a concise 2-line comment due to lack of cohesion, evaluate whether to refactor the file into focused, cohesive units without introducing performance penalties or unwarranted complexity. If in doubt, prompt the user for architectural guidance.

---

## 5. Release, Versioning & Tagging Policy

- **Semantic Versioning 2.0.0**:
  - `MAJOR`: Incompatible C API/ABI modifications, struct layout breaks, or breaking wire format changes.
  - `MINOR`: New backward-compatible C API routines, feature flags, wire format extensions, or analytical capabilities.
  - `PATCH`: Backward-compatible bug fixes, SIMD vector optimizations, documentation/test enhancements.
- **Single Source of Truth**:
  - Core C version resides in [`include/histo/version.h`](include/histo/version.h) (`HISTO_VERSION_*` macros) and [`CMakeLists.txt`](CMakeLists.txt) (`project(libhisto VERSION X.Y.Z)`).
  - Language bindings synchronize major/minor with core: `bindings/python/pyproject.toml`, `bindings/perl/Math-Histo/lib/Math/Histo.pm`, `bindings/perl/Alien-libhisto/lib/Alien/libhisto.pm`.
  - Automated version synchronization and verification is enforced via `python3 tools/scripts/bump_version.py --check`.
- **Changelog Hygiene**:
  - Root [`CHANGELOG.md`](CHANGELOG.md) must be maintained under Keep a Changelog standards (`[Unreleased]`, `[X.Y.Z] - YYYY-MM-DD`), categorized across Core C, CLI, Python, and Perl.
  - CPAN `Changes` files in `bindings/perl/*/Changes` must be updated on Perl binding releases following CPAN standards.
- **Release Tagging**:
  - Coordinated monorepo releases: Annotated git tag `vX.Y.Z` (e.g., `v0.1.0`).
  - Independent out-of-band binding releases: Namespaced tags `perl-math-histo-vX.Y.Z`, `python-histo-vX.Y.Z`.
- **Pre-Release Verification Gate**:
  - No release or tag may be made without a 100% clean run of `make test-all` (Release, ASan/UBSan, TSan, Valgrind memcheck, Python sdist hermetic tests, Perl dist hermetic tests, Doxygen documentation).

