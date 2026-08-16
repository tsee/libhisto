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
Bitflags passed during histogram creation to tailor memory overhead to the application's requirements:
```c
typedef enum histo_flags {
    HISTO_FLAG_NONE          = 0,       /* Basic count/weight storage only */
    HISTO_FLAG_TRACK_SUMW2   = 1 << 0,  /* Track per-bin sum-of-weights squared (sum w^2) for uncertainty */
    HISTO_FLAG_EXACT_MOMENTS = 1 << 1   /* Maintain online exact sample statistics (Welford's algorithm) */
} histo_flags_t;
```

---

## 3. Internal Histogram Structure (`struct histo`)

Located in `src/internal.h`:

```c
struct histo {
    /* --- Binning Geometry --- */
    histo_bin_type_t bin_type;   /* Uniform or Variable */
    uint32_t         flags;      /* Configuration flags (HISTO_FLAG_*) */
    uint32_t         nbins;      /* Number of in-range bins (N >= 1) */
    double           min;        /* Lower bound of bin 0 (finite) */
    double           max;        /* Upper bound of bin N-1 (max > min, finite) */
    double           width;      /* Range width: (max - min) */
    double           binsize;    /* Width per bin: (width / nbins) for UNIFORM; 0.0 for VARIABLE */
    double          *bin_edges;  /* Array of (nbins + 1) strictly monotonic doubles for VARIABLE; NULL for UNIFORM */

    /* --- Binned Data Storage --- */
    double          *bins;       /* Array of (nbins) doubles: accumulated bin weights */
    double          *sum_w2;     /* Array of (nbins) doubles: accumulated sum of squared weights (NULL if disabled) */

    /* --- Out-of-Range & Exception Accumulators --- */
    double           underflow_weight;   /* Accumulated weight for x < min */
    double           overflow_weight;    /* Accumulated weight for x >= max */
    double           underflow_sum_w2;   /* Accumulated sum(w^2) for underflow */
    double           overflow_sum_w2;    /* Accumulated sum(w^2) for overflow */
    uint64_t         n_underflow;        /* Fill count for underflow */
    uint64_t         n_overflow;         /* Fill count for overflow */
    uint64_t         n_nan;              /* Count of rejected NaN / invalid samples */

    /* --- In-Range Global Aggregates --- */
    uint64_t         n_fills;            /* Total in-range fill events */
    double           total_weight;       /* Total in-range accumulated weight */
    double           total_sum_w2;       /* Total in-range accumulated sum of squared weights */

    /* --- Exact Online Sample Statistics (Updated if HISTO_FLAG_EXACT_MOMENTS is set) --- */
    double           stats_min;          /* Exact minimum sample value observed */
    double           stats_max;          /* Exact maximum sample value observed */
    double           stats_mean;         /* Online running mean (Welford) */
    double           stats_M2;           /* Online running sum of squared differences (Welford) */
};
```

---

## 4. Memory Footprint & Allocation Strategy

### 4.1 Fixed-Width (Uniform) Histograms
- Memory allocated:
  - Base `struct histo`: ~128 bytes.
  - `bins`: $N \times 8$ bytes.
  - `sum_w2` *(optional)*: $N \times 8$ bytes.
- Total for 100 uniform bins (without `sum_w2`): $\approx 928$ bytes.
- Zero auxiliary coordinate allocations (`bin_edges == NULL`).

### 4.2 Variable-Width Histograms
- Additional memory:
  - `bin_edges`: $(N + 1) \times 8$ bytes.
- All coordinate values are checked for strict monotonicity during creation ($x_0 < x_1 < \dots < x_N$) and copied into library-owned memory.

### 4.3 Allocation Lifecycle & Teardown
- `histo_create_uniform(...)` and `histo_create_variable(...)` allocate the struct and buffers.
- `histo_destroy(histo_t *h)` cleanly releases `bins`, `sum_w2`, `bin_edges`, and the root struct `h`. Passing `NULL` is a safe no-op.

---

## 5. Structural Invariants

1. **Finite Range**: `min` and `max` must be finite numbers (`isfinite(min) && isfinite(max)`), with `max > min`.
2. **Valid Bin Count**: `nbins >= 1`.
3. **Strict Monotonicity**: For variable bins, `bin_edges[i] < bin_edges[i+1]` for all $0 \le i < N$.
4. **Const Correctness**: All read/query functions accept `const histo_t *h` with zero internal side effects or cached pointer mutations.
