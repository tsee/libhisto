# Design Specification: Serialization & Wire Formats

This document defines the canonical binary wire format, version migration mechanics, and JSON schema for `libhisto`.

---

## 1. Canonical Binary Wire Format (`libhisto` Binary v1 & v2)

All multi-byte numeric fields in `libhisto` binary blobs are encoded in **Canonical Little-Endian** byte order. Platforms operating in Big-Endian mode perform byte-swapping during both serialization and deserialization to ensure seamless cross-architecture data exchange.

The current library serialization format is **Version 2**.

### 1.1 Header Structure (256 Bytes Fixed, 8-Byte Aligned)

| Offset (Bytes) | Field Name | Data Type | Description |
| :--- | :--- | :--- | :--- |
| `0x00 - 0x07` | `magic` | `uint8_t[8]` | Unambiguous signature: `\x89 L H I S T \n \0` (`0x89, 0x4C, 0x48, 0x49, 0x53, 0x54, 0x0A, 0x00`) |
| `0x08 - 0x09` | `version` | `uint16_t` | File format version (`0x0001` for v1, `0x0002` for v2) |
| `0x0A - 0x0B` | `header_size` | `uint16_t` | Header byte length (`256` / `0x0100`) |
| `0x0C - 0x0F` | `flags` | `uint32_t` | Bitflags: `0x01`=Variable Bins, `0x02`=Has $\sum w^2$, `0x04`=Has Exact Moments |
| `0x10 - 0x13` | `nbins` | `uint32_t` | Number of in-range bins ($N \ge 1$) |
| `0x14 - 0x17` | `reserved_pad`| `uint32_t` | Reserved padding (`0x00000000`) for strict 8-byte alignment |
| `0x18 - 0x1F` | `min` | `double` (LE) | Lower boundary coordinate ($x_{\min}$) |
| `0x20 - 0x27` | `max` | `double` (LE) | Upper boundary coordinate ($x_{\max}$) |
| `0x28 - 0x2F` | `total_weight`| `double` (LE) | Total in-range accumulated weight |
| `0x30 - 0x37` | `total_sum_w2`| `double` (LE) | In-range total $\sum w^2$ |
| `0x38 - 0x3F` | `n_fills` | `uint64_t` (LE) | Total in-range fill operations count |
| `0x40 - 0x47` | `underflow_w` | `double` (LE) | Accumulated underflow weight ($x < x_{\min}$) |
| `0x48 - 0x4F` | `overflow_w` | `double` (LE) | Accumulated overflow weight ($x \ge x_{\max}$) |
| `0x50 - 0x57` | `underflow_w2`| `double` (LE) | Accumulated underflow $\sum w^2$ |
| `0x58 - 0x5F` | `overflow_w2` | `double` (LE) | Accumulated overflow $\sum w^2$ |
| `0x60 - 0x67` | `n_underflow` | `uint64_t` (LE) | Underflow sample count |
| `0x68 - 0x6F` | `n_overflow` | `uint64_t` (LE) | Overflow sample count |
| `0x70 - 0x77` | `n_nan` | `uint64_t` (LE) | Rejected non-finite / NaN sample count |
| `0x78 - 0x7F` | `stats_mean` | `double` (LE) | Exact online running mean |
| `0x80 - 0x87` | `stats_M2` | `double` (LE) | Exact online sum of squared differences |
| `0x88 - 0x8F` | `stats_min` | `double` (LE) | Observed minimum sample coordinate |
| `0x90 - 0x97` | `stats_max` | `double` (LE) | Observed maximum sample coordinate |
| `0x98 - 0xFF` | `reserved_ext`| `uint8_t[104]`| Reserved extension area (zero-padded). In v2, reserved for future extensions. |

### 1.2 Payload Arrays (Contiguous doubles, Little-Endian)

Payloads begin immediately at byte offset `header_size` (`0x0100` / 256):

1. **`bin_edges`** *(Present only if `flags & 0x01`)*:
   - $(N + 1) \times 8$ bytes: Contiguous IEEE-754 doubles representing monotonic bin boundaries.
2. **`bins`** *(Always present)*:
   - $N \times 8$ bytes: Contiguous IEEE-754 doubles representing accumulated bin weights.
3. **`sum_w2`** *(Present only if `flags & 0x02`)*:
   - $N \times 8$ bytes: Contiguous IEEE-754 doubles representing accumulated sum-of-weights squared.

### 1.3 Buffer Sizing & Deserializer Safety
- Total required binary buffer size:
  $$\text{Size} = 256 + N \times 8 + (\text{is\_var} ? (N+1) \times 8 : 0) + (\text{has\_sumw2} ? N \times 8 : 0)$$
- **Integer Overflow Protection**: Before buffer allocation or reading, `libhisto` verifies `nbins <= HISTO_MAX_NBINS` and performs multiplication checks against `SIZE_MAX`.

### 1.4 Version Migration and Backward Compatibility

`libhisto` automatically migrates older format versions when deserializing. The migration logic implements a chainable pipeline ($V_i \to V_{i+1} \dots \to V_{\text{current}}$).

- **V1 to V2 Migration**: A v1 blob is structurally upgraded to v2 by setting the version field to 2 and explicitly zero-padding the `reserved_ext` area (`0x98 - 0xFF`).
- Explicit migration is available via the public API:
  ```c
  histo_status_t histo_migrate_binary(const void *in_buf, size_t in_size, 
                                      void **out_buf, size_t *out_size);
  ```

---

## 2. JSON Wire Format

Human-readable, portable schema formatted with `%.17g` precision for lossless 53-bit IEEE-754 roundtripping:

```json
{
  "schema": "libhisto-v1",
  "bin_type": "uniform",
  "nbins": 5,
  "min": 0.0,
  "max": 10.0,
  "flags": 3,
  "underflow": { "weight": 0.0, "sum_w2": 0.0, "entries": 0 },
  "overflow": { "weight": 0.0, "sum_w2": 0.0, "entries": 0 },
  "nan_count": 0,
  "total": { "weight": 100.0, "sum_w2": 100.0, "entries": 100 },
  "exact_stats": {
    "mean": 5.042,
    "M2": 12.345,
    "min": 0.12,
    "max": 9.87
  },
  "bins": [ 10.0, 25.0, 35.0, 20.0, 10.0 ],
  "sum_w2": [ 10.0, 25.0, 35.0, 20.0, 10.0 ]
}
```
