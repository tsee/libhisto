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

## 5. Development Workflow & Verification Gate

`libhisto` provides granular and summary Makefile targets for core and multi-language development:

```bash
# 1. Build and test Core C library and CLI tools
make
make test        # Runs core C tests under AddressSanitizer & UBSan

# 2. Build and test all language bindings (Python, Perl, Node.js)
make bindings
make bindings-test

# 3. Run individual language bindings
make test-python
make test-perl
make test-node

# 4. Run the complete monorepo pre-release test suite (Sanitizers, TSan, Valgrind, Distributions, Docs)
make test-all
```

For AI agents working on the codebase, please review the operational rules in [`AGENTS.md`](AGENTS.md).
