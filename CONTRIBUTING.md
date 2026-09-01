# Contributing to libhisto

Thank you for your interest in contributing to `libhisto`. This document outlines the engineering guidelines, coding standards, and testing expectations for all contributions.

---

## 1. Language & Portability Standards

- **Standard**: Strict **ISO C99** (`-std=c99`).
- **Dependencies**: Standard C runtime only (`libc`, `libm`). No external runtime dependencies.
- **Architectures**: Support 32-bit and 64-bit platforms, Little-Endian and Big-Endian memory layouts.
- **Compilers**: Must compile cleanly with zero warnings under `-Wall -Wextra -Wpedantic -Werror` on GCC and Clang, and `/W4 /WX` on MSVC.

---

## 2. API Design & Namespace Conventions

- **Prefixing**: Every public symbol must use the `histo_` prefix (for types, functions, variables) or `HISTO_` prefix (for macros, enums, constants).
- **Opaque Handles**: Expose histogram instances as opaque pointer types (`histo_t*`, `histo2d_t*`, `histo_sketch_t*`) in public headers to encapsulate internal layout and preserve ABI stability.
- **Header Hygiene**: Public headers (`include/histo/*.h`) include only the minimum necessary standard headers (`<stddef.h>`, `<stdint.h>`, `<stdbool.h>`). Internal implementation details remain in private headers (`src/*.h`).
- **C++ Compatibility**: All public headers include `extern "C"` wrappers and include guards.

---

## 3. Memory Safety & Error Handling

- **Explicit Lifecycle**: Every allocated structure must have a matching destructor (`histo_destroy`, `histo2d_destroy`, `histo_fit_result_destroy`, `histo_sketch_destroy`).
- **Pointer Validation**: Validate public API pointer arguments against `NULL`.
- **Status Codes**: Functions returning status use `histo_status_t` (`HISTO_OK`, `HISTO_ERR_*`).
- **Zero-Tolerance for Leaks**: All code must run leak-free under AddressSanitizer (ASan) and Valgrind.
- **No Uncontrolled Exits**: Library code must never call `exit()` or `abort()` in runtime paths.

---

## 4. Numerical Robustness

- **Floating-Point Precision**: Default to `double` for sample values, weights, and accumulators.
- **Compiler Flags**: `-ffast-math` is strictly **prohibited** as it invalidates `isnan()`, `isinf()`, subnormals, and IEEE-754 compliance.
- **Deterministic Edge Cases**: Explicit handling of IEEE-754 floating-point edge cases (`NaN`, `+Inf`, `-Inf`, subnormals, `-0.0`).
- **Endian Independence**: Wire formats enforce explicit canonical Little-Endian representation.

---

---

## 5. Testing Strategy & Multi-Tier Verification

`libhisto` enforces a 3-tier testing strategy ensuring portability across CPU architectures (x86_64, ARM64, s390x Big-Endian, RISC-V, 32-bit x86/ARM), operating systems, and C standard libraries:

```
 ┌────────────────────────────────────────────────────────┐
 │ Tier 1: Fast Inner-Loop (Local Native, < 5s)           │  make test / make test-asan
 ├────────────────────────────────────────────────────────┤
 │ Tier 2: Local Hermetic Matrix (Docker/Podman, < 1m)    │  make test-matrix-local (s390x, musl, 32bit...)
 ├────────────────────────────────────────────────────────┤
 │ Tier 3: Pre-Release Portability Gate                   │  make portability (test-all + full matrix)
 └────────────────────────────────────────────────────────┘
```

### 5.1 Tier 1: Local Native Inner-Loop (Host)

Immediate feedback for day-to-day development using host compilers and dynamic instrumentation:

```bash
# Core C library and CLI with ASan + UBSan
make test

# Thread safety & data race detection
make test-tsan

# Memory leak and uninitialized access checking
make memcheck

# Documentation inline examples validation
make test-doc-examples

# Rapid 32-bit pointer model validation via host multilib (-m32)
make test-32bit-native
```

### 5.2 Tier 2: Local Hermetic Matrix (Containers & Emulation)

Containerized testing via Docker or Podman with transparent QEMU user-mode emulation for foreign CPU architectures and alternative C runtimes:

| Target | Command | Image / Runtime | Key Validation in `libhisto` |
| :--- | :--- | :--- | :--- |
| **Hermetic Core C99** | `make test-hermetic` | `debian:bookworm-slim` (`linux/amd64`) | Minimal baseline toolchain (`gcc` + `make` + `cmake`); zero Python, Perl, Node, or external interpreters. |
| **Strict C99 / Musl** | `make test-musl` | `alpine:latest` (`linux/amd64`) | `musl` libc compliance; catches non-standard GNU extension leaks. |
| **Big-Endian Wire Format** | `make test-big-endian` | `ubuntu:24.04` (`linux/s390x`) | Endian byte-swapping macros and serialization wire format in [`src/serialize.c`](src/serialize.c). |
| **32-Bit Userland** | `make test-32bit` | `debian:bookworm-slim` (`linux/386`) | 32-bit `size_t` limits and integer overflow in bin index math. |
| **ARM NEON SIMD** | `make test-arm64` | `ubuntu:24.04` (`linux/arm64`) | ARM64 NEON vector acceleration in [`src/simd_neon.c`](src/simd_neon.c). |
| **Embedded 32-Bit ARM** | `make test-armv7` | `ubuntu:24.04` (`linux/arm/v7`) | 32-bit alignment and scalar/NEON fallback kernels. |
| **RISC-V Architecture** | `make test-riscv64` | `ubuntu:24.04` (`linux/riscv64`) | 64-bit RISC-V portable scalar math execution. |
| **Full Local Matrix** | `make test-matrix-local` | *Runs all above targets* | Complete multi-architecture and libc validation pass. |

#### Python 3 Test Harness CLI (`tools/scripts/test_container.py`)

The test harness can also be invoked directly with granular options:

```bash
# List all available target architectures and profiles
python3 tools/scripts/test_container.py --list

# Run specific target with 8 parallel jobs
python3 tools/scripts/test_container.py --target s390x -j 8

# Run full suite (including language bindings) inside Alpine
python3 tools/scripts/test_container.py --target musl --full

# Clean up container build directories
python3 tools/scripts/test_container.py --clean
```

### 5.3 Language Bindings & Monorepo Targets

```bash
# Build and test all language bindings (Python, Perl, Node.js)
make bindings
make bindings-test

# Run individual language bindings
make test-python
make test-perl
make test-node

# Run complete monorepo test suite (Sanitizers, TSan, Valgrind, Distributions, Docs)
make test-all
```

### 5.4 Pre-Release Portability Gate (`make portability`)

Before tagging a release or merging major architectural modifications, run the complete portability gate:

```bash
make portability
```
This runs native `make test-all` (Release, ASan, UBSan, TSan, Valgrind, doc tests, hermetic Python/Perl/Node dist builds) followed by the complete multi-architecture and multi-libc container matrix in full mode.

---

## 6. Software Dependencies

The development and test harnesses require the following toolchains:

### Core C Toolchain
| Tool | Minimum Version | Package Name (Debian/Ubuntu) | Purpose |
| :--- | :--- | :--- | :--- |
| **CMake** | 3.15+ | `cmake` | Build system configuration |
| **GCC** or **Clang** | GCC 9+ / Clang 10+ | `build-essential` or `clang` | C99 compiler and linker |
| **Multilib C Headers** | - | `gcc-multilib libc6-dev-i386` | Native 32-bit (`-m32`) testing |
| **Valgrind** | 3.15+ | `valgrind` | Memory leak and corruption analysis (`make memcheck`) |
| **Doxygen** | 1.9+ | `doxygen graphviz` | API documentation generation (`make docs`) |

### Language Binding Toolchains (Optional for Core C, Required for `make test-all`)
| Runtime / Tool | Minimum Version | Package / Setup | Purpose |
| :--- | :--- | :--- | :--- |
| **Python 3** | 3.8+ | `python3 python3-dev python3-setuptools` | Python C-extension and test runner |
| **NumPy** | 1.20+ | `python3-numpy` | Python buffer protocol and array tests |
| **Perl** | 5.20+ | `perl libperl-dev` | XS bindings (`Math::Histo`, `Alien::libhisto`) |
| **cpanminus** | - | `cpanminus` | Perl dependency management |
| **Node.js & npm** | 16.0+ | `nodejs npm` | Native Node-API addon (`bindings/node`) |
| **node-gyp** | - | `npm install -g node-gyp` | Node.js native compilation |

### Container & Emulation Toolchain (Optional for Core C, Required for `test-matrix-local` & `portability`)
| Tool | Setup Command | Purpose |
| :--- | :--- | :--- |
| **Docker** or **Podman** | `sudo apt install docker.io` or `podman` | Container runtime engine |
| **QEMU User-Mode Binfmt** | `docker run --rm --privileged multiarch/qemu-user-static --reset -p yes` | Foreign CPU architecture emulation (s390x, ARM, RISC-V) |

> **Docker / Podman Permissions Note**: 
> - If using Docker, ensure your user has permission to communicate with the daemon socket: `sudo usermod -aG docker $USER` (requires logging out and back in).
> - Alternatively, install **Podman** (`sudo apt install podman`), which runs completely rootless without requiring daemon socket permissions. `test_container.py` auto-detects and uses whichever engine is working.

---

## 7. AI Agent Guidelines

For AI agents working on the codebase, please review the operational rules in [`AGENTS.md`](AGENTS.md).

