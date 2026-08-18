# Curve Fitting & Non-Linear Regression Guide

`libhisto` provides a high-performance, modular curve fitting and regression engine in `histo/fit.h`. It supports built-in parametric models (Gaussian, Exponential, Polynomial, Breit-Wigner, Power Law) and arbitrary user-defined models with analytical or finite-difference gradients.

---

## 1. Mathematical Formulations & Algorithms

### 1.1 Chi-Square Minimization (Weighted Least Squares)

For a histogram with $N$ bins, bin centers $x_i$, observed contents $y_i$, and variances $\sigma_i^2$, the Chi-Square objective function is:

$$\chi^2(\mathbf{p}) = \sum_{i=1}^N \frac{(y_i - f(x_i; \mathbf{p}))^2}{\sigma_i^2}$$

where:
- When sum-of-weights-squared tracking (`HISTO_FLAG_TRACK_SUMW2`) is enabled, $\sigma_i^2 = \sum w_i^2$.
- For unweighted Poisson counts, $\sigma_i^2 = \max(y_i, 1.0)$.
- For unweighted least squares (`HISTO_FIT_LOSS_UNWEIGHTED_LS`), $\sigma_i = 1.0$, and the covariance matrix is scaled post-fit by $s^2 = \chi^2 / \text{ndf}$.

### 1.2 Binned Poisson Maximum Likelihood Estimation (Cash / Baker-Cousins Deviance)

For low-count or sparse Poisson histograms where Gaussian approximations ($\sigma_i \approx \sqrt{y_i}$) break down, `libhisto` provides the Cash Poisson deviance statistic:

$$-2 \ln \lambda(\mathbf{p}) = 2 \sum_{i=1}^N \left[ f(x_i; \mathbf{p}) - y_i + y_i \ln\left(\frac{y_i}{f(x_i; \mathbf{p})}\right) \right]$$

with the convention $0 \ln(0 / f) \equiv 0$. The gradient and Fisher Information (approximate Hessian) used in Levenberg-Marquardt iterations are:

$$\nabla [-2\ln\lambda] = 2 \sum_{i=1}^N \left(1 - \frac{y_i}{f(x_i; \mathbf{p})}\right) \nabla f(x_i; \mathbf{p})$$

$$\mathbf{H} \approx 2 \sum_{i=1}^N \frac{1}{f(x_i; \mathbf{p})} \nabla f(x_i; \mathbf{p}) \nabla f(x_i; \mathbf{p})^T$$

### 1.3 Levenberg-Marquardt (LM) Optimizer

The Levenberg-Marquardt algorithm adaptively interpolates between Gradient Descent (when far from the optimum) and Gauss-Newton optimization (near the optimum). At iteration $k$, the step $\Delta \mathbf{p}$ is solved from:

$$\left(\mathbf{J}^T \mathbf{W} \mathbf{J} + \lambda \operatorname{diag}(\mathbf{J}^T \mathbf{W} \mathbf{J})\right) \Delta \mathbf{p} = \mathbf{J}^T \mathbf{W} (\mathbf{y} - \mathbf{f}(\mathbf{p}))$$

- **Damping parameter $\lambda$**: Updated via the Marquardt-Nielsen gain ratio $\rho = \frac{S(\mathbf{p}) - S(\mathbf{p} + \Delta \mathbf{p})}{L(\mathbf{0}) - L(\Delta \mathbf{p})}$. When $\rho > 0.75$, $\lambda$ is decreased; when $\rho < 0.25$, $\lambda$ is increased and the step is rejected.
- **System Solver**: Solved via Cholesky decomposition ($\mathbf{L} \mathbf{L}^T$) with positive-definite diagonal ridge regularization.

### 1.4 Direct Linear Least Squares for Polynomials

For polynomial models of degree $d \le 10$ under $\chi^2$ loss without non-linear bounds, the normal equations $\mathbf{X}^T \mathbf{W} \mathbf{X} \mathbf{c} = \mathbf{X}^T \mathbf{W} \mathbf{y}$ are solved directly in a single exact pass via Cholesky decomposition without requiring initial guesses or iterations.

---

## 2. Covariance Matrix & Goodness of Fit

### 2.1 Covariance & Parameter Standard Errors

The parameter covariance matrix $\mathbf{C} \in \mathbb{R}^{P \times P}$ is obtained from the inverse of the curvature matrix:

$$\mathbf{C} = \left(\mathbf{J}^T \mathbf{W} \mathbf{J}\right)^{-1}$$

The estimated standard error for parameter $j$ is:

$$\sigma_{p_j} = \sqrt{C_{jj}}$$

The parameter correlation matrix is:

$$R_{jk} = \frac{C_{jk}}{\sigma_{p_j} \sigma_{p_k}}$$

### 2.2 Goodness-of-Fit Diagnostics

- **Degrees of Freedom (NDF)**: $\text{ndf} = N_{\text{bins}} - N_{\text{free\_params}}$.
- **Reduced $\chi^2$**: $\chi^2_\nu = \chi^2 / \text{ndf}$. A good fit typically has $\chi^2_\nu \approx 1.0$.
- **$p$-Value**: Upper-tail probability from the Chi-Square distribution:
  $$p = P\left(X \ge \chi^2_{\text{obs}} \mid \text{ndf}\right) = \frac{\Gamma(\text{ndf}/2, \chi^2/2)}{\Gamma(\text{ndf}/2)}$$
  Computed via regularized incomplete gamma functions.
- **Information Criteria**:
  - Akaike Information Criterion: $\text{AIC} = 2k - 2\ln L$.
  - Bayesian Information Criterion: $\text{BIC} = k \ln N - 2\ln L$.

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
| **Divergence (`HISTO_FIT_ERR_DIVERGENCE`)** | Model evaluated outside domain (e.g. $x - x_0 \le 0$ for power law or $\sigma \le 0$ for Gaussian). | Set box constraints with `opts.lower_bounds` and `opts.upper_bounds`. |
| **Under-constrained Fit (`ndf <= 0`)** | Number of bins in fit range is less than or equal to free parameters. | Broaden fit range `[opts.range_min, opts.range_max]` or increase histogram binning resolution. |
