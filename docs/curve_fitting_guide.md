# Curve Fitting & Non-Linear Regression Guide {#curve_fitting_guide}

This guide details the curve fitting and non-linear regression capabilities in `libhisto` (`include/histo/fit.h` and the `histo fit` CLI tool). `libhisto` provides high-performance Levenberg-Marquardt (LM) non-linear optimization, Direct Linear Least Squares for polynomials, Weighted Chi-Square minimization, and Poisson Maximum Likelihood Estimation (Cash deviance / Baker-Cousins).

---

## 1. CLI: `histo fit`

The `histo fit` command-line tool fits parametric models directly to streaming data or serialized histograms (`.bin` or `.json`), producing formatted parameter estimates, standard errors, 95% confidence intervals, \f$\chi^2/\mathrm{NDF}\f$ goodness-of-fit metrics, and visual ASCII curve overlays.


### 1.1 Supported Built-in Models

| Model Name | CLI Flag | Formula | Parameters |
| :--- | :--- | :--- | :--- |
| **Gaussian** | `-m gaussian` | \f$ f(x) = A \cdot \exp\left(-\frac{(x - \mu)^2}{2\sigma^2}\right) \f$ | [0] Amplitude (A), [1] Mean (μ), [2] Std Dev (σ) |
| **Exponential** | `-m exponential` | \f$ f(x) = A \cdot \exp(-\lambda x) + C \f$ | [0] Amplitude (A), [1] Decay Rate (λ), [2] Baseline (C) |
| **Polynomial** | `-m polynomial -d N` | \f$ f(x) = \sum_{k=0}^N c_k x^k \f$ | [0] c0, [1] c1, ..., [N] cN (Degree 0 to 10) |
| **Breit-Wigner** | `-m breit-wigner` | \f$ f(x) = \frac{A}{\pi} \frac{\Gamma/2}{(x - M)^2 + (\Gamma/2)^2} \f$ | [0] Area Scale (A), [1] Peak Mass (M), [2] FWHM (Γ) |
| **Power Law** | `-m power-law` | \f$ f(x) = A \cdot (x - x_0)^k \f$ | [0] Amplitude (A), [1] Exponent (k), [2] Origin (x0) |
| **Log-Normal** | `-m lognormal` | \f$ f(x) = \frac{A}{x\sigma\sqrt{2\pi}} \exp\left(-\frac{(\ln x - \mu)^2}{2\sigma^2}\right) \f$ | [0] Scale (A), [1] Log-Mean (μ), [2] Log-Std (σ) |
| **Gauss + Linear** | `-m gauss+linear` | \f$ f(x) = A \cdot \exp\left(-\frac{(x - \mu)^2}{2\sigma^2}\right) + c_0 + c_1 x \f$ | [0] Amp (A), [1] Mean (μ), [2] Std (σ), [3] c0, [4] c1 |
| **Weibull** | `-m weibull` | \f$ f(x) = A \frac{k}{\lambda} \left(\frac{x}{\lambda}\right)^{k-1} \exp\left(-\left(\frac{x}{\lambda}\right)^k\right) \f$ | [0] Scale (A), [1] Shape (k), [2] Scale (λ) |
| **Gamma / Erlang** | `-m gamma` | \f$ f(x) = A \frac{x^{k-1} \exp(-x/\theta)}{\Gamma(k) \theta^k} \f$ | [0] Scale (A), [1] Shape (k), [2] Scale (θ) |
| **Poisson** | `-m poisson` | \f$ f(x) = A \frac{\lambda^x \exp(-\lambda)}{\Gamma(x + 1)} \f$ | [0] Scale (A), [1] Rate (λ) |
| **Laplace** | `-m laplace` | \f$ f(x) = \frac{A}{2b} \exp\left(-\frac{\lvert x - \mu \rvert}{b}\right) \f$ | [0] Scale (A), [1] Location (μ), [2] Diversity (b) |


### 1.2 CLI Usage Recipes

```bash
# 1. Pipe Gaussian data into histo fill and fit with visual ASCII overlay
python3 -c "import random; print('\n'.join(str(random.gauss(50.0, 5.0)) for _ in range(10000)))" \
  | histo fill -n 50 --min 20 --max 80 \
  | histo fit -p

# 2. Fit quadratic background polynomial (Degree 2)
histo fit -m polynomial -d 2 background.json

# 3. Fit low-count Poisson data with Maximum Likelihood Estimation (--mle)
histo fit -m gaussian --mle sparse_counts.bin

# 4. Fit with parameter bounds and frozen parameters
histo fit -m gaussian --bounds 2=1.0:10.0 --fix-param 1=50.0 data.json

# 5. Output machine-readable JSON results
histo fit -m gaussian -j data.bin > fit_results.json
```

---

## 2. Programmatic C99 API (`histo/fit.h`)

### 2.1 Core API Workflow

```c
#include <stdio.h>
#include <histo/histo.h>
#include <histo/fit.h>

int main(void) {
    // 1. Create and fill a histogram
    histo_t *h = histo_create_uniform(50, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
    for (int i = 0; i < 5000; ++i) {
        double val = 50.0 + ((i % 30) - 15) * 1.5;
        histo_fill(h, val);
    }

    // 2. Configure fit options (optional: pass NULL for standard defaults)
    histo_fit_options_t opts;
    histo_fit_options_init(&opts);
    opts.loss_type = HISTO_FIT_LOSS_CHI2; // Or HISTO_FIT_LOSS_POISSON_MLE for low counts
    opts.max_iterations = 500;
    opts.ftol = 1e-8;

    // 3. Execute fit (initial guesses estimated automatically from moments if NULL)
    histo_fit_result_t *res = NULL;
    histo_status_t status = histo_fit_model(h, HISTO_FIT_MODEL_GAUSSIAN, NULL, &opts, &res);

    if (status == HISTO_OK && res->converged) {
        printf("Convergence: %s (%u iterations)\n", res->stop_reason, res->iterations);
        printf("Goodness-of-fit: Chi2/NDF = %.2f / %d (p-value: %.4g)\n",
               res->chi2, res->ndf, res->p_value);
        printf("Fitted Parameters:\n");
        printf("  [0] Amplitude (A) = %.3f +/- %.3f\n", res->params[0], res->param_errors[0]);
        printf("  [1] Mean (mu)     = %.3f +/- %.3f\n", res->params[1], res->param_errors[1]);
        printf("  [2] Sigma         = %.3f +/- %.3f\n", res->params[2], res->param_errors[2]);
    }

    // 4. Clean lifecycle management
    histo_fit_result_destroy(res);
    histo_destroy(h);
    return 0;
}
```

### 2.2 Parameter Constraints & Freezing

You can constrain parameters within physical boundaries ([lower_bound, upper_bound]) or freeze known constants during optimization:


```c
histo_fit_options_t opts;
histo_fit_options_init(&opts);

double lower_bounds[3] = {0.0, 40.0, 0.5};    // A >= 0, mu >= 40, sigma >= 0.5
double upper_bounds[3] = {1e6, 60.0, 20.0};   // A <= 1e6, mu <= 60, sigma <= 20
bool fixed_params[3]   = {false, true, false}; // Freeze mu at initial guess value

opts.lower_bounds = lower_bounds;
opts.upper_bounds = upper_bounds;
opts.fixed_params = fixed_params;

double initial_guess[3] = {1000.0, 50.0, 5.0};
histo_fit_result_t *res = NULL;
histo_fit_model(h, HISTO_FIT_MODEL_GAUSSIAN, initial_guess, &opts, &res);
```

### 2.3 Custom User Models (`histo_fit_custom`)

Fit arbitrary non-linear user functions with finite-difference or analytical gradients:

```c
#include <stdio.h>
#include <math.h>
#include <histo/histo.h>
#include <histo/fit.h>

// User function callback: f(x; p0, p1) = p0 * sin(p1 * x)
static double my_sine_model(double x, const double *p, void *userdata) {
    (void)userdata;
    return p[0] * sin(p[1] * x);
}

// Optional analytical gradient: [df/dp0, df/dp1]
static void my_sine_gradient(double x, const double *p, double *grad, void *userdata) {
    (void)userdata;
    grad[0] = sin(p[1] * x);
    grad[1] = p[0] * x * cos(p[1] * x);
}

int main(void) {
    histo_t *h = histo_create_uniform(20, 0.0, 10.0, HISTO_FLAG_NONE);
    for (int i = 0; i < 100; ++i) histo_fill(h, (double)i * 0.1);

    histo_fit_options_t opts;
    histo_fit_options_init(&opts);
    opts.grad_fn = my_sine_gradient; // Or NULL for automatic central finite differences

    double initial_params[2] = {10.0, 0.1};
    histo_fit_result_t *res = NULL;
    histo_fit_custom(h, my_sine_model, 2, initial_params, &opts, &res);

    histo_fit_result_destroy(res);
    histo_destroy(h);
    return 0;
}
```


---

## 3. Statistical Formulations & Loss Functions

### 3.1 Chi-Square Minimization (\f$ \chi^2 \f$)
Standard weighted least squares accounting for per-bin variance:
\f[
\chi^2(\mathbf{p}) = \sum_{i=1}^N \frac{(y_i - f(x_i; \mathbf{p}))^2}{\sigma_i^2}
\f]
where \f$\sigma_i^2 = \sum w_i^2\f$ if tracked, or \f$\max(y_i, 1.0)\f$ for unweighted counts.

### 3.2 Poisson Maximum Likelihood Estimation (Cash Deviance)
For sparse or low-count histograms where Gaussian approximations fail, Cash Poisson deviance minimizes:
\f[
-2 \ln \lambda(\mathbf{p}) = 2 \sum_{i=1}^N \left[ f(x_i; \mathbf{p}) - y_i + y_i \ln\left(\frac{y_i}{f(x_i; \mathbf{p})}\right) \right]
\f]
