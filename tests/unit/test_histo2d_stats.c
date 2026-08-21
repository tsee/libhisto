#include "unity.h"
#include "histo/histo2d.h"
#include "histo/histo.h"
#include <math.h>
#include <stdlib.h>
#include <float.h>

void setUp(void) {}
void tearDown(void) {}

/* Helper to generate standard normal random variates via Box-Muller */
static void generate_gaussian_pair(double mu_x, double sigma_x,
                                   double mu_y, double sigma_y,
                                   double rho,
                                   double *out_x, double *out_y)
{
    double u1 = ((double)rand() + 1.0) / ((double)RAND_MAX + 2.0);
    double u2 = ((double)rand() + 1.0) / ((double)RAND_MAX + 2.0);

    double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * 3.14159265358979323846 * u2);
    double z1 = sqrt(-2.0 * log(u1)) * sin(2.0 * 3.14159265358979323846 * u2);

    /* Correlate: X = z0, Y = rho * z0 + sqrt(1 - rho^2) * z1 */
    double x_std = z0;
    double y_std = rho * z0 + sqrt(1.0 - rho * rho) * z1;

    *out_x = mu_x + sigma_x * x_std;
    *out_y = mu_y + sigma_y * y_std;
}

/* -------------------------------------------------------------------------
 * 1. 2D Online Moments & Bivariate Statistics (Welford)
 * ------------------------------------------------------------------------- */

void test_histo2d_online_welford_correlation(void) {
    srand(42);
    const size_t N = 10000;
    const double true_mu_x = 50.0;
    const double true_sigma_x = 10.0;
    const double true_mu_y = 100.0;
    const double true_sigma_y = 20.0;
    const double true_rho = 0.75;

    histo2d_t *h = histo2d_create_uniform(100, 0.0, 100.0, 100, 0.0, 200.0,
                                          HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    TEST_ASSERT_NOT_NULL(h);

    for (size_t i = 0; i < N; ++i) {
        double x, y;
        generate_gaussian_pair(true_mu_x, true_sigma_x, true_mu_y, true_sigma_y, true_rho, &x, &y);
        histo2d_fill(h, x, y);
    }

    double mx, my, var_x, var_y, std_x, std_y, cov, rho;
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_mean_x(h, &mx));
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_mean_y(h, &my));
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_variance_x(h, &var_x));
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_variance_y(h, &var_y));
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_std_dev_x(h, &std_x));
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_std_dev_y(h, &std_y));
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_covariance(h, &cov));
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_correlation(h, &rho));

    /* Verify recovery within statistical margin */
    TEST_ASSERT_DOUBLE_WITHIN(1.0, true_mu_x, mx);
    TEST_ASSERT_DOUBLE_WITHIN(2.0, true_mu_y, my);
    TEST_ASSERT_DOUBLE_WITHIN(20.0, true_sigma_x * true_sigma_x, var_x);
    TEST_ASSERT_DOUBLE_WITHIN(40.0, true_sigma_y * true_sigma_y, var_y);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, true_sigma_x, std_x);
    TEST_ASSERT_DOUBLE_WITHIN(2.0, true_sigma_y, std_y);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, true_rho, rho);

    /* Verify summary struct */
    histo2d_stats_t stats;
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_get_stats(h, &stats));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, mx, stats.mean_x);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, my, stats.mean_y);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, rho, stats.correlation);

    histo2d_destroy(h);
}

void test_histo2d_negative_correlation(void) {
    srand(123);
    const size_t N = 8000;
    const double true_rho = -0.60;

    histo2d_t *h = histo2d_create_uniform(80, 0.0, 80.0, 80, 0.0, 80.0,
                                          HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);

    for (size_t i = 0; i < N; ++i) {
        double x, y;
        generate_gaussian_pair(40.0, 8.0, 40.0, 8.0, true_rho, &x, &y);
        histo2d_fill(h, x, y);
    }

    double rho;
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_correlation(h, &rho));
    TEST_ASSERT_DOUBLE_WITHIN(0.06, true_rho, rho);

    histo2d_destroy(h);
}

/* -------------------------------------------------------------------------
 * 2. 1D Marginal Projections, Slices & Profiles
 * ------------------------------------------------------------------------- */

void test_histo2d_projections_and_slices(void) {
    histo2d_t *h = histo2d_create_uniform(4, 0.0, 4.0, 3, 0.0, 3.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h);

    /* Fill grid:
     *   Row 0 (ix=0): [1.0, 2.0, 3.0] -> sum = 6.0
     *   Row 1 (ix=1): [4.0, 5.0, 6.0] -> sum = 15.0
     *   Row 2 (ix=2): [7.0, 8.0, 9.0] -> sum = 24.0
     *   Row 3 (ix=3): [10.0, 11.0, 12.0] -> sum = 33.0
     * Column sums:
     *   Col 0 (iy=0): 1 + 4 + 7 + 10 = 22.0
     *   Col 1 (iy=1): 2 + 5 + 8 + 11 = 26.0
     *   Col 2 (iy=2): 3 + 6 + 9 + 12 = 30.0
     */
    double val = 1.0;
    for (uint32_t ix = 0; ix < 4; ++ix) {
        for (uint32_t iy = 0; iy < 3; ++iy) {
            histo2d_fill_bin(h, ix, iy, val++);
        }
    }

    /* 1. Project X */
    histo_t *px = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_project_x(h, &px));
    TEST_ASSERT_NOT_NULL(px);
    TEST_ASSERT_EQUAL_UINT32(4, histo_nbins(px));

    double c;
    histo_bin_content(px, 0, &c); TEST_ASSERT_DOUBLE_WITHIN(1e-9, 6.0, c);
    histo_bin_content(px, 1, &c); TEST_ASSERT_DOUBLE_WITHIN(1e-9, 15.0, c);
    histo_bin_content(px, 2, &c); TEST_ASSERT_DOUBLE_WITHIN(1e-9, 24.0, c);
    histo_bin_content(px, 3, &c); TEST_ASSERT_DOUBLE_WITHIN(1e-9, 33.0, c);
    histo_destroy(px);

    /* 2. Project Y */
    histo_t *py = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_project_y(h, &py));
    TEST_ASSERT_NOT_NULL(py);
    TEST_ASSERT_EQUAL_UINT32(3, histo_nbins(py));

    histo_bin_content(py, 0, &c); TEST_ASSERT_DOUBLE_WITHIN(1e-9, 22.0, c);
    histo_bin_content(py, 1, &c); TEST_ASSERT_DOUBLE_WITHIN(1e-9, 26.0, c);
    histo_bin_content(py, 2, &c); TEST_ASSERT_DOUBLE_WITHIN(1e-9, 30.0, c);
    histo_destroy(py);

    /* 3. Slice X (iy=1 to iy=2) */
    histo_t *sx = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_slice_x(h, 1, 2, &sx));
    TEST_ASSERT_NOT_NULL(sx);
    histo_bin_content(sx, 0, &c); TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0 + 3.0, c);     /* 5.0 */
    histo_bin_content(sx, 1, &c); TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0 + 6.0, c);     /* 11.0 */
    histo_bin_content(sx, 2, &c); TEST_ASSERT_DOUBLE_WITHIN(1e-9, 8.0 + 9.0, c);     /* 17.0 */
    histo_bin_content(sx, 3, &c); TEST_ASSERT_DOUBLE_WITHIN(1e-9, 11.0 + 12.0, c);   /* 23.0 */
    histo_destroy(sx);

    /* 4. Slice Y (ix=0 to ix=1) */
    histo_t *sy = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_slice_y(h, 0, 1, &sy));
    TEST_ASSERT_NOT_NULL(sy);
    histo_bin_content(sy, 0, &c); TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0 + 4.0, c); /* 5.0 */
    histo_bin_content(sy, 1, &c); TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0 + 5.0, c); /* 7.0 */
    histo_bin_content(sy, 2, &c); TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0 + 6.0, c); /* 9.0 */
    histo_destroy(sy);

    histo2d_destroy(h);
}

void test_histo2d_profiles(void) {
    /* Line y = 2x across x in [0, 5], y in [0, 10] */
    histo2d_t *h = histo2d_create_uniform(5, 0.0, 5.0, 10, 0.0, 10.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h);

    for (int i = 0; i < 5000; ++i) {
        double x = ((double)rand() / (double)RAND_MAX) * 5.0;
        double y = 2.0 * x + (((double)rand() / (double)RAND_MAX) - 0.5) * 0.4;
        if (y >= 0.0 && y < 10.0) {
            histo2d_fill(h, x, y);
        }
    }

    histo_t *prof_x = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_profile_x(h, &prof_x));
    TEST_ASSERT_NOT_NULL(prof_x);

    /* For bin 0: x in [0, 1], center = 0.5 -> expected y mean ~ 1.0 */
    double m0, m1, m2, m3, m4;
    histo_bin_content(prof_x, 0, &m0); TEST_ASSERT_DOUBLE_WITHIN(0.15, 1.0, m0);
    histo_bin_content(prof_x, 1, &m1); TEST_ASSERT_DOUBLE_WITHIN(0.15, 3.0, m1);
    histo_bin_content(prof_x, 2, &m2); TEST_ASSERT_DOUBLE_WITHIN(0.15, 5.0, m2);
    histo_bin_content(prof_x, 3, &m3); TEST_ASSERT_DOUBLE_WITHIN(0.15, 7.0, m3);
    histo_bin_content(prof_x, 4, &m4); TEST_ASSERT_DOUBLE_WITHIN(0.15, 9.0, m4);

    histo_destroy(prof_x);
    histo2d_destroy(h);
}

/* -------------------------------------------------------------------------
 * 3. 2D Transformations: Scaling, Normalization, Rebinning, Arithmetic
 * ------------------------------------------------------------------------- */

void test_histo2d_scaling_and_normalization(void) {
    histo2d_t *h = histo2d_create_uniform(4, 0.0, 4.0, 4, 0.0, 4.0, HISTO_FLAG_TRACK_SUMW2);
    histo2d_fill_w(h, 1.5, 1.5, 10.0);
    histo2d_fill_w(h, 2.5, 2.5, 30.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 40.0, histo2d_total_weight(h));

    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_scale(h, 0.5));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 20.0, histo2d_total_weight(h));
    double c;
    histo2d_bin_content(h, 1, 1, &c); TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0, c);

    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_normalize(h, 1.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, histo2d_total_weight(h));
    histo2d_bin_content(h, 1, 1, &c); TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.25, c);
    histo2d_bin_content(h, 2, 2, &c); TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.75, c);

    histo2d_destroy(h);
}

void test_histo2d_rebinning(void) {
    /* 4x4 -> 2x2 with factor_x=2, factor_y=2 */
    histo2d_t *h = histo2d_create_uniform(4, 0.0, 4.0, 4, 0.0, 4.0, HISTO_FLAG_TRACK_SUMW2);
    for (uint32_t ix = 0; ix < 4; ++ix) {
        for (uint32_t iy = 0; iy < 4; ++iy) {
            histo2d_fill_bin(h, ix, iy, 1.0);
        }
    }

    histo2d_t *rebinned = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_rebin(h, 2, 2, &rebinned));
    TEST_ASSERT_NOT_NULL(rebinned);
    TEST_ASSERT_EQUAL_UINT32(2, histo2d_nbins_x(rebinned));
    TEST_ASSERT_EQUAL_UINT32(2, histo2d_nbins_y(rebinned));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 16.0, histo2d_total_weight(rebinned));

    for (uint32_t ix = 0; ix < 2; ++ix) {
        for (uint32_t iy = 0; iy < 2; ++iy) {
            double c;
            histo2d_bin_content(rebinned, ix, iy, &c);
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.0, c); /* 2x2 sum = 4 */
        }
    }

    histo2d_destroy(rebinned);
    histo2d_destroy(h);
}

void test_histo2d_arithmetic(void) {
    histo2d_t *h1 = histo2d_create_uniform(2, 0.0, 2.0, 2, 0.0, 2.0, HISTO_FLAG_TRACK_SUMW2);
    histo2d_t *h2 = histo2d_create_uniform(2, 0.0, 2.0, 2, 0.0, 2.0, HISTO_FLAG_TRACK_SUMW2);

    histo2d_fill_bin(h1, 0, 0, 10.0);
    histo2d_fill_bin(h1, 0, 1, 20.0);
    histo2d_fill_bin(h2, 0, 0, 2.0);
    histo2d_fill_bin(h2, 0, 1, 4.0);

    /* Addition: h1 += 2.0 * h2 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_add(h1, h2, 2.0));
    double c;
    histo2d_bin_content(h1, 0, 0, &c); TEST_ASSERT_DOUBLE_WITHIN(1e-9, 14.0, c); /* 10 + 4 */
    histo2d_bin_content(h1, 0, 1, &c); TEST_ASSERT_DOUBLE_WITHIN(1e-9, 28.0, c); /* 20 + 8 */

    /* Subtraction: h1 -= h2 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_subtract(h1, h2));
    histo2d_bin_content(h1, 0, 0, &c); TEST_ASSERT_DOUBLE_WITHIN(1e-9, 12.0, c);
    histo2d_bin_content(h1, 0, 1, &c); TEST_ASSERT_DOUBLE_WITHIN(1e-9, 24.0, c);

    /* Multiplication: h1 *= h2 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_multiply(h1, h2));
    histo2d_bin_content(h1, 0, 0, &c); TEST_ASSERT_DOUBLE_WITHIN(1e-9, 24.0, c); /* 12 * 2 */
    histo2d_bin_content(h1, 0, 1, &c); TEST_ASSERT_DOUBLE_WITHIN(1e-9, 96.0, c); /* 24 * 4 */

    /* Division: h1 /= h2 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_divide(h1, h2));
    histo2d_bin_content(h1, 0, 0, &c); TEST_ASSERT_DOUBLE_WITHIN(1e-9, 12.0, c); /* 24 / 2 */
    histo2d_bin_content(h1, 0, 1, &c); TEST_ASSERT_DOUBLE_WITHIN(1e-9, 24.0, c); /* 96 / 4 */

    histo2d_destroy(h1);
    histo2d_destroy(h2);
}

void test_histo2d_degenerate_and_boundary_stats(void) {
    /* 1. Empty 2D histogram */
    histo2d_t *h_empty = histo2d_create_uniform(5, 0.0, 10.0, 5, 0.0, 10.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h_empty);

    double val = 0.0;
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo2d_mean_x(h_empty, &val));
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo2d_mean_y(h_empty, &val));
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo2d_variance_x(h_empty, &val));
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo2d_variance_y(h_empty, &val));
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo2d_covariance(h_empty, &val));
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo2d_correlation(h_empty, &val));

    /* NULL inputs check */
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo2d_mean_x(NULL, &val));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo2d_mean_x(h_empty, NULL));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo2d_get_stats(NULL, NULL));

    histo2d_destroy(h_empty);

    /* 2. Single sample 2D histogram */
    histo2d_t *h_single = histo2d_create_uniform(10, 0.0, 100.0, 10, 0.0, 100.0,
                                                 HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    TEST_ASSERT_NOT_NULL(h_single);
    histo2d_fill(h_single, 50.0, 75.0);

    double mx = 0, my = 0, vx = 0, vy = 0, cov = 0, rho = 0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_mean_x(h_single, &mx));
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_mean_y(h_single, &my));
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_variance_x(h_single, &vx));
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_variance_y(h_single, &vy));
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_covariance(h_single, &cov));
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_correlation(h_single, &rho));

    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 50.0, mx);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 75.0, my);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, vx);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, vy);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, cov);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, rho);

    histo2d_destroy(h_single);

    /* 3. Constant X, varying Y (zero variance on X axis) */
    histo2d_t *h_const_x = histo2d_create_uniform(10, 0.0, 100.0, 10, 0.0, 100.0,
                                                  HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    TEST_ASSERT_NOT_NULL(h_const_x);
    for (int i = 0; i < 50; ++i) {
        histo2d_fill(h_const_x, 42.0, (double)i * 2.0);
    }
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_variance_x(h_const_x, &vx));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, vx);
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_correlation(h_const_x, &rho));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, rho);
    histo2d_destroy(h_const_x);

    /* 4. Perfect diagonal correlation (+1.0) and anti-correlation (-1.0) */
    histo2d_t *h_diag = histo2d_create_uniform(100, 0.0, 100.0, 100, 0.0, 100.0,
                                               HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    histo2d_t *h_antidiag = histo2d_create_uniform(100, 0.0, 100.0, 100, 0.0, 100.0,
                                                   HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    for (int i = 1; i <= 90; ++i) {
        histo2d_fill(h_diag, (double)i, (double)i);
        histo2d_fill(h_antidiag, (double)i, 100.0 - (double)i);
    }
    double rho_pos = 0, rho_neg = 0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_correlation(h_diag, &rho_pos));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1.0, rho_pos);

    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_correlation(h_antidiag, &rho_neg));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, -1.0, rho_neg);

    histo2d_destroy(h_diag);
    histo2d_destroy(h_antidiag);

    /* 5. Out of range integral bounds */
    histo2d_t *h_grid = histo2d_create_uniform(5, 0.0, 10.0, 5, 0.0, 10.0, HISTO_FLAG_NONE);
    double integ = 0;
    TEST_ASSERT_EQUAL(HISTO_ERR_OUT_OF_RANGE, histo2d_integral_range(h_grid, 3, 1, 0, 2, &integ));
    TEST_ASSERT_EQUAL(HISTO_ERR_OUT_OF_RANGE, histo2d_integral_range(h_grid, 0, 5, 0, 2, &integ));
    TEST_ASSERT_EQUAL(HISTO_ERR_OUT_OF_RANGE, histo2d_integral_range(h_grid, 0, 2, 4, 1, &integ));
    TEST_ASSERT_EQUAL(HISTO_ERR_OUT_OF_RANGE, histo2d_integral_range(h_grid, 0, 2, 0, 5, &integ));
    histo2d_destroy(h_grid);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_histo2d_online_welford_correlation);
    RUN_TEST(test_histo2d_negative_correlation);
    RUN_TEST(test_histo2d_projections_and_slices);
    RUN_TEST(test_histo2d_profiles);
    RUN_TEST(test_histo2d_scaling_and_normalization);
    RUN_TEST(test_histo2d_rebinning);
    RUN_TEST(test_histo2d_arithmetic);
    RUN_TEST(test_histo2d_degenerate_and_boundary_stats);
    return UNITY_END();
}

