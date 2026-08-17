# Design Specification: Statistical & Analytical Formulae

This document specifies the mathematical formulations, statistical estimators, and error-propagation algorithms implemented in `libhisto`.

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

## 3. Higher-Order Central Moments & Shape Measures

For higher-order moments, `libhisto` evaluates the central moments around the distribution mean $\mu$:

### 3.1 k-th Central Moment ($M_k$)
$$M_k = \frac{1}{W_{\text{total}}} \sum_{i=0}^{N-1} w_i \cdot (x_{c, i} - \mu)^k$$
- For $k = 0$: $M_0 = 1.0$.
- For $k = 1$: $M_1 = 0.0$.
- For $k = 2$: $M_2 = \sigma^2$ (variance).

### 3.2 Skewness ($\gamma_1$)
Measures the asymmetry of the probability distribution about its mean:
$$\gamma_1 = \frac{M_3}{M_2^{3/2}} = \frac{M_3}{\sigma^3}$$
- $\gamma_1 > 0$: Right-skewed distribution (tail extends right).
- $\gamma_1 < 0$: Left-skewed distribution (tail extends left).
- $\gamma_1 = 0$: Symmetric distribution.
- If $\sigma^2 = 0.0$, returns `HISTO_ERR_DIV_BY_ZERO`.

### 3.3 Kurtosis ($\beta_2$) & Excess Kurtosis ($\gamma_2$)
Measures the tailedness and peak sharpness of the distribution:
$$\beta_2 = \frac{M_4}{M_2^2} = \frac{M_4}{\sigma^4}$$
$$\gamma_2 = \beta_2 - 3.0$$
- $\gamma_2 = 0$: Mesokurtic (identical tail weight to a standard Gaussian distribution).
- $\gamma_2 > 0$: Leptokurtic (heavier tails and sharper central peak than Gaussian).
- $\gamma_2 < 0$: Platykurtic (lighter tails and flatter peak than Gaussian).
- If $\sigma^2 = 0.0$, returns `HISTO_ERR_DIV_BY_ZERO`.

---

## 4. Mode Estimation, Peak Characterization & RMS

### 4.1 Discrete Mode Bin
The mode bin index $m$ satisfies:
$$m = \arg\max_{0 \le i < N} w_i$$
If multiple bins share the maximum weight, the lowest index is chosen deterministically.

### 4.2 Continuous Mode (3-Point Parabolic Peak Interpolation)
For uniform binning, when the mode bin is an interior bin ($0 < m < N-1$), sub-bin peak localization is computed via parabolic interpolation through the three adjacent bin contents $(w_{m-1}, w_m, w_{m+1})$:

$$\text{denom} = 2 w_m - w_{m-1} - w_{m+1}$$
If $\text{denom} > 0$:
$$\delta = \frac{1}{2} \frac{w_{m+1} - w_{m-1}}{\text{denom}}, \quad \delta \in [-0.5, 0.5]$$
$$x_{\text{mode}} = x_{c, m} + \delta \cdot \Delta$$
For boundary bins ($m=0$ or $m=N-1$), variable binning, or non-convex peaks ($\text{denom} \le 0$), the peak coordinate falls back to $x_{c, m}$.

### 4.3 Full Width at Half Maximum (FWHM)
The FWHM measures the width of the dominant distribution peak at half of its maximum weight:
$$y_{\text{half}} = \frac{w_m}{2}$$
- **Left Flank Crossing ($x_{\text{left}}$)**: Scans downward from $m$ to find the crossing bin $i$ where $w_i < y_{\text{half}}$, linearly interpolating between bin centers $x_{c, i}$ and $x_{c, i+1}$. If no crossing is found, falls back to the histogram lower limit $x_{\min}$.
- **Right Flank Crossing ($x_{\text{right}}$)**: Scans upward from $m$ to find the crossing bin $i$ where $w_i < y_{\text{half}}$, linearly interpolating between $x_{c, i-1}$ and $x_{c, i}$. If no crossing is found, falls back to the histogram upper limit $x_{\max}$.
$$\text{FWHM} = \max(0.0, x_{\text{right}} - x_{\text{left}})$$

### 4.4 Root Mean Square (RMS)
The quadratic mean of the distribution:
$$\text{RMS} = \sqrt{\frac{1}{W_{\text{total}}} \sum_{i=0}^{N-1} w_i \cdot x_{c, i}^2} = \sqrt{\sigma^2 + \mu^2}$$

---

## 5. Quantile & Median Estimation

For a cumulative probability $p \in [0.0, 1.0]$ ($p = 0.5$ for median):

1. **Validation & Edge Cases**:
   - If total in-range weight $W_{\text{total}} \le 0$, return `HISTO_ERR_EMPTY`.
   - If $p < 0.0$ or $p > 1.0$, return `HISTO_ERR_OUT_OF_RANGE`.
2. **Boundary Target Support**:
   - Let $k_{\text{first}}$ be the index of the first bin with $w_i > 0$, and $k_{\text{last}}$ be the last bin with $w_i > 0$.
   - If $p == 0.0$: return $x_{\text{lower}, k_{\text{first}}}$ (or $x_{\min}^{\text{exact}}$ if exact moments enabled).
   - If $p == 1.0$: return $x_{\text{upper}, k_{\text{last}}}$ (or $x_{\max}^{\text{exact}}$ if exact moments enabled).
3. **Cumulative Bracketing on Non-Empty Bins**:
   - Target cumulative weight: $T = p \cdot W_{\text{total}}$.
   - Locate the unique non-empty bin $k$ such that:
     $$C_{k-1} = \sum_{i=0}^{k-1} w_i < T \le C_k = \sum_{i=0}^k w_i$$
4. **Linear Intra-Bin Interpolation**:
   $$\theta = \frac{T - C_{k-1}}{w_k} \in (0, 1]$$
   $$Q(p) = x_{\text{lower}, k} + \theta \cdot (x_{\text{upper}, k} - x_{\text{lower}, k})$$

---

## 6. Robust Dispersion & Resistant Estimators

### 6.1 Interquartile Range (IQR)
$$\text{IQR} = Q(0.75) - Q(0.25)$$

### 6.2 Median Absolute Deviation (MAD)
Measures statistical dispersion resistant to outliers:
$$\text{MAD} = \text{median}(|x - \text{median}(x)|)$$
`libhisto` computes deviations $d_i = |x_{c, i} - \text{med}|$ for all non-empty bins, sorts the $(d_i, w_i)$ pairs in $O(N \log N)$ time, and evaluates the weighted 50th percentile of absolute deviations.

### 6.3 Trimmed Mean
Calculates the mean after discarding tails below lower quantile $p_1$ and above upper quantile $p_2$ ($0 \le p_1 < p_2 \le 1$):
$$\mu_{\text{trimmed}} = \frac{1}{T_2 - T_1} \sum_{i} w_{\text{eff}, i} \cdot x_{c, i}$$
where $T_1 = p_1 W_{\text{total}}$, $T_2 = p_2 W_{\text{total}}$, and $w_{\text{eff}, i}$ represents the exact partial overlap of bin interval $[C_{i-1}, C_i]$ with $[T_1, T_2]$.

### 6.4 Winsorized Mean
Replaces tail samples outside $[Q(p_1), Q(p_2)]$ with the threshold values $x_{\text{low}} = Q(p_1)$ and $x_{\text{high}} = Q(p_2)$:
$$\tilde{x}_i = \text{clamp}(x_{c, i}, x_{\text{low}}, x_{\text{high}})$$
$$\mu_{\text{winsorized}} = \frac{1}{W_{\text{total}}} \sum_{i=0}^{N-1} w_i \cdot \tilde{x}_i$$

---

## 7. Two-Distribution Comparison & Distance Metrics

For two histograms $H_1, H_2$ with identical binning geometry and normalized weights $P_i = w_{1,i} / W_{1,\text{total}}$, $Q_i = w_{2,i} / W_{2,\text{total}}$:

### 7.1 Weighted Chi-Square ($\chi^2$) Compatibility Test
$$\chi^2 = \sum_{i=0}^{N-1} \frac{(w_{1,i} - w_{2,i})^2}{\sigma_{1,i}^2 + \sigma_{2,i}^2}$$
where $\sigma_{k,i}^2 = \text{sum\_w2}_{k,i}$ (or $w_{k,i}$ for Poisson counts). $\text{NDF}$ is the number of bins where $\sigma_{1,i}^2 + \sigma_{2,i}^2 > 0$.

### 7.2 Kolmogorov-Smirnov ($D$) Metric
The maximum vertical difference between the two empirical cumulative distribution functions:
$$F_k(i) = \sum_{j=0}^i P_{k, j}$$
$$D = \max_{0 \le i < N} |F_1(i) - F_2(i)| \in [0.0, 1.0]$$

### 7.3 1D Wasserstein / Earth Mover's Distance ($W_1$)
The $L_1$ distance between the cumulative distribution functions:
$$W_1 = \int_{-\infty}^{\infty} |F_1(x) - F_2(x)| \, dx = \sum_{i=0}^{N-1} |F_1(i) - F_2(i)| \cdot \Delta x_i$$

### 7.4 Kullback-Leibler (KL) Divergence
The relative entropy from distribution $Q$ to distribution $P$ (measured in nats):
$$D_{\text{KL}}(P \parallel Q) = \sum_{i=0}^{N-1} P_i \ln \frac{P_i}{Q_i}$$
To prevent $\ln(0)$ singularities when $P_i > 0$ and $Q_i = 0$, $Q_i$ is floor-clamped to $\epsilon = 10^{-12}$.

### 7.5 Bhattacharyya Distance ($D_B$)
Measures the overlap between two statistical distributions:
$$\text{BC}(P, Q) = \sum_{i=0}^{N-1} \sqrt{P_i \cdot Q_i}$$
$$D_B(P, Q) = -\ln(\text{BC}(P, Q)) \ge 0.0$$

---

## 8. Bin Uncertainties & Error Propagation

### 8.1 Bin Variance & Standard Error
- **Unweighted Fills ($w_j = 1$)**:
  $$\sigma_i = \sqrt{N_i}$$
- **Weighted Fills ($w_j \ne 1$, with `HISTO_FLAG_TRACK_SUMW2`)**:
  $$\sigma_i = \sqrt{\sum_{j \in \text{bin } i} w_j^2}$$

### 8.2 Error Propagation in Arithmetic Operations
- **Addition ($H_3 = H_1 + H_2$) & Subtraction ($H_3 = H_1 - H_2$)**:
  $$\text{sum\_w2}_{3, i} = \text{sum\_w2}_{1, i} + \text{sum\_w2}_{2, i}$$

- **Scalar Scaling ($H_3 = c \cdot H_1$)**:
  $$\text{sum\_w2}_{3, i} = c^2 \cdot \text{sum\_w2}_{1, i}$$

- **Multiplication ($H_3 = H_1 \cdot H_2$)**:
  Using the algebraically expanded, **division-free** formula:
  $$\text{sum\_w2}_{3, i} = H_{2, i}^2 \cdot \text{sum\_w2}_{1, i} + H_{1, i}^2 \cdot \text{sum\_w2}_{2, i}$$
  *(Guaranteed division-free: evaluates cleanly to $0.0$ if either bin is empty, eliminating NaN singularities).*

- **Division ($H_3 = H_1 / H_2$)**:
  $$\text{sum\_w2}_{3, i} = \frac{\text{sum\_w2}_{1, i} \cdot H_{2, i}^2 + \text{sum\_w2}_{2, i} \cdot H_{1, i}^2}{H_{2, i}^4}$$
  If $H_{2, i} == 0$, $H_{3, i} = 0.0$ and $\text{sum\_w2}_{3, i} = 0.0$.

---

## 9. DDSketch Bounded Relative-Error Quantile Sketch

DDSketch (Masson et al., VLDB 2019) provides an online sketch for streaming data with a guaranteed relative-error bound $\alpha \in (0, 1)$ over unbounded numeric values.

### 9.1 Base Multiplier & Logarithmic Bin Mapping
Given relative accuracy parameter $\alpha$, the growth factor $\gamma$ is defined as:
$$\gamma = \frac{1 + \alpha}{1 - \alpha} > 1$$

For a non-zero value $x$, its logarithmic bin index $k$ is:
$$k = \left\lceil \frac{\ln |x|}{\ln \gamma} \right\rceil$$

Bin $k$ covers the half-open interval $(\gamma^{k-1}, \gamma^k]$.

### 9.2 Representative Value & Relative Error Guarantee
When retrieving the quantile for a cumulative rank in bin $k$, the representative value $\hat{q}$ is chosen as the midpoint of the relative error envelope:
$$\hat{q} = \frac{2 \gamma^k}{1 + \gamma}$$

For any value $x \in (\gamma^{k-1}, \gamma^k]$:
$$|x - \hat{q}| \le \alpha \cdot x$$
which guarantees that the reconstructed quantile has relative error bounded by $\alpha$:
$$\frac{|\hat{q} - q|}{q} \le \alpha$$

### 9.3 Zero and Negative Value Support
- **Zero Value Accumulator**: A distinct scalar accumulator $w_0$ captures exact zeros ($x = 0$).
- **Symmetric Positive & Negative Stores**: Values with $x > 0$ and $x < 0$ are maintained in separate logarithmic stores (`pos` and `neg`), ensuring identical $\alpha$ error guarantees across all positive and negative real numbers.

