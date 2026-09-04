# Design Specification: Statistical & Analytical Formulae {#statistical_formulae}

This document provides a mathematically rigorous specification of the statistical estimators, error-propagation algorithms, and sketch mechanics implemented in `libhisto`.

The statistical features are organized into a tiered structure corresponding to the level of data aggregation: online sample-level algorithms (Tier 1), binned distribution approximations (Tier 2), and histogram-level distance metrics (Tier 3).

---

## Tier 1: Continuous Online Estimators

These algorithms operate directly on the continuous sample stream before or alongside binning, preventing discretization error.

### 1.1 Exact Sample Moments (Weighted Welford Algorithm)

When `HISTO_FLAG_EXACT_MOMENTS` is enabled, the first two moments are accumulated continuously using a numerically stable, single-pass Welford algorithm.

For a data stream of pairs $(x_k, w_k)$ with non-negative weights $w_k \ge 0$, let the cumulative weight be $W_k = \sum_{j=1}^k w_j$. The update equations for the $k$-th sample are:

$$
\delta_k = x_k - \mu_{k-1}
$$

$$
\mu_k = \mu_{k-1} + \frac{w_k}{W_k} \delta_k
$$

$$
M_{2, k} = M_{2, k-1} + w_k \cdot \delta_k \cdot (x_k - \mu_k)
$$

**Numerical Protection**: To prevent floating-point inaccuracies from driving $M_2$ slightly negative (e.g., when observing identical samples), $M_2$ is strictly clamped:

$$
M_2 \leftarrow \max(0.0, M_2)
$$

**Variance Formulations**:
- **Population Variance**: $\sigma^2 = \frac{M_{2}}{W}$
- **Unweighted Sample Variance** ($w_j = 1, N > 1$): $s^2 = \frac{M_{2}}{N - 1}$
- **Reliability-Weighted Sample Variance**: $s^2 = \frac{W}{W^2 - \sum w_i^2} M_{2}$

### 1.2 DDSketch: Bounded Relative-Error Quantile Sketch

DDSketch (Masson et al., VLDB 2019) maintains a streaming sketch of the data distribution, ensuring a strict relative-error bound $\alpha \in (0, 1)$ across the entire real line.

**Logarithmic Bin Mapping**:
Given an accuracy parameter $\alpha$, the growth factor is $\gamma = \frac{1 + \alpha}{1 - \alpha}$. 
For a non-zero value $x$, its sketch bin index $k$ is computed as:

$$
k = \left\lceil \frac{\ln |x|}{\ln \gamma} \right\rceil
$$

This bin covers the half-open interval $(\gamma^{k-1}, \gamma^k]$. 

**Quantile Reconstruction**:
The representative value $\hat{q}$ for bin $k$ is the relative-error midpoint:

$$
\hat{q} = \frac{2 \gamma^k}{1 + \gamma}
$$

For any $x$ in bin $k$, the relative error is analytically bounded by $\alpha$:

$$
\frac{|x - \hat{q}|}{|x|} \le \alpha
$$

---

## Tier 2: Binned Distribution Approximations

When exact sample tracking is disabled, statistics are estimated from the binned distribution. Let $x_{c, i}$ be the center of bin $i$, $w_i$ be its weight, and $W_{\text{total}} = \sum w_i$.

### 2.1 Central Moments & Shape Measures

**Mean**:
$$
\mu = \frac{1}{W_{\text{total}}} \sum_{i=0}^{N-1} w_i \cdot x_{c, i}
$$

**$k$-th Central Moment ($M_k$)**:
$$
M_k = \frac{1}{W_{\text{total}}} \sum_{i=0}^{N-1} w_i \cdot (x_{c, i} - \mu)^k
$$
where $M_2 = \sigma^2$ is the binned variance.

**Skewness ($\gamma_1$)**:
Measures the asymmetry of the probability distribution:
$$
\gamma_1 = \frac{M_3}{M_2^{3/2}} = \frac{M_3}{\sigma^3}
$$

**Kurtosis ($\beta_2$) & Excess Kurtosis ($\gamma_2$)**:
Measures the tailedness of the distribution relative to a standard Gaussian:
$$
\beta_2 = \frac{M_4}{\sigma^4}, \quad \gamma_2 = \beta_2 - 3.0
$$

### 2.2 Mode, Peaks, and FWHM

**Continuous Mode (Parabolic Interpolation)**:
For a uniform histogram where the mode bin $m$ is an interior bin, a 3-point parabolic interpolation determines the sub-bin peak position. Let $D = 2 w_m - w_{m-1} - w_{m+1}$. If $D > 0$:

$$
\delta = \frac{1}{2} \frac{w_{m+1} - w_{m-1}}{D}, \quad \delta \in \left[-\frac{1}{2}, \frac{1}{2}\right]
$$

$$
x_{\text{mode}} = x_{c, m} + \delta \cdot \Delta x
$$

**Full Width at Half Maximum (FWHM)**:
Given a peak maximum $y_{\max} = w_m$, the threshold is $y_{\text{half}} = y_{\max} / 2$. The algorithm scans outward from $m$ to find the crossing points $x_{\text{left}}$ and $x_{\text{right}}$ via linear interpolation between the adjacent bin centers bridging the threshold.

$$
\text{FWHM} = \max(0.0, x_{\text{right}} - x_{\text{left}})
$$

### 2.3 Quantiles and Robust Statistics

**Interpolated Quantile $Q(p)$**:
For a target probability $p \in (0, 1)$, the target weight is $T = p \cdot W_{\text{total}}$. Locate the bin $k$ such that the cumulative sum $C_k = \sum_{i=0}^k w_i$ satisfies $C_{k-1} < T \le C_k$.
Assuming a uniform density within the bin, the value is linearly interpolated:

$$
\theta = \frac{T - C_{k-1}}{w_k}
$$

$$
Q(p) = x_{\text{lower}, k} + \theta \cdot (x_{\text{upper}, k} - x_{\text{lower}, k})
$$

**Median Absolute Deviation (MAD)**:
A robust measure of variability:

$$
\text{MAD} = \text{median}(|x - \text{median}(x)|)
$$

Computed in optimal $O(N)$ deterministic time and $O(1)$ auxiliary space via a two-pointer monotonic merge over pre-sorted histogram bins, expanding outward from the median bin without memory allocations or comparison sorting.

---

## Tier 3: Distribution Comparisons & Uncertainties

These metrics quantify the dissimilarity between two histograms $H_1, H_2$ with identical binning geometries. Let $P_i = w_{1,i} / W_{1,\text{total}}$ and $Q_i = w_{2,i} / W_{2,\text{total}}$ be the normalized probability mass functions.

### 3.1 Distance Metrics

**Kolmogorov-Smirnov ($D$) Metric**:
The maximum absolute difference between the empirical cumulative distribution functions $F_1(x)$ and $F_2(x)$:

$$
D = \max_{0 \le i < N} |F_1(i) - F_2(i)|
$$

**1D Wasserstein / Earth Mover's Distance ($W_1$)**:
The $L_1$ distance between the cumulative distribution functions:

$$
W_1 = \sum_{i=0}^{N-1} |F_1(i) - F_2(i)| \cdot \Delta x_i
$$

**Kullback-Leibler (KL) Divergence**:
The relative entropy, clamped against singularities using a numerical floor $\epsilon = 10^{-12}$ for $Q_i = 0$:

$$
D_{\text{KL}}(P \parallel Q) = \sum_{i=0}^{N-1} P_i \ln \frac{P_i}{\max(Q_i, \epsilon)}
$$

### 3.2 Error Propagation

The uncertainty in bin $i$ is tracked as the sum of squared weights: $\sigma_i^2 = \sum w_{j}^2$.

During arithmetic operations (e.g., histogram division $H_3 = H_1 / H_2$), standard linear error propagation is applied, assuming uncorrelated bins:

$$
\sigma_{3, i}^2 = \frac{\sigma_{1, i}^2 \cdot w_{2, i}^2 + \sigma_{2, i}^2 \cdot w_{1, i}^2}{w_{2, i}^4}
$$

*(If $w_{2, i} == 0$, the result is strictly $w_{3, i} = 0.0, \sigma_{3, i}^2 = 0.0$ to avoid numerical explosion).*
