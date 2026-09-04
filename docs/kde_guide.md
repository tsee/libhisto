# Kernel Density Estimation (KDE) & Automated Binning Guide {#kde_guide}

`libhisto` provides high-performance, non-parametric 1-dimensional Kernel Density Estimation (KDE) and automated optimal bin width estimation algorithms.

---

## 1. Automated Optimal Bin Width Heuristics

When binning continuous data, choosing an inappropriate bin width $h$ or bin count $N_{\text{bins}}$ can obscure multi-modality (over-smoothing) or introduce sampling noise (under-smoothing). `libhisto` provides standard statistical heuristics:

| Rule | Formula | Key Characteristic |
| :--- | :--- | :--- |
| **Freedman-Diaconis (FD)** | $h = 2 \cdot \text{IQR} \cdot n^{-1/3}$ | **Default**: Robust against outliers and heavy tails. |
| **Scott's Reference** | $h = 3.49 \cdot \sigma \cdot n^{-1/3}$ | Optimal for Gaussian-distributed data. |
| **Sturges' Rule** | $N = \lceil \log_2(n) + 1 \rceil$ | Classic rule for small sample sizes ($n < 200$). |
| **Doane's Rule** | $N = \lceil 1 + \log_2(n) + \log_2(1 + |\gamma_1|/\sigma_{\gamma_1}) \rceil$ | Extends Sturges for skewed distributions. |
| **Knuth's Rule** | $\arg\max_M F(M)$ posterior log-likelihood | Non-parametric Bayesian model selection. |

### C99 API Usage
```c
#include <stdio.h>
#include <histo/histo.h>

int main(void) {
    double samples[] = { 1.2, 2.3, 2.5, 3.1, 4.8, 5.0, 5.2, 6.1 };
    size_t n = sizeof(samples) / sizeof(samples[0]);

    /* Option A: One-liner automatic histogram creation and fill */
    histo_t *h = histo_create_auto(n, samples, HISTO_BIN_RULE_AUTO, HISTO_FLAG_TRACK_SUMW2);
    printf("Auto-binned histogram created with %u bins\n", histo_nbins(h));
    histo_destroy(h);

    /* Option B: Estimate parameters explicitly */
    uint32_t nbins = 0;
    double min_val = 0.0, max_val = 0.0;
    histo_estimate_bins_fd(n, samples, &nbins, &min_val, &max_val);
    printf("Freedman-Diaconis estimate: %u bins on [%.2f, %.2f]\n", nbins, min_val, max_val);
    return 0;
}
```

### Python API Usage
```python
from histo import Histogram

# Automatic sizing and ingestion
h = Histogram.auto(data, rule="fd")
```

### Perl API Usage
```perl
use Math::Histo;

# Automatic sizing and ingestion
my $h = Math::Histo->create_auto(\@data, rule => 'fd');
```

---

## 2. Kernel Density Estimation (`histo_kde`)

Given $n$ observations $\{x_i\}$ with weights $\{w_i\}$ ($\sum w_i = W$) and smoothing bandwidth $h > 0$:

$$
\hat{f}_h(x) = \frac{1}{W h} \sum_{i=1}^n w_i K\left(\frac{x - x_i}{h}\right)
$$

### Supported Kernels $K(u)$

1. **Gaussian** (Default):
   $$K(u) = \frac{1}{\sqrt{2\pi}} e^{-u^2/2}$$
2. **Epanechnikov**:
   $$K(u) = \frac{3}{4}(1 - u^2) \quad \text{for } |u| \le 1$$
3. **Uniform / Boxcar**:
   $$K(u) = \frac{1}{2} \quad \text{for } |u| \le 1$$
4. **Triangular**:
   $$K(u) = 1 - |u| \quad \text{for } |u| \le 1$$
5. **Biweight / Quartic**:
   $$K(u) = \frac{15}{16}(1 - u^2)^2 \quad \text{for } |u| \le 1$$
6. **Cosine**:
   $$K(u) = \frac{\pi}{4} \cos\left(\frac{\pi}{2} u\right) \quad \text{for } |u| \le 1$$

---

## 3. Bandwidth Selection

- **Silverman's Rule of Thumb** (Default):
  $$h = 0.9 \cdot \min\left(\sigma, \frac{\text{IQR}}{1.34}\right) \cdot n^{-1/5}$$
- **Scott's Rule**:
  $$h = 1.059 \cdot \sigma \cdot n^{-1/5}$$
- **Manual**: Explicit user bandwidth $h > 0$, with optional scaling modifier `bw_adjust`.

---

## 4. Code Examples

### C99 Example
```c
#include <histo/kde.h>
#include <histo/histo.h>
#include <stdio.h>

int main(void) {
    double samples[] = { 1.2, 2.3, 2.5, 3.1, 4.8, 5.0, 5.2, 6.1 };
    size_t n = sizeof(samples) / sizeof(samples[0]);

    /* Construct model using defaults (Gaussian kernel, Silverman bandwidth) */
    histo_kde_t *kde = histo_kde_create(n, samples, NULL, NULL);

    /* Evaluate PDF */
    double pdf = histo_kde_eval(kde, 3.0);
    printf("f(3.0) = %.4f\n", pdf);

    /* Evaluate continuous CDF */
    double cdf = histo_kde_cdf(kde, 3.0);
    printf("F(3.0) = %.4f\n", cdf);

    /* Compute Quantile */
    double median = 0.0;
    histo_kde_quantile(kde, 0.50, &median);
    printf("Estimated Median = %.4f\n", median);

    histo_kde_destroy(kde);
    return 0;
}
```

### Python Example
```python
from histo import KDE, Histogram

# From raw data
kde = KDE(samples, kernel="gaussian", bw_method="silverman")
print("PDF at 3.0:", kde.eval(3.0))
print("CDF at 3.0:", kde.cdf(3.0))
print("Median:", kde.quantile(0.50))
print("Synthetic samples:", kde.sample(10, seed=42))

# Directly from an existing Histogram
h = Histogram(bins=50, range=(0, 100))
# ... fill h ...
h_kde = KDE.from_histogram(h)
```

### Perl Example
```perl
use Math::Histo::KDE;

my $kde = Math::Histo::KDE->new(
    samples   => \@samples,
    kernel    => 'gaussian',
    bw_method => 'silverman',
);

my $pdf    = $kde->eval(3.0);
my $cdf    = $kde->cdf(3.0);
my $median = $kde->quantile(0.50);
my @synth  = $kde->sample(100, 42);
```
