# Design Specification: Statistical & Analytical Formulae

This document specifies the mathematical formulations and algorithms used for summary statistics, quantiles, and uncertainty propagation in `libhisto`.

---

## 1. Online Exact Sample Moments (Weighted Welford Algorithm)

When `HISTO_FLAG_EXACT_MOMENTS` is enabled, sample moments are updated continuously during each `fill` operation without binning discretization error.

For a stream of samples $(x_k, w_k)$ with cumulative weight $W_k = \sum_{j=1}^k w_j$:

$$\delta = x_k - \mu_{k-1}$$
$$\mu_k = \mu_{k-1} + \frac{w_k}{W_k} \delta$$
$$M_{2, k} = M_{2, k-1} + w_k \cdot \delta \cdot (x_k - \mu_k)$$

- **Exact Mean**: $\mu = \mu_k$
- **Exact Population Variance**: $\sigma^2 = \frac{M_{2, k}}{W_k}$
- **Exact Sample Variance (Unweighted)**: $s^2 = \frac{M_{2, k}}{N - 1}$
- **Exact Minimum / Maximum**: Updated via $x_{\min} \leftarrow \min(x_{\min}, x_k)$ and $x_{\max} \leftarrow \max(x_{\max}, x_k)$.

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

## 3. Quantile & Median Estimation (Linear CDF Interpolation)

For a target percentile $p \in [0.0, 1.0]$ ($p = 0.5$ for median):

1. Compute target cumulative weight:
   $$T = p \cdot W_{\text{total}}$$
2. Find bin index $k$ such that the running sum up to bin $k$ brackets $T$:
   $$C_{k-1} = \sum_{i=0}^{k-1} w_i \le T \le C_k = \sum_{i=0}^k w_i$$
3. Compute linear interpolation fraction within bin $k$:
   $$\theta = \begin{cases} 0.5 & \text{if } w_k = 0 \\ \frac{T - C_{k-1}}{w_k} & \text{if } w_k > 0 \end{cases}$$
4. Interpolated Quantile:
   $$Q(p) = x_{\text{lower}, k} + \theta \cdot (x_{\text{upper}, k} - x_{\text{lower}, k})$$

---

## 4. Bin Uncertainties & Error Propagation

### 4.1 Bin Variance & Standard Error
- **Unweighted Fills ($w_j = 1$)**:
  $$\sigma_i = \sqrt{N_i}$$
- **Weighted Fills ($w_j \ne 1$, with `HISTO_FLAG_TRACK_SUMW2`)**:
  $$\sigma_i = \sqrt{\sum_{j \in \text{bin } i} w_j^2}$$

### 4.2 Error Propagation in Arithmetic Operations
- **Addition ($H_3 = H_1 + H_2$) & Subtraction ($H_3 = H_1 - H_2$)**:
  $$\text{sum\_w2}_{3, i} = \text{sum\_w2}_{1, i} + \text{sum\_w2}_{2, i}$$
- **Scalar Scaling ($H_3 = c \cdot H_1$)**:
  $$\text{sum\_w2}_{3, i} = c^2 \cdot \text{sum\_w2}_{1, i}$$
- **Multiplication ($H_3 = H_1 \cdot H_2$)**:
  $$\left(\frac{\sigma_3}{H_3}\right)^2 = \left(\frac{\sigma_1}{H_1}\right)^2 + \left(\frac{\sigma_2}{H_2}\right)^2 \implies \text{sum\_w2}_{3, i} = H_{3, i}^2 \left( \frac{\text{sum\_w2}_{1, i}}{H_{1, i}^2} + \frac{\text{sum\_w2}_{2, i}}{H_{2, i}^2} \right)$$
- **Division ($H_3 = H_1 / H_2$)**:
  $$\text{sum\_w2}_{3, i} = \left(\frac{H_{1, i}}{H_{2, i}}\right)^2 \left( \frac{\text{sum\_w2}_{1, i}}{H_{1, i}^2} + \frac{\text{sum\_w2}_{2, i}}{H_{2, i}^2} \right)$$
