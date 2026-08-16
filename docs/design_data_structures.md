# Design Specification: Core Data Structures & Memory Layout

This document specifies the internal memory layout, data structures, and type definitions for `libhisto`.

---

## 1. Public Handle & Type Abstractions

In the public header (`include/histo/types.h` and `include/histo/histo.h`), the histogram object is an opaque pointer:

```c
typedef struct histo histo_t;
```

This guarantees ABI encapsulation and allows internal layout optimization without breaking binary compatibility with client applications.

---

## 2. Enumerations & Configuration Flags

### 2.1 Binning Mode
```c
typedef enum histo_bin_type {
    HISTO_BIN_UNIFORM  = 0,  /* Fixed-width equidistant bins: O(1) direct arithmetic lookup */
    HISTO_BIN_VARIABLE = 1   /* Variable-width arbitrary bins: O(log N) binary search lookup */
} histo_bin_type_t;
```

### 2.2 Feature & Allocation Flags
Bitflags passed during histogram creation:
```c
typedef enum histo_flags {
    HISTO_FLAG_NONE          = 0,          /* Basic count/weight storage only */
    HISTO_FLAG_TRACK_SUMW2   = (1u << 0),  /* Track per-bin sum-of-weights squared (sum w^2) for uncertainty */
    HISTO_FLAG_EXACT_MOMENTS = (1u << 1)   /* Maintain online exact sample statistics (Welford's algorithm) */
} histo_flags_t;
```

---

## 3. Internal Histogram Structure (`struct histo`)

Located in `src/internal.h`:

```c
struct histo {
    /* --- Binning Geometry & Alignment --- */
    histo_bin_type_t bin_type;       /* 4 bytes: Uniform or Variable */
    uint32_t         flags;          /* 4 bytes: Configuration flags (HISTO_FLAG_*) */
    uint32_t         nbins;          /* 4 bytes: Number of in-range bins (N >= 1) */
    uint32_t         reserved_pad;   /* 4 bytes: Explicit padding for strict 8-byte alignment */

    double           min;            /* 8 bytes: Lower bound of bin 0 (finite) */
    double           max;            /* 8 bytes: Upper bound of bin N-1 (max > min, finite) */
    double           width;          /* 8 bytes: Range width: (max - min) */
    double           binsize;        /* 8 bytes: Width per bin: (width / nbins) for UNIFORM; 0.0 for VARIABLE */
    double           inv_binsize;    /* 8 bytes: Fast reciprocal (nbins / width) for 3-cycle FMUL lookup */
    double          *bin_edges;      /* 8 bytes: Pointer to (nbins + 1) strictly monotonic doubles for VARIABLE; NULL for UNIFORM */

    /* --- Binned Data Storage --- */
    double          *bins;           /* 8 bytes: Array of (nbins) doubles: accumulated bin weights */
    double          *sum_w2;         /* 8 bytes: Array of (nbins) doubles: accumulated sum of squared weights (NULL if disabled) */

    /* --- Out-of-Range & Exception Accumulators --- */
    double           underflow_weight;   /* 8 bytes: Accumulated weight for x < min */
    double           overflow_weight;    /* 8 bytes: Accumulated weight for x >= max */
    double           underflow_sum_w2;   /* 8 bytes: Accumulated sum(w^2) for underflow */
    double           overflow_sum_w2;    /* 8 bytes: Accumulated sum(w^2) for overflow */
    uint64_t         n_underflow;        /* 8 bytes: Fill count for underflow */
    uint64_t         n_overflow;         /* 8 bytes: Fill count for overflow */
    uint64_t         n_nan;              /* 8 bytes: Count of rejected NaN / non-finite samples */

    /* --- In-Range Global Aggregates --- */
    uint64_t         n_fills;            /* 8 bytes: Total in-range fill events */
    double           total_weight;       /* 8 bytes: Total in-range accumulated weight */
    double           total_sum_w2;       /* 8 bytes: Total in-range accumulated sum of squared weights */

    /* --- Exact Online Sample Statistics (Updated if HISTO_FLAG_EXACT_MOMENTS is set) --- */
    double           stats_min;          /* 8 bytes: Exact minimum sample value observed */
    double           stats_max;          /* 8 bytes: Exact maximum sample value observed */
    double           stats_mean;         /* 8 bytes: Online running mean (Welford) */
    double           stats_M2;           /* 8 bytes: Online running sum of squared differences (Welford) */
};
```

---

## 4. Memory Footprint & Alignment Strategy

### 4.1 Strict 8-Byte Alignment
All 64-bit integer (`uint64_t`), double-precision floating-point (`double`), and pointer types are aligned on 8-byte boundaries. The explicit `reserved_pad` field eliminates compiler-dependent padding holes across 32-bit and 64-bit platforms.

### 4.2 Fixed-Width (Uniform) Histograms
- Memory allocated:
  - Base `struct histo`: 160 bytes.
  - `bins`: $N \times 8$ bytes.
  - `sum_w2` *(optional)*: $N \times 8$ bytes.
- Total for 100 uniform bins (without `sum_w2`): $\approx 960$ bytes.
- Zero auxiliary coordinate allocations (`bin_edges == NULL`).

### 4.3 Variable-Width Histograms
- Additional memory:
  - `bin_edges`: $(N + 1) \times 8$ bytes.
- All coordinate values are checked for strict monotonicity during creation ($x_0 < x_1 < \dots < x_N$) and copied into library-owned memory.

### 4.4 Allocation Lifecycle & Teardown
- `histo_create_uniform(...)` and `histo_create_variable(...)` allocate the struct and buffers.
- `histo_destroy(histo_t *h)` cleanly releases `bins`, `sum_w2`, `bin_edges`, and the root struct `h`. Passing `NULL` is a safe no-op.

---

## 5. Structural Invariants

1. **Finite Range**: `min` and `max` must be finite numbers (`isfinite(min) && isfinite(max)`), with `max > min` and `isfinite(max - min)`.
2. **Valid Bin Count**: `nbins >= 1 && nbins <= HISTO_MAX_NBINS` (capped at $10^8$ to prevent 32-bit integer overflow).
3. **Strict Monotonicity**: For variable bins, `bin_edges[i] < bin_edges[i+1]` for all $0 \le i < N$.
4. **Const Correctness**: All read/query functions accept `const histo_t *h` with zero internal side effects or cached pointer mutations.
