# Design Specification: Serialization & Wire Formats

This document defines the canonical binary wire format and JSON schema for `libhisto`.

---

## 1. Canonical Binary Wire Format (`libhisto` Binary v1)

All multi-byte numeric fields are encoded in **Canonical Little-Endian** byte order. Machines running in Big-Endian mode perform byte-swapping during serialization and deserialization.

### 1.1 Header Structure (64 Bytes Fixed)

| Offset (Bytes) | Field Name | Data Type | Description |
| :--- | :--- | :--- | :--- |
| `0x00 - 0x03` | `magic` | `uint8_t[4]` | ASCII `"HIST"` (`0x48, 0x49, 0x53, 0x54`) |
| `0x04 - 0x05` | `version` | `uint16_t` | File format version (`0x0001`) |
| `0x06 - 0x07` | `header_size` | `uint16_t` | Header byte length (`64`) |
| `0x08 - 0x0B` | `flags` | `uint32_t` | Bitflags: `0x01`=Variable Bins, `0x02`=Has $\sum w^2$, `0x04`=Has Exact Moments |
| `0x0C - 0x0F` | `nbins` | `uint32_t` | Number of in-range bins ($N$) |
| `0x10 - 0x17` | `min` | `double` (LE) | Lower boundary coordinate ($x_{\min}$) |
| `0x18 - 0x1F` | `max` | `double` (LE) | Upper boundary coordinate ($x_{\max}$) |
| `0x20 - 0x27` | `total_weight`| `double` (LE) | Total in-range accumulated weight |
| `0x28 - 0x2F` | `n_fills` | `uint64_t` (LE) | Total in-range fill operations |
| `0x30 - 0x37` | `underflow_w` | `double` (LE) | Underflow weight |
| `0x38 - 0x3F` | `overflow_w` | `double` (LE) | Overflow weight |

### 1.2 Extended Metadata Block (64 Bytes Fixed)

| Offset (Bytes) | Field Name | Data Type | Description |
| :--- | :--- | :--- | :--- |
| `0x40 - 0x47` | `n_underflow` | `uint64_t` (LE) | Underflow entry count |
| `0x48 - 0x4F` | `n_overflow` | `uint64_t` (LE) | Overflow entry count |
| `0x50 - 0x57` | `n_nan` | `uint64_t` (LE) | Rejected NaN / non-finite count |
| `0x58 - 0x5F` | `total_sum_w2`| `double` (LE) | In-range total $\sum w^2$ |
| `0x60 - 0x67` | `underflow_sum_w2` | `double` (LE) | Underflow $\sum w^2$ |
| `0x68 - 0x6F` | `overflow_sum_w2` | `double` (LE) | Overflow $\sum w^2$ |
| `0x70 - 0x77` | `stats_mean` | `double` (LE) | Exact online running mean |
| `0x78 - 0x7F` | `stats_M2` | `double` (LE) | Exact online $M_2$ (variance numerator) |

### 1.3 Payload Arrays (Contiguous doubles, Little-Endian)

1. **`bin_edges`** *(Present only if `flags & 0x01`)*:
   - $(N + 1) \times 8$ bytes: Array of doubles representing monotonic boundaries.
2. **`bins`** *(Always present)*:
   - $N \times 8$ bytes: Array of doubles representing accumulated bin weights.
3. **`sum_w2`** *(Present only if `flags & 0x02`)*:
   - $N \times 8$ bytes: Array of doubles representing accumulated sum-of-weights squared.

---

## 2. JSON Wire Format

Human-readable, portable schema:

```json
{
  "schema": "libhisto-v1",
  "bin_type": "uniform",
  "nbins": 5,
  "min": 0.0,
  "max": 10.0,
  "flags": 1,
  "underflow": { "weight": 0.0, "sum_w2": 0.0, "entries": 0 },
  "overflow": { "weight": 0.0, "sum_w2": 0.0, "entries": 0 },
  "nan_count": 0,
  "total": { "weight": 100.0, "sum_w2": 100.0, "entries": 100 },
  "bins": [ 10.0, 25.0, 35.0, 20.0, 10.0 ],
  "sum_w2": [ 10.0, 25.0, 35.0, 20.0, 10.0 ]
}
```

For variable binning:
```json
{
  "schema": "libhisto-v1",
  "bin_type": "variable",
  "nbins": 3,
  "bin_edges": [ 0.0, 2.5, 7.5, 10.0 ],
  "bins": [ 30.0, 50.0, 20.0 ]
}
```
