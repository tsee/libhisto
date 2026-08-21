#include "unity.h"
#include "histo/histo.h"
#include "histo/fit.h"
#include <math.h>
#include <stdlib.h>
#include <float.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void setUp(void) {}
void tearDown(void) {}

/* Helper: Fill histogram with synthetic model data */
static histo_t* create_synthetic_histo(
    uint32_t nbins,
    double min,
    double max,
    histo_fit_model_t model,
    const double *params,
    size_t num_params
) {
    histo_t *h = histo_create_uniform(nbins, min, max, HISTO_FLAG_TRACK_SUMW2);
    if (!h) return NULL;

    for (uint32_t i = 0; i < nbins; ++i) {
        double xc = 0.0;
        histo_bin_center(h, i, &xc);
        double val = histo_fit_eval(model, params, num_params, xc);
        if (val < 0.0) val = 0.0;
        histo_fill_w(h, xc, val);
    }
    return h;
}

/* -------------------------------------------------------------------------
 * 1. Built-in Model Fits: Gaussian, Exponential, Polynomial, Breit-Wigner, Power Law
 * ------------------------------------------------------------------------- */

void test_fit_gaussian_recovery(void) {
    const double true_params[] = {120.0, 5.0, 1.2}; /* A, mu, sigma */
    histo_t *h = create_synthetic_histo(100, 0.0, 10.0, HISTO_FIT_MODEL_GAUSSIAN, true_params, 3);
    TEST_ASSERT_NOT_NULL(h);

    histo_fit_result_t *res = NULL;
    histo_status_t status = histo_fit_model(h, HISTO_FIT_MODEL_GAUSSIAN, NULL, NULL, &res);
    TEST_ASSERT_EQUAL(HISTO_OK, status);
    TEST_ASSERT_NOT_NULL(res);
    TEST_ASSERT_TRUE(res->converged);
    TEST_ASSERT_EQUAL_UINT32(3, res->num_params);

    /* Verify parameter recovery within tight tolerance */
    TEST_ASSERT_DOUBLE_WITHIN(1.0, true_params[0], res->params[0]);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, true_params[1], res->params[1]);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, true_params[2], res->params[2]);

    /* Diagnostics */
    TEST_ASSERT_TRUE(res->param_errors[0] > 0.0);
    TEST_ASSERT_TRUE(res->param_errors[1] > 0.0);
    TEST_ASSERT_TRUE(res->param_errors[2] > 0.0);
    TEST_ASSERT_TRUE(res->chi2 >= 0.0);
    TEST_ASSERT_EQUAL_INT(100 - 3, res->ndf);
    TEST_ASSERT_TRUE(res->reduced_chi2 >= 0.0);
    TEST_ASSERT_TRUE(res->p_value >= 0.0 && res->p_value <= 1.0);
    TEST_ASSERT_TRUE(isfinite(res->log_likelihood));
    TEST_ASSERT_TRUE(isfinite(res->aic));
    TEST_ASSERT_TRUE(isfinite(res->bic));

    /* Correlation matrix diagonal */
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1.0, res->cor_matrix[0 * 3 + 0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1.0, res->cor_matrix[1 * 3 + 1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1.0, res->cor_matrix[2 * 3 + 2]);

    histo_fit_result_destroy(res);
    histo_destroy(h);
}

void test_fit_exponential_decay(void) {
    const double true_params[] = {80.0, 0.4, 3.0}; /* A, lambda, C */
    histo_t *h = create_synthetic_histo(80, 0.0, 10.0, HISTO_FIT_MODEL_EXPONENTIAL, true_params, 3);
    TEST_ASSERT_NOT_NULL(h);

    histo_fit_result_t *res = NULL;
    histo_status_t status = histo_fit_model(h, HISTO_FIT_MODEL_EXPONENTIAL, NULL, NULL, &res);
    TEST_ASSERT_EQUAL(HISTO_OK, status);
    TEST_ASSERT_NOT_NULL(res);
    TEST_ASSERT_TRUE(res->converged);

    TEST_ASSERT_DOUBLE_WITHIN(2.0, true_params[0], res->params[0]);
    TEST_ASSERT_DOUBLE_WITHIN(0.02, true_params[1], res->params[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, true_params[2], res->params[2]);

    histo_fit_result_destroy(res);
    histo_destroy(h);
}

void test_fit_polynomial_linear_and_quadratic(void) {
    /* Degree 1 (Linear): y = 4.0 + 1.5 * x */
    const double linear_params[] = {4.0, 1.5};
    histo_t *h_lin = create_synthetic_histo(50, 0.0, 10.0, HISTO_FIT_MODEL_POLYNOMIAL, linear_params, 2);
    TEST_ASSERT_NOT_NULL(h_lin);

    histo_fit_options_t opts;
    histo_fit_options_init(&opts);
    opts.poly_degree = 1;
    opts.algo = HISTO_FIT_ALGO_LINEAR_LS;

    histo_fit_result_t *res_lin = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fit_model(h_lin, HISTO_FIT_MODEL_POLYNOMIAL, NULL, &opts, &res_lin));
    TEST_ASSERT_NOT_NULL(res_lin);
    TEST_ASSERT_TRUE(res_lin->converged);
    TEST_ASSERT_EQUAL(HISTO_FIT_CONVERGED_EXACT, res_lin->status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, linear_params[0], res_lin->params[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, linear_params[1], res_lin->params[1]);
    histo_fit_result_destroy(res_lin);
    histo_destroy(h_lin);

    /* Degree 2 (Quadratic): y = 2.0 - 0.8 * x + 0.15 * x^2 */
    const double quad_params[] = {2.0, -0.8, 0.15};
    histo_t *h_quad = create_synthetic_histo(60, 0.0, 10.0, HISTO_FIT_MODEL_POLYNOMIAL, quad_params, 3);
    TEST_ASSERT_NOT_NULL(h_quad);

    opts.poly_degree = 2;
    opts.algo = HISTO_FIT_ALGO_AUTO;
    histo_fit_result_t *res_quad = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fit_model(h_quad, HISTO_FIT_MODEL_POLYNOMIAL, NULL, &opts, &res_quad));
    TEST_ASSERT_NOT_NULL(res_quad);
    TEST_ASSERT_TRUE(res_quad->converged);
    TEST_ASSERT_DOUBLE_WITHIN(1e-3, quad_params[0], res_quad->params[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-3, quad_params[1], res_quad->params[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-3, quad_params[2], res_quad->params[2]);
    histo_fit_result_destroy(res_quad);

    /* Test Quadratic with LM algorithm explicitly */
    opts.algo = HISTO_FIT_ALGO_LEVENBERG_MARQUARDT;
    double p_init[] = {1.0, 0.0, 0.0};
    histo_fit_result_t *res_quad_lm = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fit_model(h_quad, HISTO_FIT_MODEL_POLYNOMIAL, p_init, &opts, &res_quad_lm));
    TEST_ASSERT_NOT_NULL(res_quad_lm);
    TEST_ASSERT_TRUE(res_quad_lm->converged);
    TEST_ASSERT_DOUBLE_WITHIN(1e-3, quad_params[0], res_quad_lm->params[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-3, quad_params[1], res_quad_lm->params[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-3, quad_params[2], res_quad_lm->params[2]);
    histo_fit_result_destroy(res_quad_lm);

    histo_destroy(h_quad);
}

void test_fit_breit_wigner(void) {
    const double true_params[] = {250.0, 91.18, 2.49}; /* A, M, Gamma */
    histo_t *h = create_synthetic_histo(100, 80.0, 102.0, HISTO_FIT_MODEL_BREIT_WIGNER, true_params, 3);
    TEST_ASSERT_NOT_NULL(h);

    const double p0[] = {200.0, 90.0, 3.0};
    histo_fit_result_t *res = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fit_model(h, HISTO_FIT_MODEL_BREIT_WIGNER, p0, NULL, &res));
    TEST_ASSERT_NOT_NULL(res);
    TEST_ASSERT_TRUE(res->converged);

    TEST_ASSERT_DOUBLE_WITHIN(5.0, true_params[0], res->params[0]);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, true_params[1], res->params[1]);
    TEST_ASSERT_DOUBLE_WITHIN(0.08, true_params[2], res->params[2]);

    histo_fit_result_destroy(res);
    histo_destroy(h);
}

void test_fit_power_law(void) {
    const double true_params[] = {50.0, -1.5, 0.5}; /* A, k, x0 */
    histo_t *h = create_synthetic_histo(60, 1.0, 15.0, HISTO_FIT_MODEL_POWER_LAW, true_params, 3);
    TEST_ASSERT_NOT_NULL(h);

    /* Provide reasonable initial guess and bound x0 below 1.0 */
    const double p0[] = {40.0, -1.2, 0.4};
    double lb[] = {0.1, -5.0, -10.0};
    double ub[] = {200.0, -0.1, 0.9};

    histo_fit_options_t opts;
    histo_fit_options_init(&opts);
    opts.lower_bounds = lb;
    opts.upper_bounds = ub;

    histo_fit_result_t *res = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fit_model(h, HISTO_FIT_MODEL_POWER_LAW, p0, &opts, &res));
    TEST_ASSERT_NOT_NULL(res);
    TEST_ASSERT_TRUE(res->converged);

    TEST_ASSERT_DOUBLE_WITHIN(3.0, true_params[0], res->params[0]);
    TEST_ASSERT_DOUBLE_WITHIN(0.1, true_params[1], res->params[1]);
    TEST_ASSERT_DOUBLE_WITHIN(0.1, true_params[2], res->params[2]);

    histo_fit_result_destroy(res);
    histo_destroy(h);
}

/* -------------------------------------------------------------------------
 * 2. Custom User Callback Model & Analytical vs Numerical Gradients
 * ------------------------------------------------------------------------- */

/* Double exponential model: f(x) = p0 * exp(-p1 * x) + p2 * exp(-p3 * x) */
static double custom_double_exp(double x, const double *p, void *userdata) {
    (void)userdata;
    return p[0] * exp(-p[1] * x) + p[2] * exp(-p[3] * x);
}

static void custom_double_exp_grad(double x, const double *p, double *grad, void *userdata) {
    (void)userdata;
    double e1 = exp(-p[1] * x);
    double e2 = exp(-p[3] * x);
    grad[0] = e1;
    grad[1] = -p[0] * x * e1;
    grad[2] = e2;
    grad[3] = -p[2] * x * e2;
}

void test_fit_custom_model(void) {
    const double true_p[] = {60.0, 1.0, 30.0, 0.1};
    histo_t *h = histo_create_uniform(60, 0.0, 10.0, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h);

    for (uint32_t i = 0; i < 60; ++i) {
        double xc = 0.0;
        histo_bin_center(h, i, &xc);
        double val = custom_double_exp(xc, true_p, NULL);
        histo_fill_w(h, xc, val);
    }

    const double p0[] = {50.0, 0.8, 25.0, 0.15};

    /* 1. Finite differences */
    histo_fit_result_t *res_fd = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fit_custom(h, custom_double_exp, 4, p0, NULL, &res_fd));
    TEST_ASSERT_NOT_NULL(res_fd);
    TEST_ASSERT_TRUE(res_fd->converged);
    TEST_ASSERT_DOUBLE_WITHIN(2.0, true_p[0], res_fd->params[0]);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, true_p[1], res_fd->params[1]);
    TEST_ASSERT_DOUBLE_WITHIN(2.0, true_p[2], res_fd->params[2]);
    TEST_ASSERT_DOUBLE_WITHIN(0.02, true_p[3], res_fd->params[3]);
    histo_fit_result_destroy(res_fd);

    /* 2. Analytical gradient callback */
    histo_fit_options_t opts;
    histo_fit_options_init(&opts);
    opts.grad_fn = custom_double_exp_grad;

    histo_fit_result_t *res_grad = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fit_custom(h, custom_double_exp, 4, p0, &opts, &res_grad));
    TEST_ASSERT_NOT_NULL(res_grad);
    TEST_ASSERT_TRUE(res_grad->converged);
    TEST_ASSERT_DOUBLE_WITHIN(2.0, true_p[0], res_grad->params[0]);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, true_p[1], res_grad->params[1]);
    histo_fit_result_destroy(res_grad);

    histo_destroy(h);
}

/* -------------------------------------------------------------------------
 * 3. Box Constraints & Fixed Parameters
 * ------------------------------------------------------------------------- */

void test_fit_box_constraints(void) {
    /* True peak is at mu = 5.0, but lower bound restricts mu >= 6.0 */
    const double true_params[] = {100.0, 5.0, 1.0};
    histo_t *h = create_synthetic_histo(80, 0.0, 10.0, HISTO_FIT_MODEL_GAUSSIAN, true_params, 3);
    TEST_ASSERT_NOT_NULL(h);

    double lower_bounds[] = {0.0, 6.0, 0.1};
    double upper_bounds[] = {500.0, 10.0, 5.0};

    histo_fit_options_t opts;
    histo_fit_options_init(&opts);
    opts.lower_bounds = lower_bounds;
    opts.upper_bounds = upper_bounds;

    const double p0[] = {80.0, 6.5, 1.2};
    histo_fit_result_t *res = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fit_model(h, HISTO_FIT_MODEL_GAUSSIAN, p0, &opts, &res));
    TEST_ASSERT_NOT_NULL(res);

    /* Parameter 1 (mu) must respect lower bound 6.0 */
    TEST_ASSERT_TRUE(res->params[1] >= 6.0 - 1e-9);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, 6.0, res->params[1]);

    histo_fit_result_destroy(res);
    histo_destroy(h);
}

void test_fit_fixed_parameters(void) {
    const double true_params[] = {150.0, 4.5, 1.5};
    histo_t *h = create_synthetic_histo(70, 0.0, 10.0, HISTO_FIT_MODEL_GAUSSIAN, true_params, 3);
    TEST_ASSERT_NOT_NULL(h);

    /* Freeze mu at 4.5 */
    bool fixed[] = {false, true, false};
    histo_fit_options_t opts;
    histo_fit_options_init(&opts);
    opts.fixed_params = fixed;

    const double p0[] = {100.0, 4.5, 2.0};
    histo_fit_result_t *res = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fit_model(h, HISTO_FIT_MODEL_GAUSSIAN, p0, &opts, &res));
    TEST_ASSERT_NOT_NULL(res);
    TEST_ASSERT_TRUE(res->converged);

    /* Frozen parameter must remain exactly 4.5 */
    TEST_ASSERT_EQUAL_DOUBLE(4.5, res->params[1]);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, res->param_errors[1]);
    TEST_ASSERT_EQUAL_INT(70 - 2, res->ndf); /* 2 free params */

    TEST_ASSERT_DOUBLE_WITHIN(2.0, true_params[0], res->params[0]);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, true_params[2], res->params[2]);

    histo_fit_result_destroy(res);
    histo_destroy(h);
}

/* -------------------------------------------------------------------------
 * 4. Poisson MLE vs Chi2 Loss on Low-Count / Sparse Histograms
 * ------------------------------------------------------------------------- */

void test_fit_poisson_mle_sparse(void) {
    histo_t *h = histo_create_uniform(20, 0.0, 10.0, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h);

    /* Sparse low counts: mostly 0, 1, 2 entries */
    histo_fill(h, 4.5);
    histo_fill(h, 4.8);
    histo_fill(h, 5.0);
    histo_fill(h, 5.1);
    histo_fill(h, 5.2);
    histo_fill(h, 5.5);

    histo_fit_options_t opts;
    histo_fit_options_init(&opts);
    opts.loss_type = HISTO_FIT_LOSS_POISSON_MLE;

    const double p0[] = {5.0, 5.0, 1.0};
    histo_fit_result_t *res = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fit_model(h, HISTO_FIT_MODEL_GAUSSIAN, p0, &opts, &res));
    TEST_ASSERT_NOT_NULL(res);
    TEST_ASSERT_TRUE(res->converged);
    TEST_ASSERT_DOUBLE_WITHIN(0.5, 5.0, res->params[1]); /* Center near 5.0 */

    histo_fit_result_destroy(res);
    histo_destroy(h);
}

/* -------------------------------------------------------------------------
 * 5. Range Sub-window Fitting
 * ------------------------------------------------------------------------- */

void test_fit_sub_range(void) {
    const double true_params[] = {100.0, 25.0, 3.0};
    histo_t *h = create_synthetic_histo(100, 0.0, 100.0, HISTO_FIT_MODEL_GAUSSIAN, true_params, 3);
    TEST_ASSERT_NOT_NULL(h);

    histo_fit_options_t opts;
    histo_fit_options_init(&opts);
    opts.range_min = 15.0;
    opts.range_max = 35.0;

    histo_fit_result_t *res = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_fit_model(h, HISTO_FIT_MODEL_GAUSSIAN, NULL, &opts, &res));
    TEST_ASSERT_NOT_NULL(res);
    TEST_ASSERT_TRUE(res->converged);
    TEST_ASSERT_DOUBLE_WITHIN(0.1, 25.0, res->params[1]);

    /* Number of bins in [15, 35] out of 100 bins on [0, 100] is 20 */
    TEST_ASSERT_EQUAL_INT(20 - 3, res->ndf);

    histo_fit_result_destroy(res);
    histo_destroy(h);
}

/* -------------------------------------------------------------------------
 * 6. Chi2 CDF & p-value Calculations
 * ------------------------------------------------------------------------- */

void test_fit_p_value_math(void) {
    /* Extreme limits */
    TEST_ASSERT_EQUAL_DOUBLE(1.0, histo_fit_chi2_p_value(0.0, 10));
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, histo_fit_chi2_p_value(1000.0, 5));

    /* Invalid arguments */
    TEST_ASSERT_TRUE(isnan(histo_fit_chi2_p_value(-1.0, 10)));
    TEST_ASSERT_TRUE(isnan(histo_fit_chi2_p_value(5.0, 0)));
    TEST_ASSERT_TRUE(isnan(histo_fit_chi2_p_value(5.0, -2)));
    TEST_ASSERT_TRUE(isnan(histo_fit_chi2_p_value(NAN, 5)));

    /* Known Chi2 distribution upper tail values */
    /* For ndf = 1, chi2 = 3.84146 -> p ~ 0.05 */
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.05, histo_fit_chi2_p_value(3.84146, 1));
    /* For ndf = 2, chi2 = 5.99146 -> p ~ 0.05 */
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.05, histo_fit_chi2_p_value(5.99146, 2));
    /* For ndf = 10, chi2 = 18.307 -> p ~ 0.05 */
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.05, histo_fit_chi2_p_value(18.307, 10));
}

/* -------------------------------------------------------------------------
 * 7. Pathological & Edge Cases
 * ------------------------------------------------------------------------- */

void test_fit_edge_cases_and_errors(void) {
    histo_fit_result_t *res = NULL;

    /* NULL inputs */
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_fit_model(NULL, HISTO_FIT_MODEL_GAUSSIAN, NULL, NULL, &res));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_fit_options_init(NULL));

    histo_t *h = histo_create_uniform(10, 0.0, 10.0, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h);

    /* NULL result pointer */
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_fit_model(h, HISTO_FIT_MODEL_GAUSSIAN, NULL, NULL, NULL));

    /* Custom model with NULL callback or NULL initial parameters */
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_fit_custom(h, NULL, 2, NULL, NULL, &res));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_fit_custom(h, custom_double_exp, 4, NULL, NULL, &res));

    /* Empty histogram */
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo_fit_model(h, HISTO_FIT_MODEL_GAUSSIAN, NULL, NULL, &res));

    /* Polynomial degree boundary check (> 10 rejected) */
    histo_fill(h, 5.0);
    histo_fit_options_t opts;
    histo_fit_options_init(&opts);
    opts.poly_degree = 15;
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_fit_model(h, HISTO_FIT_MODEL_POLYNOMIAL, NULL, &opts, &res));

    /* Destroy NULL is safe */
    histo_fit_result_destroy(NULL);

    histo_destroy(h);
}

void test_fit_adversarial_and_pathological(void) {
    /* 1. Model evaluation on special IEEE numbers and invalid models */
    double pars[] = { 100.0, 5.0, 1.0 };
    double ev_nan = histo_fit_eval(HISTO_FIT_MODEL_GAUSSIAN, pars, 3, NAN);
    TEST_ASSERT_TRUE(isnan(ev_nan) || ev_nan == 0.0);

    double ev_inf = histo_fit_eval(HISTO_FIT_MODEL_GAUSSIAN, pars, 3, INFINITY);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, ev_inf);

    double ev_inv = histo_fit_eval((histo_fit_model_t)999, pars, 3, 5.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, ev_inv);

    /* 2. Fit on 3 bins with 3 parameters (NDF = 0) */
    histo_t *h3 = histo_create_uniform(3, 0.0, 3.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h3);
    histo_fill_w(h3, 0.5, 10.0);
    histo_fill_w(h3, 1.5, 50.0);
    histo_fill_w(h3, 2.5, 10.0);

    histo_fit_result_t *res3 = NULL;
    histo_status_t st = histo_fit_model(h3, HISTO_FIT_MODEL_GAUSSIAN, NULL, NULL, &res3);
    if (st == HISTO_OK && res3 != NULL) {
        TEST_ASSERT_EQUAL_INT(0, res3->ndf);
        histo_fit_result_destroy(res3);
    }
    histo_destroy(h3);

    /* 3. Inverted bounds lb > ub */
    histo_t *h_bnd = histo_create_uniform(20, 0.0, 20.0, HISTO_FLAG_TRACK_SUMW2);
    for (int i = 0; i < 20; ++i) histo_fill(h_bnd, (double)i);

    double lb[] = { 100.0, 10.0, 5.0 };
    double ub[] = { 10.0, 1.0, 1.0 }; /* Inverted */
    histo_fit_options_t opts;
    histo_fit_options_init(&opts);
    opts.lower_bounds = lb;
    opts.upper_bounds = ub;

    histo_fit_result_t *res_bnd = NULL;
    /* Must not crash */
    histo_status_t st_bnd = histo_fit_model(h_bnd, HISTO_FIT_MODEL_GAUSSIAN, NULL, &opts, &res_bnd);
    if (st_bnd == HISTO_OK && res_bnd) {
        histo_fit_result_destroy(res_bnd);
    }
    histo_destroy(h_bnd);
}

void test_fit_lognormal_recovery(void) {
    const double true_params[] = {500.0, 1.5, 0.4}; /* A, mu, sigma */
    histo_t *h = create_synthetic_histo(100, 0.1, 15.0, HISTO_FIT_MODEL_LOG_NORMAL, true_params, 3);
    TEST_ASSERT_NOT_NULL(h);

    histo_fit_result_t *res = NULL;
    histo_status_t status = histo_fit_model(h, HISTO_FIT_MODEL_LOG_NORMAL, NULL, NULL, &res);
    TEST_ASSERT_EQUAL(HISTO_OK, status);
    TEST_ASSERT_NOT_NULL(res);
    TEST_ASSERT_TRUE(res->converged);

    TEST_ASSERT_DOUBLE_WITHIN(5.0, true_params[0], res->params[0]);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, true_params[1], res->params[1]);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, true_params[2], res->params[2]);

    histo_fit_result_destroy(res);
    histo_destroy(h);
}

void test_fit_gaussian_plus_linear_recovery(void) {
    const double true_params[] = {80.0, 5.0, 0.8, 10.0, 0.5}; /* A, mu, sigma, c0, c1 */
    histo_t *h = create_synthetic_histo(100, 0.0, 10.0, HISTO_FIT_MODEL_GAUSSIAN_PLUS_LINEAR, true_params, 5);
    TEST_ASSERT_NOT_NULL(h);

    histo_fit_result_t *res = NULL;
    histo_status_t status = histo_fit_model(h, HISTO_FIT_MODEL_GAUSSIAN_PLUS_LINEAR, NULL, NULL, &res);
    TEST_ASSERT_EQUAL(HISTO_OK, status);
    TEST_ASSERT_NOT_NULL(res);
    TEST_ASSERT_TRUE(res->converged);

    TEST_ASSERT_DOUBLE_WITHIN(2.0, true_params[0], res->params[0]);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, true_params[1], res->params[1]);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, true_params[2], res->params[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, true_params[3], res->params[3]);
    TEST_ASSERT_DOUBLE_WITHIN(0.1, true_params[4], res->params[4]);

    histo_fit_result_destroy(res);
    histo_destroy(h);
}

void test_fit_weibull_recovery(void) {
    const double true_params[] = {300.0, 2.2, 4.0}; /* A, k, lambda */
    histo_t *h = create_synthetic_histo(80, 0.1, 12.0, HISTO_FIT_MODEL_WEIBULL, true_params, 3);
    TEST_ASSERT_NOT_NULL(h);

    histo_fit_result_t *res = NULL;
    histo_status_t status = histo_fit_model(h, HISTO_FIT_MODEL_WEIBULL, NULL, NULL, &res);
    TEST_ASSERT_EQUAL(HISTO_OK, status);
    TEST_ASSERT_NOT_NULL(res);
    TEST_ASSERT_TRUE(res->converged);

    TEST_ASSERT_DOUBLE_WITHIN(5.0, true_params[0], res->params[0]);
    TEST_ASSERT_DOUBLE_WITHIN(0.1, true_params[1], res->params[1]);
    TEST_ASSERT_DOUBLE_WITHIN(0.1, true_params[2], res->params[2]);

    histo_fit_result_destroy(res);
    histo_destroy(h);
}

void test_fit_gamma_recovery(void) {
    const double true_params[] = {250.0, 3.5, 1.2}; /* A, k, theta */
    histo_t *h = create_synthetic_histo(100, 0.1, 15.0, HISTO_FIT_MODEL_GAMMA, true_params, 3);
    TEST_ASSERT_NOT_NULL(h);

    histo_fit_result_t *res = NULL;
    histo_status_t status = histo_fit_model(h, HISTO_FIT_MODEL_GAMMA, NULL, NULL, &res);
    TEST_ASSERT_EQUAL(HISTO_OK, status);
    TEST_ASSERT_NOT_NULL(res);
    TEST_ASSERT_TRUE(res->converged);

    TEST_ASSERT_DOUBLE_WITHIN(5.0, true_params[0], res->params[0]);
    TEST_ASSERT_DOUBLE_WITHIN(0.15, true_params[1], res->params[1]);
    TEST_ASSERT_DOUBLE_WITHIN(0.15, true_params[2], res->params[2]);

    histo_fit_result_destroy(res);
    histo_destroy(h);
}

void test_fit_poisson_recovery(void) {
    const double true_params[] = {100.0, 4.5}; /* A, lambda */
    histo_t *h = create_synthetic_histo(30, 0.0, 15.0, HISTO_FIT_MODEL_POISSON, true_params, 2);
    TEST_ASSERT_NOT_NULL(h);

    histo_fit_result_t *res = NULL;
    histo_status_t status = histo_fit_model(h, HISTO_FIT_MODEL_POISSON, NULL, NULL, &res);
    TEST_ASSERT_EQUAL(HISTO_OK, status);
    TEST_ASSERT_NOT_NULL(res);
    TEST_ASSERT_TRUE(res->converged);

    TEST_ASSERT_DOUBLE_WITHIN(2.0, true_params[0], res->params[0]);
    TEST_ASSERT_DOUBLE_WITHIN(0.1, true_params[1], res->params[1]);

    histo_fit_result_destroy(res);
    histo_destroy(h);
}

void test_fit_laplace_recovery(void) {
    const double true_params[] = {150.0, 5.0, 1.2}; /* A, mu, b */
    histo_t *h = create_synthetic_histo(100, 0.0, 10.0, HISTO_FIT_MODEL_LAPLACE, true_params, 3);
    TEST_ASSERT_NOT_NULL(h);

    histo_fit_result_t *res = NULL;
    histo_status_t status = histo_fit_model(h, HISTO_FIT_MODEL_LAPLACE, NULL, NULL, &res);
    TEST_ASSERT_EQUAL(HISTO_OK, status);
    TEST_ASSERT_NOT_NULL(res);
    TEST_ASSERT_TRUE(res->converged);

    TEST_ASSERT_DOUBLE_WITHIN(3.0, true_params[0], res->params[0]);
    TEST_ASSERT_DOUBLE_WITHIN(0.1, true_params[1], res->params[1]);
    TEST_ASSERT_DOUBLE_WITHIN(0.1, true_params[2], res->params[2]);

    histo_fit_result_destroy(res);
    histo_destroy(h);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_fit_gaussian_recovery);
    RUN_TEST(test_fit_exponential_decay);
    RUN_TEST(test_fit_polynomial_linear_and_quadratic);
    RUN_TEST(test_fit_breit_wigner);
    RUN_TEST(test_fit_power_law);
    RUN_TEST(test_fit_lognormal_recovery);
    RUN_TEST(test_fit_gaussian_plus_linear_recovery);
    RUN_TEST(test_fit_weibull_recovery);
    RUN_TEST(test_fit_gamma_recovery);
    RUN_TEST(test_fit_poisson_recovery);
    RUN_TEST(test_fit_laplace_recovery);
    RUN_TEST(test_fit_custom_model);
    RUN_TEST(test_fit_box_constraints);
    RUN_TEST(test_fit_fixed_parameters);
    RUN_TEST(test_fit_poisson_mle_sparse);
    RUN_TEST(test_fit_sub_range);
    RUN_TEST(test_fit_p_value_math);
    RUN_TEST(test_fit_edge_cases_and_errors);
    RUN_TEST(test_fit_adversarial_and_pathological);

    return UNITY_END();
}

