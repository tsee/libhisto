/*
 * Unit tests for Kernel Density Estimation: kernels, CDF, and sampling.
 */

#include "unity.h"
#include "histo/kde.h"
#include "histo/histo.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

void setUp(void) {}
void tearDown(void) {}

void test_kde_gaussian_pdf_cdf(void) {
    const size_t n = 2000;
    double *vals = (double*)malloc(n * sizeof(double));
    TEST_ASSERT_NOT_NULL(vals);

    uint64_t state = 0x123456789ULL;
    for (size_t i = 0; i < n; i += 2) {
        uint64_t z1 = (state += 0x9e3779b97f4a7c15ULL);
        z1 = (z1 ^ (z1 >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z1 = (z1 ^ (z1 >> 27)) * 0x94d049bb133111ebULL;
        double u1 = (double)((z1 ^ (z1 >> 31)) >> 11) * (1.0 / 9007199254740992.0);

        uint64_t z2 = (state += 0x9e3779b97f4a7c15ULL);
        z2 = (z2 ^ (z2 >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z2 = (z2 ^ (z2 >> 27)) * 0x94d049bb133111ebULL;
        double u2 = (double)((z2 ^ (z2 >> 31)) >> 11) * (1.0 / 9007199254740992.0);

        if (u1 <= 1e-15) u1 = 1e-15;
        double r = sqrt(-2.0 * log(u1));
        double theta = 2.0 * 3.141592653589793 * u2;
        vals[i] = r * cos(theta);
        if (i + 1 < n) {
            vals[i + 1] = r * sin(theta);
        }
    }

    histo_kde_options_t opts = histo_kde_default_options();
    opts.kernel = HISTO_KDE_KERNEL_GAUSSIAN;
    opts.bw_method = HISTO_KDE_BANDWIDTH_SILVERMAN;

    histo_kde_t *kde = histo_kde_create(n, vals, NULL, &opts);
    TEST_ASSERT_NOT_NULL(kde);
    TEST_ASSERT_EQUAL(HISTO_KDE_KERNEL_GAUSSIAN, histo_kde_get_kernel(kde));
    TEST_ASSERT_EQUAL_UINT64(n, histo_kde_num_points(kde));

    double bw = histo_kde_get_bandwidth(kde);
    TEST_ASSERT_TRUE(bw >= 0.10 && bw <= 0.35);

    /* Test PDF at center */
    double pdf_0 = histo_kde_eval(kde, 0.0);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, 0.3989, pdf_0);

    /* Test batch eval */
    double x_test[3] = { -1.0, 0.0, 1.0 };
    double pdf_test[3] = { 0 };
    TEST_ASSERT_EQUAL(HISTO_OK, histo_kde_eval_n(kde, 3, x_test, pdf_test));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, pdf_0, pdf_test[1]);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, pdf_test[0], pdf_test[2]);

    /* Test numerical integration */
    double integral = 0.0;
    double dx = 0.01;
    for (double x = -6.0; x <= 6.0; x += dx) {
        integral += histo_kde_eval(kde, x) * dx;
    }
    TEST_ASSERT_DOUBLE_WITHIN(0.02, 1.0, integral);

    /* Test CDF */
    double cdf_neg_inf = histo_kde_cdf(kde, -10.0);
    double cdf_pos_inf = histo_kde_cdf(kde, 10.0);
    double cdf_mid = histo_kde_cdf(kde, 0.0);

    TEST_ASSERT_TRUE(cdf_neg_inf < 1e-4);
    TEST_ASSERT_TRUE(cdf_pos_inf > 0.9999);
    TEST_ASSERT_DOUBLE_WITHIN(0.03, 0.50, cdf_mid);

    /* Test Quantile */
    double q50 = 0.0, q95 = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_kde_quantile(kde, 0.50, &q50));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_kde_quantile(kde, 0.95, &q95));
    TEST_ASSERT_DOUBLE_WITHIN(0.05, 0.0, q50);
    TEST_ASSERT_DOUBLE_WITHIN(0.15, 1.645, q95);

    /* Test sampling */
    double samples[500];
    TEST_ASSERT_EQUAL(HISTO_OK, histo_kde_sample(kde, 500, samples, 12345));
    double sample_mean = 0.0;
    for (int i = 0; i < 500; i++) sample_mean += samples[i];
    sample_mean /= 500.0;
    TEST_ASSERT_DOUBLE_WITHIN(0.15, 0.0, sample_mean);

    histo_kde_destroy(kde);
    free(vals);
}

void test_kde_all_kernels(void) {
    double data[] = { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0 };
    histo_kde_kernel_t kernels[] = {
        HISTO_KDE_KERNEL_GAUSSIAN,
        HISTO_KDE_KERNEL_EPANECHNIKOV,
        HISTO_KDE_KERNEL_UNIFORM,
        HISTO_KDE_KERNEL_TRIANGULAR,
        HISTO_KDE_KERNEL_BIWEIGHT,
        HISTO_KDE_KERNEL_COSINE
    };

    for (size_t i = 0; i < sizeof(kernels)/sizeof(kernels[0]); i++) {
        histo_kde_options_t opts = histo_kde_default_options();
        opts.kernel = kernels[i];
        opts.bandwidth = 1.0;
        opts.bw_method = HISTO_KDE_BANDWIDTH_MANUAL;

        histo_kde_t *kde = histo_kde_create(10, data, NULL, &opts);
        TEST_ASSERT_NOT_NULL(kde);
        TEST_ASSERT_EQUAL(kernels[i], histo_kde_get_kernel(kde));

        double pdf = histo_kde_eval(kde, 5.5);
        TEST_ASSERT_TRUE(pdf > 0.0);

        double cdf = histo_kde_cdf(kde, 5.5);
        TEST_ASSERT_TRUE(cdf >= 0.40 && cdf <= 0.60);

        histo_kde_destroy(kde);
    }
}

void test_kde_from_histogram(void) {
    histo_t *h = histo_create_uniform(50, -5.0, 5.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h);

    for (int i = 0; i < 1000; i++) {
        double v = ((double)rand() / (double)RAND_MAX) * 4.0 - 2.0;
        histo_fill(h, v);
    }

    histo_kde_t *kde = histo_kde_create_from_histo(h, NULL);
    TEST_ASSERT_NOT_NULL(kde);
    TEST_ASSERT_TRUE(histo_kde_get_bandwidth(kde) > 0.0);

    double pdf_center = histo_kde_eval(kde, 0.0);
    TEST_ASSERT_TRUE(pdf_center > 0.15 && pdf_center < 0.35);

    histo_kde_destroy(kde);
    histo_destroy(h);
}

void test_kde_edge_cases(void) {
    TEST_ASSERT_NULL(histo_kde_create(0, NULL, NULL, NULL));
    TEST_ASSERT_NULL(histo_kde_create_from_histo(NULL, NULL));

    double single[] = { 5.0 };
    histo_kde_t *kde_single = histo_kde_create(1, single, NULL, NULL);
    TEST_ASSERT_NOT_NULL(kde_single);
    TEST_ASSERT_TRUE(histo_kde_eval(kde_single, 5.0) > 0.0);
    histo_kde_destroy(kde_single);

    double with_nans[] = { NAN, 1.0, 2.0, INFINITY };
    histo_kde_t *kde_nan = histo_kde_create(4, with_nans, NULL, NULL);
    TEST_ASSERT_NOT_NULL(kde_nan);
    TEST_ASSERT_EQUAL_UINT64(2, histo_kde_num_points(kde_nan));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, histo_kde_eval(kde_nan, NAN));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, histo_kde_cdf(kde_nan, -INFINITY));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, histo_kde_cdf(kde_nan, INFINITY));
    histo_kde_destroy(kde_nan);
}

void test_kde_adversarial_datasets(void) {
    /* 1. All identical samples (zero variance) */
    double ident[100];
    for (int i = 0; i < 100; ++i) ident[i] = 42.0;
    histo_kde_t *kde_ident = histo_kde_create(100, ident, NULL, NULL);
    TEST_ASSERT_NOT_NULL(kde_ident);
    TEST_ASSERT_TRUE(histo_kde_get_bandwidth(kde_ident) > 0.0);
    double pdf_at_val = histo_kde_eval(kde_ident, 42.0);
    TEST_ASSERT_TRUE(pdf_at_val > 0.0);
    double cdf_at_val = histo_kde_cdf(kde_ident, 42.0);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, 0.50, cdf_at_val);
    histo_kde_destroy(kde_ident);

    /* 2. Subnormal samples */
    double subn[10];
    for (int i = 0; i < 10; ++i) subn[i] = (double)(i + 1) * 1e-315;
    histo_kde_t *kde_subn = histo_kde_create(10, subn, NULL, NULL);
    TEST_ASSERT_NOT_NULL(kde_subn);
    TEST_ASSERT_TRUE(histo_kde_get_bandwidth(kde_subn) > 0.0);
    histo_kde_destroy(kde_subn);

    /* 3. Quantile boundary and error inputs */
    double normal_data[50];
    for (int i = 0; i < 50; ++i) normal_data[i] = (double)i;
    histo_kde_t *kde_norm = histo_kde_create(50, normal_data, NULL, NULL);
    TEST_ASSERT_NOT_NULL(kde_norm);

    double q_val = 0;
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_kde_quantile(NULL, 0.5, &q_val));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_kde_quantile(kde_norm, -0.01, &q_val));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_kde_quantile(kde_norm, 1.01, &q_val));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_kde_quantile(kde_norm, NAN, &q_val));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_kde_quantile(kde_norm, 0.5, NULL));

    /* q = 0.0 and q = 1.0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_kde_quantile(kde_norm, 0.0, &q_val));
    TEST_ASSERT_TRUE(q_val < 0.0);
    TEST_ASSERT_EQUAL(HISTO_OK, histo_kde_quantile(kde_norm, 1.0, &q_val));
    TEST_ASSERT_TRUE(q_val > 49.0);

    /* 4. Batch evaluation invalid args */
    double out_eval[5];
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_kde_eval_n(NULL, 5, normal_data, out_eval));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_kde_eval_n(kde_norm, 5, NULL, out_eval));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_kde_eval_n(kde_norm, 5, normal_data, NULL));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_kde_eval_n(kde_norm, 0, normal_data, out_eval));

    /* 5. Sampling invalid args */
    double smp[10];
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_kde_sample(NULL, 10, smp, 123));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_kde_sample(kde_norm, 10, NULL, 123));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_kde_sample(kde_norm, 0, smp, 123));

    histo_kde_destroy(kde_norm);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_kde_gaussian_pdf_cdf);
    RUN_TEST(test_kde_all_kernels);
    RUN_TEST(test_kde_from_histogram);
    RUN_TEST(test_kde_edge_cases);
    RUN_TEST(test_kde_adversarial_datasets);
    return UNITY_END();
}

