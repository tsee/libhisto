# Design Specification: Statistical & Analytical Formulae

This document specifies the mathematical formulations and algorithms used for summary statistics, quantiles, and uncertainty propagation in `libhisto`.

---

## 1. Online Exact Sample Moments (Weighted Welford Algorithm)

When `HISTO_FLAG_EXACT_MOMENTS` is enabled, sample moments are updated continuously during each `fill` operation without binning discretization error (assuming non-negative weights $w_k \ge 0$).

For a stream of samples $(x_k, w_k)$ with cumulative weight $W_k = \sum_{j=1}^k w_j$:

$$\delta = x_k - \mu_{k-1}$$
$$\mu_k = \mu_{k-1} + \frac{w_k}{W_k} \delta$$
$$M_{2, k} = M_{2, k-1} + w_k \cdot \delta \cdot (x_k - \mu_k)$$

### 1.1 Numerical Floor Protection
Due to floating-point rounding when processing identical values, $M_2$ can become slightly negative (e.g. $-10^{-17}$). `libhisto` clamps $M_2$:
$$M_2 = \max(0.0, M_2)$$

### 1.2 Variance Formulations
- **Exact Population Variance**:
  $$\sigma^2 = \frac{M_{2, k}}{W_k}$$
- **Exact Unweighted Sample Variance** ($w_j = 1, N > 1$):
  $$s^2 = \frac{M_{2, k}}{N - 1}$$
- **Exact Weighted Sample Variance (Reliability / Effective Sample Size)**:
  $$s^2 = \frac{W_k}{W_k^2 - \sum w_i^2} M_{2, k}$$
- **Exact Minimum / Maximum**:
  $$x_{\min} \leftarrow \min(x_{\min}, x_k), \quad x_{\max} \leftarrow \max(x_{\max}, x_k)$$

---

## 2. Bin-Approximated Moments

When exact online moments are disabled, moments are estimated from the binned distribution using bin centers $x_{c, i}$:

### 2.1 Mean
$$\mu_{\text{bin}} = \frac{1}{W_{\text{total}}} \sum_{i=0}^{N-1} w_i \cdot x_{c, i}$$

### 2.2 Variance & Standard Deviation
To prevent catastrophic cancellation, a two-pass algorithm over bin centers is used:
$$\sigma^2_{\text{bin}} = \frac{1}{W_{\text{total}}} \sum_{i=0}^{N-1} w_i \cdot (x_{c, i} - \mu_{\text{bin}})^2$$
$$\sigma_{\text{bin}} = \sqrt{\sigma^2_{\text{bin}}}$$

---

## 3. Quantile & Median Estimation (Continuous Non-Empty Support Interpolation)

For a target percentile $p \in [0.0, 1.0]$ ($p = 0.5$ for median):

1. **Validation & Edge Cases**:
   - If total in-range weight $W_{\text{total}} \le 0$, return `HISTO_ERR_EMPTY`.
   - If $p < 0.0$ or $p > 1.0$, return `HISTO_ERR_OUT_OF_RANGE`.
2. **Boundary Target Support**:
   - Let $k_{\text{first}}$ be the index of the first bin with $w_i > 0$, and $k_{\text{last}}$ be the last bin with $w_i > 0$.
   - If $p == 0.0$: return $x_{\text{lower}, k_{\text{first}}}$ (or `stats_min` if exact moments enabled).
   - If $p == 1.0$: return $x_{\text{upper}, k_{\text{last}}}$ (or `stats_max` if exact moments enabled).
3. **Cumulative Bracketing on Non-Empty Bins**:
   - Target cumulative weight: $T = p \cdot W_{\text{total}}$.
   - Locate the unique non-empty bin $k$ such that:
     $$C_{k-1} = \sum_{i=0}^{k-1} w_i < T \le C_k = \sum_{i=0}^k w_i$$
4. **Linear Intra-Bin Interpolation**:
   $$\theta = \frac{T - C_{k-1}}{w_k} \in (0, 1]$$
   $$Q(p) = x_{\text{lower}, k} + \theta \cdot (x_{\text{upper}, k} - x_{\text{lower}, k})$$

This formulation eliminates artificial plateau jumps and never evaluates percentiles inside empty boundary bins.

---

## 4. Bin Uncertainties & Error Propagation

### 4.1 Bin Variance & Standard Error
- **Unweighted Fills ($w_j = 1$)**:
  $$\sigma_i = \sqrt{N_i}$$
- **Weighted Fills ($w_j \ne 1$, with `HISTO_FLAG_TRACK_SUMW2`)**:
  $$\sigma_i = \sqrt{\sum_{j \in \text{bin } i} w_j^2}$$

### 4.2 Robust Error Propagation in Arithmetic Operations
All operations verify identical binning between operands.

- **Addition ($H_3 = H_1 + H_2$) & Subtraction ($H_3 = H_1 - H_2$)**:
  $$\text{sum\_w2}_{3, i} = \text{sum\_w2}_{1, i} + \text{sum\_w2}_{2, i}$$

- **Scalar Scaling ($H_3 = c \cdot H_1$)**:
  $$\text{sum\_w2}_{3, i} = c^2 \cdot \text{sum\_w2}_{1, i}$$

- **Multiplication ($H_3 = H_1 \cdot H_2$)**:
  Using the algebraically expanded, **division-free** formula:
  $$\mathbf{\text{sum\_w2}_{3, i} = H_{2, i}^2 \cdot \text{sum\_w2}_{1, i} + H_{1, i}^2 \cdot \text{sum\_w2}_{2, i}}$$
  *(Guaranteed division-free: evaluates cleanly to $0.0$ if either bin is empty, eliminating NaN singularities).*

- **Division ($H_3 = H_1 / H_2$)**:
  $$\mathbf{\text{sum\_w2}_{3, i} = \frac{\text{sum\_w2}_{1, i} \cdot H_{2, i}^2 + \text{sum\_w2}_{2, i} \cdot H_{1, i}^2}{H_{2, i}^4}}$$
  - If $H_{2, i} == 0$, `histo_divide` sets $H_{3, i} = 0.0$ and $\text{sum\_w2}_{3, i} = 0.0$ (or returns `HISTO_ERR_DIV_BY_ZERO` if strict mode is configured).
