#include "unity.h"
#include "histo/histo.h"
#include <math.h>

void setUp(void) {}
void tearDown(void) {}

/* Test comparison of identical distributions */
void test_cmp_identical(void) {
    histo_t *h1 = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
    histo_t *h2 = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);

    for (uint32_t i = 0; i < 10; ++i) {
        double w = (double)(i + 1) * 2.5;
        histo_fill_bin(h1, i, w);
        histo_fill_bin(h2, i, w);
    }

    double chi2 = 0.0, ks = 0.0, wass = 0.0, kl = 0.0, bhatt = 0.0;
    uint32_t ndf = 0;

    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_chi2(h1, h2, &chi2, &ndf));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, chi2);
    TEST_ASSERT_EQUAL_UINT32(10, ndf);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_ks(h1, h2, &ks));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, ks);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_wasserstein_1d(h1, h2, &wass));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, wass);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_kl_divergence(h1, h2, &kl));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, kl);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_bhattacharyya(h1, h2, &bhatt));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, bhatt);

    histo_destroy(h1);
    histo_destroy(h2);
}

/* Test comparison of completely disjoint distributions */
void test_cmp_disjoint(void) {
    histo_t *h1 = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_NONE);
    histo_t *h2 = histo_create_uniform(10, 0.0, 100.0, HISTO_FLAG_NONE);

    /* h1 all in bin 0 ([0, 10)), h2 all in bin 9 ([90, 100)) */
    histo_fill_bin(h1, 0, 50.0);
    histo_fill_bin(h2, 9, 50.0);

    double ks = 0.0, wass = 0.0, bhatt = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_ks(h1, h2, &ks));
    /* Max CDF difference between step at 0 and step at 9 is 1.0 */
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, ks);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_wasserstein_1d(h1, h2, &wass));
    /* Wasserstein distance across 9 intermediate bins of width 10 is 9 * 10 = 90.0 */
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 90.0, wass);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_bhattacharyya(h1, h2, &bhatt));
    /* Zero overlap -> Bhattacharyya distance is +Inf */
    TEST_ASSERT_TRUE(isinf(bhatt) && bhatt > 0.0);

    histo_destroy(h1);
    histo_destroy(h2);
}

/* Test Chi-Square difference with weighted sum_w2 uncertainties */
void test_cmp_chi2_weighted(void) {
    histo_t *h1 = histo_create_uniform(4, 0.0, 40.0, HISTO_FLAG_TRACK_SUMW2);
    histo_t *h2 = histo_create_uniform(4, 0.0, 40.0, HISTO_FLAG_TRACK_SUMW2);

    /* Fill bin 0: h1=10 (w2=100), h2=14 (w2=196) -> diff = -4, var = 296, chi2 = 16 / 296 */
    histo_fill_w(h1, 5.0, 10.0);
    histo_fill_w(h2, 5.0, 14.0);

    double chi2 = 0.0;
    uint32_t ndf = 0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_chi2(h1, h2, &chi2, &ndf));
    TEST_ASSERT_EQUAL_UINT32(1, ndf);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 16.0 / 296.0, chi2);

    histo_destroy(h1);
    histo_destroy(h2);
}

/* Test comparison on variable binning */
void test_cmp_variable_bins(void) {
    double edges[5] = {0.0, 1.0, 5.0, 20.0, 100.0};
    histo_t *h1 = histo_create_variable(4, edges, HISTO_FLAG_NONE);
    histo_t *h2 = histo_create_variable(4, edges, HISTO_FLAG_NONE);

    histo_fill(h1, 0.5);
    histo_fill(h2, 0.5);

    double wass = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_wasserstein_1d(h1, h2, &wass));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, wass);

    histo_destroy(h1);
    histo_destroy(h2);
}

/* Test incompatible geometry rejection */
void test_cmp_incompatible(void) {
    histo_t *h1 = histo_create_uniform(5, 0.0, 50.0, HISTO_FLAG_NONE);
    histo_t *h2 = histo_create_uniform(10, 0.0, 50.0, HISTO_FLAG_NONE);
    histo_t *h3 = histo_create_uniform(5, 0.0, 100.0, HISTO_FLAG_NONE);

    histo_fill(h1, 10.0);
    histo_fill(h2, 10.0);
    histo_fill(h3, 10.0);

    double out = 0.0;
    uint32_t ndf = 0;

    /* Different bin counts */
    TEST_ASSERT_EQUAL(HISTO_ERR_INCOMPATIBLE, histo_cmp_chi2(h1, h2, &out, &ndf));
    TEST_ASSERT_EQUAL(HISTO_ERR_INCOMPATIBLE, histo_cmp_ks(h1, h2, &out));
    TEST_ASSERT_EQUAL(HISTO_ERR_INCOMPATIBLE, histo_cmp_wasserstein_1d(h1, h2, &out));
    TEST_ASSERT_EQUAL(HISTO_ERR_INCOMPATIBLE, histo_cmp_kl_divergence(h1, h2, &out));
    TEST_ASSERT_EQUAL(HISTO_ERR_INCOMPATIBLE, histo_cmp_bhattacharyya(h1, h2, &out));

    /* Different ranges */
    TEST_ASSERT_EQUAL(HISTO_ERR_INCOMPATIBLE, histo_cmp_chi2(h1, h3, &out, &ndf));

    histo_destroy(h1);
    histo_destroy(h2);
    histo_destroy(h3);
}

/* Test empty and NULL edge cases */
void test_cmp_empty_and_null(void) {
    histo_t *h1 = histo_create_uniform(5, 0.0, 50.0, HISTO_FLAG_NONE);
    histo_t *h2 = histo_create_uniform(5, 0.0, 50.0, HISTO_FLAG_NONE);

    double out = 0.0;
    uint32_t ndf = 0;

    /* Empty histograms */
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo_cmp_chi2(h1, h2, &out, &ndf));
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo_cmp_ks(h1, h2, &out));
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo_cmp_wasserstein_1d(h1, h2, &out));
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo_cmp_kl_divergence(h1, h2, &out));
    TEST_ASSERT_EQUAL(HISTO_ERR_EMPTY, histo_cmp_bhattacharyya(h1, h2, &out));

    /* NULL pointers */
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_cmp_chi2(NULL, h2, &out, &ndf));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_cmp_chi2(h1, NULL, &out, &ndf));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_cmp_ks(NULL, h2, &out));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_cmp_wasserstein_1d(NULL, h2, &out));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_cmp_kl_divergence(NULL, h2, &out));
    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_cmp_bhattacharyya(NULL, h2, &out));

    histo_destroy(h1);
    histo_destroy(h2);
}

/* Test all comparison metrics on variable binning histograms */
void test_cmp_variable_metrics(void) {
    const double edges[] = {0.0, 1.0, 5.0, 20.0, 100.0}; /* 4 variable bins */
    histo_t *h1 = histo_create_variable(4, edges, HISTO_FLAG_TRACK_SUMW2);
    histo_t *h2 = histo_create_variable(4, edges, HISTO_FLAG_TRACK_SUMW2);

    histo_fill_bin(h1, 0, 10.0);
    histo_fill_bin(h1, 1, 20.0);
    histo_fill_bin(h2, 0, 10.0);
    histo_fill_bin(h2, 1, 20.0);

    double chi2 = 0.0, ks = 0.0, wass = 0.0, kl = 0.0, bhatt = 0.0;
    uint32_t ndf = 0;

    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_chi2(h1, h2, &chi2, &ndf));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, chi2);
    TEST_ASSERT_EQUAL_UINT32(2, ndf);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_ks(h1, h2, &ks));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, ks);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_wasserstein_1d(h1, h2, &wass));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, wass);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_kl_divergence(h1, h2, &kl));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, kl);

    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_bhattacharyya(h1, h2, &bhatt));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, bhatt);

    histo_destroy(h1);
    histo_destroy(h2);
}

/* Test KL divergence with epsilon smoothing when Q has zero bins */
void test_cmp_kl_divergence_smoothing(void) {
    histo_t *h1 = histo_create_uniform(4, 0.0, 40.0, HISTO_FLAG_NONE);
    histo_t *h2 = histo_create_uniform(4, 0.0, 40.0, HISTO_FLAG_NONE);

    /* h1 has weight in bins 0 and 1, h2 only has weight in bin 0 */
    histo_fill_bin(h1, 0, 10.0);
    histo_fill_bin(h1, 1, 10.0);
    histo_fill_bin(h2, 0, 20.0);

    double kl = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_kl_divergence(h1, h2, &kl));
    /* With epsilon smoothing on Q for bin 1, KL divergence must be finite and positive */
    TEST_ASSERT_TRUE(isfinite(kl) && kl > 0.0);

    histo_destroy(h1);
    histo_destroy(h2);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cmp_identical);
    RUN_TEST(test_cmp_disjoint);
    RUN_TEST(test_cmp_chi2_weighted);
    RUN_TEST(test_cmp_variable_bins);
    RUN_TEST(test_cmp_incompatible);
    RUN_TEST(test_cmp_empty_and_null);
    RUN_TEST(test_cmp_variable_metrics);
    RUN_TEST(test_cmp_kl_divergence_smoothing);
    return UNITY_END();
}
