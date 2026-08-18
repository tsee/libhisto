# Curve Fitting & Non-Linear Regression Guide {#curve_fitting_guide}

`libhisto` provides a high-performance, modular curve fitting and regression engine in `histo/fit.h`. It supports built-in parametric models (Gaussian, Exponential, Polynomial, Breit-Wigner, Power Law) and arbitrary user-defined models with analytical or finite-difference gradients.

---

## 1. Mathematical Formulations & Algorithms

### 1.1 Chi-Square Minimization (Weighted Least Squares)

For a histogram with \f$N\f$ bins, bin centers \f$x_i\f$, observed contents \f$y_i\f$, and variances \f$\sigma_i^2\f$, the Chi-Square objective function is:

\f[
\chi^2(\mathbf{p}) = \sum_{i=1}^N \frac{(y_i - f(x_i; \mathbf{p}))^2}{\sigma_i^2}
\f]

where:
- When sum-of-weights-squared tracking (`HISTO_FLAG_TRACK_SUMW2`) is enabled, \f$\sigma_i^2 = \sum w_i^2\f$.
- For unweighted Poisson counts, \f$\sigma_i^2 = \max(y_i, 1.0)\f$.
- For unweighted least squares (`HISTO_FIT_LOSS_UNWEIGHTED_LS`), \f$\sigma_i = 1.0\f$, and the covariance matrix is scaled post-fit by \f$s^2 = \chi^2 / \text{ndf}\f$.

### 1.2 Binned Poisson Maximum Likelihood Estimation (Cash / Baker-Cousins Deviance)

For low-count or sparse Poisson histograms where Gaussian approximations (\f$\sigma_i \approx \sqrt{y_i}\f$) break down, `libhisto` provides the Cash Poisson deviance statistic:

\f[
-2 \ln \lambda(\mathbf{p}) = 2 \sum_{i=1}^N \left[ f(x_i; \mathbf{p}) - y_i + y_i \ln\left(\frac{y_i}{f(x_i; \mathbf{p})}\right) \right]
\f]

with the convention \f$0 \ln(0 / f) \equiv 0\f$. The gradient and Fisher Information (approximate Hessian) used in Levenberg-Marquardt iterations are:

\f[
\nabla [-2\ln\lambda] = 2 \sum_{i=1}^N \left(1 - \frac{y_i}{f(x_i; \mathbf{p})}\right) \nabla f(x_i; \mathbf{p})
\f]

\f[
\mathbf{H} \approx 2 \sum_{i=1}^N \frac{1}{f(x_i; \mathbf{p})} \nabla f(x_i; \mathbf{p}) \nabla f(x_i; \mathbf{p})^T
\f]

### 1.3 Levenberg-Marquardt (LM) Optimizer

The Levenberg-Marquardt algorithm adaptively interpolates between Gradient Descent (when far from the optimum) and Gauss-Newton optimization (near the optimum). At iteration \f$k\f$, the step \f$\Delta \mathbf{p}\f$ is solved from:

\f[
\left(\mathbf{J}^T \mathbf{W} \mathbf{J} + \lambda \mathrm{diag}(\mathbf{J}^T \mathbf{W} \mathbf{J})\right) \Delta \mathbf{p} = \mathbf{J}^T \mathbf{W} (\mathbf{y} - \mathbf{f}(\mathbf{p}))
\f]

- **Damping parameter \f$\lambda\f$**: Updated via the Marquardt-Nielsen gain ratio \f$\rho = \frac{S(\mathbf{p}) - S(\mathbf{p} + \Delta \mathbf{p})}{L(\mathbf{0}) - L(\Delta \mathbf{p})}\f$. When \f$\rho > 0.75\f$, \f$\lambda\f$ is decreased; when \f$\rho < 0.25\f$, \f$\lambda\f$ is increased and the step is rejected.
- **System Solver**: Solved via Cholesky decomposition (\f$\mathbf{L} \mathbf{L}^T\f$) with positive-definite diagonal ridge regularization.

### 1.4 Direct Linear Least Squares for Polynomials

For polynomial models of degree \f$d \le 10\f$ under \f$\chi^2\f$ loss without non-linear bounds, the normal equations \f$\mathbf{X}^T \mathbf{W} \mathbf{X} \mathbf{c} = \mathbf{X}^T \mathbf{W} \mathbf{y}\f$ are solved directly in a single exact pass via Cholesky decomposition without requiring initial guesses or iterations.

---

## 2. Covariance Matrix & Goodness of Fit

### 2.1 Covariance & Parameter Standard Errors

The parameter covariance matrix \f$\mathbf{C} \in \mathbb{R}^{P \times P}\f$ is obtained from the inverse of the curvature matrix:

\f[
\mathbf{C} = \left(\mathbf{J}^T \mathbf{W} \mathbf{J}\right)^{-1}
\f]

The estimated standard error for parameter \f$j\f$ is:

\f[
\sigma_{p_j} = \sqrt{C_{jj}}
\f]

The parameter correlation matrix is:

\f[
R_{jk} = \frac{C_{jk}}{\sigma_{p_j} \sigma_{p_k}}
\f]

### 2.2 Goodness-of-Fit Diagnostics

- **Degrees of Freedom (NDF)**: \f$\text{ndf} = N_{\text{bins}} - N_{\text{free\_params}}\f$.
- **Reduced \f$\chi^2\f$**: \f$\chi^2_\nu = \chi^2 / \text{ndf}\f$. A good fit typically has \f$\chi^2_\nu \approx 1.0\f$.
- **p-Value**: Upper-tail probability from the Chi-Square distribution:
  \f[
  p = P\left(X \ge \chi^2_{\text{obs}} \mid \text{ndf}\right) = \frac{\Gamma(\text{ndf}/2, \chi^2/2)}{\Gamma(\text{ndf}/2)}
  \f]
  Computed via regularized incomplete gamma functions.
- **Information Criteria**:
  - Akaike Information Criterion: \f$\text{AIC} = 2k - 2\ln L\f$.
  - Bayesian Information Criterion: \f$\text{BIC} = k \ln N - 2\ln L\f$.

---

## 3. Practical Usage & C99 Code Examples

### 3.1 Gaussian Resonance Peak Fit

```c
#include "histo/histo.h"
#include "histo/fit.h"
#include <stdio.h>

void fit_resonance_example(histo_t *h) {
    histo_fit_result_t *res = NULL;
    
    /* Automatic initial guess estimation from moments */
    histo_status_t status = histo_fit_model(h, HISTO_FIT_MODEL_GAUSSIAN, NULL, NULL, &res);
    if (status == HISTO_OK && res->converged) {
        printf("Fitted Amplitude: %.2f +/- %.2f\n", res->params[0], res->param_errors[0]);
        printf("Fitted Mean:      %.2f +/- %.2f\n", res->params[1], res->param_errors[1]);
        printf("Fitted Sigma:     %.2f +/- %.2f\n", res->params[2], res->param_errors[2]);
        printf("Chi2 / NDF:       %.2f / %d (Reduced: %.2f, p-value: %.4f)\n",
               res->chi2, res->ndf, res->reduced_chi2, res->p_value);
    }
    
    histo_fit_result_destroy(res);
}
```

### 3.2 Polynomial Background Fit with Sub-range

```c
#include "histo/histo.h"
#include "histo/fit.h"
#include <stdio.h>

void fit_background_example(histo_t *h) {
    histo_fit_options_t opts;
    histo_fit_options_init(&opts);
    opts.poly_degree = 2; /* Quadratic: c0 + c1*x + c2*x^2 */
    opts.range_min = 10.0;
    opts.range_max = 50.0;

    histo_fit_result_t *res = NULL;
    if (histo_fit_model(h, HISTO_FIT_MODEL_POLYNOMIAL, NULL, &opts, &res) == HISTO_OK) {
        printf("Background: f(x) = %.3e + %.3e*x + %.3e*x^2\n",
               res->params[0], res->params[1], res->params[2]);
    }
    histo_fit_result_destroy(res);
}
```

### 3.3 Custom Parametric Model with Analytical Gradient

```c
#include "histo/histo.h"
#include "histo/fit.h"
#include <math.h>
#include <stdio.h>

/* Double exponential decay: f(x) = A1*exp(-k1*x) + A2*exp(-k2*x) */
static double double_exp(double x, const double *p, void *userdata) {
    (void)userdata;
    return p[0] * exp(-p[1] * x) + p[2] * exp(-p[3] * x);
}

static void double_exp_grad(double x, const double *p, double *grad, void *userdata) {
    (void)userdata;
    double e1 = exp(-p[1] * x);
    double e2 = exp(-p[3] * x);
    grad[0] = e1;
    grad[1] = -p[0] * x * e1;
    grad[2] = e2;
    grad[3] = -p[2] * x * e2;
}

void fit_custom_example(histo_t *h) {
    histo_fit_options_t opts;
    histo_fit_options_init(&opts);
    opts.grad_fn = double_exp_grad;

    const double p_init[4] = {100.0, 1.0, 20.0, 0.1};
    histo_fit_result_t *res = NULL;

    if (histo_fit_custom(h, double_exp, 4, p_init, &opts, &res) == HISTO_OK) {
        printf("Fast decay rate: %.4f +/- %.4f\n", res->params[1], res->param_errors[1]);
        printf("Slow decay rate: %.4f +/- %.4f\n", res->params[3], res->param_errors[3]);
    }
    histo_fit_result_destroy(res);
}
```

---

## 4. Failure Modes & Remediation

| Failure Mode | Root Cause | Remediation |
| :--- | :--- | :--- |
| **Singular Hessian Matrix (`HISTO_FIT_ERR_SINGULAR`)** | Parameters are degenerate or linearly dependent (e.g. amplitude of zero component). | Freeze unneeded parameters using `opts.fixed_params` or add tighter box constraints. |
| **Local Minimum Trapping** | Initial guess is too far from true global basin. | Use `histo_fit_estimate_initial_params` or provide physically motivated initial guesses. |
| **Divergence (`HISTO_FIT_ERR_DIVERGENCE`)** | Model evaluated outside domain (e.g. \f$x - x_0 \le 0\f$ for power law or \f$\sigma \le 0\f$ for Gaussian). | Set box constraints with `opts.lower_bounds` and `opts.upper_bounds`. |
| **Under-constrained Fit (`ndf <= 0`)** | Number of bins in fit range is less than or equal to free parameters. | Broaden fit range `[opts.range_min, opts.range_max]` or increase histogram binning resolution. |
