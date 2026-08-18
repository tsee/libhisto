# Design Specification: Serialization & Wire Formats

This document defines the canonical binary wire format, version migration mechanics, and JSON schema for `libhisto`.

---

## 1. Canonical Binary Wire Format (`libhisto` Binary v1 & v2)

All multi-byte numeric fields in `libhisto` binary blobs are encoded in **Canonical Little-Endian** byte order. Platforms operating in Big-Endian mode perform byte-swapping during both serialization and deserialization to ensure seamless cross-architecture data exchange.

The current library serialization formats are **Version 2** (for 1D histograms) and **Version 3** (for 2D histograms).

### 1.1 Header Structure for 1D Histograms (Version 1 & 2 - 256 Bytes Fixed)

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

### 1.2 Payload Arrays for 1D Histograms
Payloads begin at byte offset `header_size` (`0x0100` / 256):
1. **`bin_edges`** *(Present only if `flags & 0x01`)*: $(N + 1) \times 8$ bytes (monotonic doubles).
2. **`bins`** *(Always present)*: $N \times 8$ bytes (accumulated bin weights).
3. **`sum_w2`** *(Present only if `flags & 0x02`)*: $N \times 8$ bytes (accumulated sum-of-weights squared).

---

## 2. Canonical Binary Wire Format: 2D Histograms (Format V3)

Version 3 introduces bivariate 2-dimensional histograms (`histo2d_t`) with support for decoupled axes and 9-region guard partitioning.

### 2.1 Header Structure for 2D Histograms (Version 3 - 256 Bytes Fixed)

| Offset (Bytes) | Field Name | Data Type | Description |
| :--- | :--- | :--- | :--- |
| `0x00 - 0x07` | `magic` | `uint8_t[8]` | Unambiguous signature: `\x89 L H I S T \n \0` (`0x89, 0x4C, 0x48, 0x49, 0x53, 0x54, 0x0A, 0x00`) |
| `0x08 - 0x09` | `version` | `uint16_t` | File format version (`0x0003`) |
| `0x0A - 0x0B` | `header_size` | `uint16_t` | Header byte length (`256` / `0x0100`) |
| `0x0C - 0x0F` | `flags` | `uint32_t` | Bitflags: `0x02`=Track SumW2, `0x04`=Exact Moments |
| `0x10 - 0x13` | `nx` | `uint32_t` | Number of bins along X ($N_x \ge 1$) |
| `0x14 - 0x17` | `ny` | `uint32_t` | Number of bins along Y ($N_y \ge 1$) |
| `0x18 - 0x1B` | `x_bin_type` | `uint32_t` | `0` = Uniform X, `1` = Variable X |
| `0x1C - 0x1F` | `y_bin_type` | `uint32_t` | `0` = Uniform Y, `1` = Variable Y |
| `0x20 - 0x27` | `xmin` | `double` (LE) | Lower boundary coordinate along X ($x_{\min}$) |
| `0x28 - 0x2F` | `xmax` | `double` (LE) | Upper boundary coordinate along X ($x_{\max}$) |
| `0x30 - 0x37` | `ymin` | `double` (LE) | Lower boundary coordinate along Y ($y_{\min}$) |
| `0x38 - 0x3F` | `ymax` | `double` (LE) | Upper boundary coordinate along Y ($y_{\max}$) |
| `0x40 - 0x47` | `total_weight`| `double` (LE) | Total in-range accumulated weight |
| `0x48 - 0x4F` | `total_sum_w2`| `double` (LE) | In-range total $\sum w^2$ |
| `0x50 - 0x57` | `n_fills` | `uint64_t` (LE) | Total in-range fill operations count |
| `0x58 - 0x5F` | `n_nan` | `uint64_t` (LE) | Rejected non-finite / NaN sample count |
| `0x60 - 0x67` | `stats_mean_x`| `double` (LE) | Exact online running mean X |
| `0x68 - 0x6F` | `stats_mean_y`| `double` (LE) | Exact online running mean Y |
| `0x70 - 0x77` | `stats_M2_xx` | `double` (LE) | Exact online sum of squared differences $M_{2,xx}$ |
| `0x78 - 0x7F` | `stats_M2_yy` | `double` (LE) | Exact online sum of squared differences $M_{2,yy}$ |
| `0x80 - 0x87` | `stats_M2_xy` | `double` (LE) | Exact online comoment $M_{2,xy}$ |
| `0x88 - 0x8F` | `stats_min_x` | `double` (LE) | Observed minimum X coordinate |
| `0x90 - 0x97` | `stats_max_x` | `double` (LE) | Observed maximum X coordinate |
| `0x98 - 0x9F` | `stats_min_y` | `double` (LE) | Observed minimum Y coordinate |
| `0xA0 - 0xA7` | `stats_max_y` | `double` (LE) | Observed maximum Y coordinate |
| `0xA8 - 0xFF` | `reserved_ext`| `uint8_t[88]` | Reserved extension area (zero-padded) |

### 2.2 Payload Arrays for 2D Histograms (Version 3)

Payloads begin immediately at offset `0x0100` (256):

1. **`x_edges`** *(Present only if `x_bin_type == 1`)*:
   - $(N_x + 1) \times 8$ bytes: Contiguous IEEE-754 Little-Endian doubles representing monotonic X boundaries.
2. **`y_edges`** *(Present only if `y_bin_type == 1`)*:
   - $(N_y + 1) \times 8$ bytes: Contiguous IEEE-754 Little-Endian doubles representing monotonic Y boundaries.
3. **`bins`** *(Always present)*:
   - $(N_x \times N_y) \times 8$ bytes: Row-major contiguous doubles representing cell weights ($i_x \cdot N_y + i_y$).
4. **`sum_w2`** *(Present only if `flags & 0x02`)*:
   - $(N_x \times N_y) \times 8$ bytes: Row-major contiguous doubles representing sum of weights squared.
5. **`guard_weights`** *(Always present)*:
   - $9 \times 8$ bytes: Weights accumulated in each of the 9 geometric regions (`HISTO2D_REGION_*`).
6. **`guard_sum_w2`** *(Present only if `flags & 0x02`)*:
   - $9 \times 8$ bytes: Sum of weights squared for each of the 9 regions.
7. **`guard_counts`** *(Always present)*:
   - $9 \times 8$ bytes: `uint64_t` fill counts for each of the 9 regions.

### 2.3 2D Buffer Sizing Calculation
$$\begin{aligned}
\text{Size}_{\text{v3}} = 256 &+ (\text{is\_var\_x} ? (N_x + 1) \times 8 : 0) \\
&+ (\text{is\_var\_y} ? (N_y + 1) \times 8 : 0) \\
&+ (N_x \times N_y) \times 8 \\
&+ (\text{has\_sumw2} ? (N_x \times N_y) \times 8 : 0) \\
&+ 9 \times 8 + (\text{has\_sumw2} ? 9 \times 8 : 0) + 9 \times 8
\end{aligned}$$

---

## 3. JSON Wire Formats

### 3.1 1D Histogram JSON Schema (`libhisto-v1`)

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

### 3.2 2D Histogram JSON Schema (`libhisto2d-v1`)

```json
{
  "schema": "libhisto2d-v1",
  "x_axis": {
    "bin_type": "uniform",
    "nbins": 4,
    "min": 0.0,
    "max": 4.0
  },
  "y_axis": {
    "bin_type": "uniform",
    "nbins": 3,
    "min": 0.0,
    "max": 3.0
  },
  "flags": 3,
  "guard_regions": {
    "center": { "weight": 150.0, "sum_w2": 150.0, "entries": 150 },
    "north":  { "weight": 2.0, "sum_w2": 2.0, "entries": 2 },
    "south":  { "weight": 1.0, "sum_w2": 1.0, "entries": 1 },
    "east":   { "weight": 3.0, "sum_w2": 3.0, "entries": 3 },
    "west":   { "weight": 0.0, "sum_w2": 0.0, "entries": 0 },
    "north_west": { "weight": 0.0, "sum_w2": 0.0, "entries": 0 },
    "north_east": { "weight": 1.0, "sum_w2": 1.0, "entries": 1 },
    "south_west": { "weight": 0.0, "sum_w2": 0.0, "entries": 0 },
    "south_east": { "weight": 0.0, "sum_w2": 0.0, "entries": 0 }
  },
  "nan_count": 0,
  "exact_stats": {
    "mean_x": 2.15,
    "mean_y": 1.48,
    "var_x": 1.05,
    "var_y": 0.68,
    "covariance": 0.42,
    "correlation": 0.497
  },
  "bins_row_major": [
    [10.0, 12.0, 15.0],
    [14.0, 18.0, 20.0],
    [11.0, 16.0, 14.0],
    [8.0,  7.0,  5.0]
  ]
}
```

