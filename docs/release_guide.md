# libhisto Release Guide

This document outlines the release, versioning, and tagging workflow for `libhisto` across the core C library and its language bindings.

---

## 1. Versioning Architecture

`libhisto` follows [Semantic Versioning 2.0.0](https://semver.org/):

- **Format**: `MAJOR.MINOR.PATCH`
  - `MAJOR`: Incompatible public API/ABI changes or non-migratable binary wire format modifications.
  - `MINOR`: Backward-compatible new capabilities, algorithms, feature flags, or wire format versions.
  - `PATCH`: Backward-compatible bug fixes, SIMD optimizations, documentation updates.

### Component Alignment
| Component | Primary Version Location | Notes |
| :--- | :--- | :--- |
| **Core C Library** | `include/histo/version.h`, `CMakeLists.txt` | Defines `HISTO_VERSION_*` macros and `SOVERSION` |
| **Python Package** | `bindings/python/pyproject.toml` | Dynamically references C `HISTO_VERSION_STRING` in `histo.__version__` |
| **Perl XS (`Math::Histo`)** | `bindings/perl/Math-Histo/lib/Math/Histo.pm` | CPAN `$VERSION` |
| **Perl Alien (`Alien::libhisto`)** | `bindings/perl/Alien-libhisto/lib/Alien/libhisto.pm` | Pinned core library release |

---

## 2. Release Tagging Convention

### Coordinated Full Releases
A full release where the core C library and all bindings are released together:
- **Git Tag**: `vX.Y.Z` (e.g. `v0.1.0`, `v0.2.0`, `v1.0.0`)
- **Pre-requisite**: All tests across all ecosystems must pass via `make test-all`.

### Out-of-Band Component Releases
When releasing independent patch updates to language bindings without a C library bump:
- **Perl Math::Histo**: `perl-math-histo-vX.Y.Z` (e.g. `perl-math-histo-v0.1.1`)
- **Perl Alien::libhisto**: `perl-alien-vX.Y.Z` (e.g. `perl-alien-v0.1.1`)
- **Python histo**: `python-histo-vX.Y.Z` (e.g. `python-histo-v0.1.1`)

---

## 3. Pre-Release Verification Checklist

Execute the full verification gate:
```bash
# Verify all versions match
python3 tools/scripts/bump_version.py --check

# Execute exhaustive test suites (Release, ASan, UBSan, TSan, Valgrind, Perl & Python dists, Docs)
make test-all
```

---

## 4. Release Execution Workflow

### Step 1: Bump Versions & Update Changelogs
```bash
# 1. Synchronize version across all files:
python3 tools/scripts/bump_version.py --set X.Y.Z

# 2. Update root CHANGELOG.md:
# Move entries from [Unreleased] to [X.Y.Z] - YYYY-MM-DD

# 3. Update CPAN Changes if releasing Perl bindings:
# bindings/perl/Math-Histo/Changes
# bindings/perl/Alien-libhisto/Changes
```

### Step 2: Commit and Tag
```bash
# 1. Commit release bump
git add include/histo/version.h CMakeLists.txt bindings/ CHANGELOG.md
git commit -m "chore(release): bump version to X.Y.Z"

# 2. Create annotated release tag
git tag -a vX.Y.Z -m "Release vX.Y.Z"
```

### Step 3: Build Distribution Artifacts
```bash
# Build Python hermetic sdist and wheel
make python-dist

# Build CPAN tarballs
make perl-dist
```
