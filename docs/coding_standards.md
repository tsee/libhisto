# libhisto Coding Standards & C Guidelines

This document specifies the technical coding standards for `libhisto`.

---

## 1. Language & Portability

- **Standard**: ISO C99 (`-std=c99` or `-std=gnu99`).
- **Dependencies**: Standard C runtime only (`libc`, `libm`). No third-party runtime libraries.
- **Architectures**: Support 32-bit and 64-bit platforms, Little-Endian and Big-Endian memory layouts.
- **Compilers**: Must compile warning-free with `-Wall -Wextra -Wpedantic -Werror` on GCC and Clang, and `/W4 /WX` on MSVC.

---

## 2. API Design & Namespace

- **Prefixing**: Every public symbol must be prefixed with `histo_` (for types, functions, variables) or `HISTO_` (for macros, enums, constants).
- **Opaque Structures**: Expose histogram handles as opaque pointer types (e.g., `typedef struct histo histo_t;`) in public headers to encapsulate internal layout and preserve ABI stability.
- **Header Hygiene**: Public headers must include only the minimum necessary standard headers (`<stddef.h>`, `<stdint.h>`, `<stdbool.h>`). Internal implementation details remain in private headers (`src/*.h`).
- **Include Guards**: Use standard macro include guards or `#pragma once` (with macro fallback) in all headers:
  ```c
  #ifndef LIBHISTO_HISTO_H
  #define LIBHISTO_HISTO_H
  ...
  #endif /* LIBHISTO_HISTO_H */
  ```
- **C++ Compatibility**: All public headers must include `extern "C"` wrappers.

---

## 3. Memory Management & Safety

- **Ownership & Allocation**:
  - Memory allocated by `histo_create_*` must be freed explicitly via `histo_destroy`.
  - Never retain pointers to internal memory after a structure is destroyed.
  - Return `NULL` or appropriate error codes on allocation failure without leaking intermediate buffers.
- **Pointer Validation**: Check public API pointer arguments against `NULL`.
- **Zero-Tolerance for Leaks**: All code changes and test runs must run cleanly under AddressSanitizer (ASan) and Valgrind.

---

## 4. Error Handling & Return Codes

- **Status Enums**: Functions returning status should use a strongly typed enum (e.g., `histo_status_t` / `HISTO_OK`, `HISTO_ERR_*`).
- **Defensive Defaults**: On error conditions, out-parameters must be set to safe, deterministic values (e.g., `NULL` or `0`), not left uninitialized.
- **No Uncontrolled Exits**: Library code must never call `exit()`, `abort()`, or uncontrolled `assert()` in production paths. Errors must be propagated cleanly to the caller.

---

## 5. Numerical Robustness & Endian Handling

- **Floating-Point Types**: Default to `double` for sample values, weights, and statistical accumulators.
- **IEEE-754 Edge Cases**:
  - Values of `NaN`, `+Inf`, and `-Inf` must not cause undefined behavior or memory out-of-bounds.
  - Non-finite inputs must be directed to designated overflow/underflow or error accumulators per design specification.
- **Serialization Byte-Order**:
  - Binary serialization formats must define an explicit byte order (fixed canonical Little-Endian representation with endian conversion helpers on read/write).

---

## 6. Testing & Commits

- **Test-Driven Changes**: Every feature or bugfix must be accompanied by unit tests.
- **Pre-Commit Verification**: Run the full test suite and sanitizer checks prior to any git commit.
- **Commit Granularity**: Keep commits small, self-contained, and accompanied by informative commit messages.
