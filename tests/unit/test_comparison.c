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

/* ========================================================================= */
/* Statistical Distances Mathematical Invariants                             */
/* ========================================================================= */

void test_cmp_mathematical_invariants_and_metrics(void) {
    const uint32_t nbins = 10;
    histo_t *p = histo_create_uniform(nbins, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
    histo_t *q = histo_create_uniform(nbins, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
    histo_t *r = histo_create_uniform(nbins, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);

    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_NOT_NULL(r);

    /* P: left-skewed, Q: center-peaked, R: right-skewed */
    for (uint32_t i = 0; i < nbins; ++i) {
        histo_fill_bin(p, i, 1.0 + (double)(nbins - i));
        histo_fill_bin(q, i, 1.0 + (double)((i < 5) ? i : (nbins - 1 - i)));
        histo_fill_bin(r, i, 1.0 + (double)(i + 1));
    }

    /* 1. Identity of indiscernibles: D(P, P) == 0 */
    double ks_pp = -1.0, chi2_pp = -1.0, wass_pp = -1.0, kl_pp = -1.0, bhatt_pp = -1.0;
    uint32_t ndf_pp = 0;

    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_ks(p, p, &ks_pp));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_chi2(p, p, &chi2_pp, &ndf_pp));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_wasserstein_1d(p, p, &wass_pp));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_kl_divergence(p, p, &kl_pp));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_bhattacharyya(p, p, &bhatt_pp));

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, ks_pp);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, chi2_pp);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, wass_pp);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, kl_pp);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, bhatt_pp);

    /* 2. Non-negativity: D(P, Q) >= 0 */
    double ks_pq = 0.0, chi2_pq = 0.0, wass_pq = 0.0, kl_pq = 0.0, bhatt_pq = 0.0;
    double ks_qp = 0.0, chi2_qp = 0.0, wass_qp = 0.0, kl_qp = 0.0, bhatt_qp = 0.0;
    uint32_t ndf_pq = 0, ndf_qp = 0;

    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_ks(p, q, &ks_pq));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_chi2(p, q, &chi2_pq, &ndf_pq));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_wasserstein_1d(p, q, &wass_pq));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_kl_divergence(p, q, &kl_pq));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_bhattacharyya(p, q, &bhatt_pq));

    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_kl_divergence(q, p, &kl_qp));

    TEST_ASSERT_TRUE(ks_pq >= 0.0);
    TEST_ASSERT_TRUE(chi2_pq >= 0.0);
    TEST_ASSERT_TRUE(wass_pq >= 0.0);
    TEST_ASSERT_TRUE(kl_pq >= 0.0); /* Gibbs' inequality KL(P||Q) >= 0 */
    TEST_ASSERT_TRUE(kl_qp >= 0.0); /* Gibbs' inequality KL(Q||P) >= 0 */
    TEST_ASSERT_TRUE(bhatt_pq >= 0.0);

    /* 3. Symmetry: D(P, Q) == D(Q, P) */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_ks(q, p, &ks_qp));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_chi2(q, p, &chi2_qp, &ndf_qp));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_wasserstein_1d(q, p, &wass_qp));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_bhattacharyya(q, p, &bhatt_qp));

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, ks_pq, ks_qp);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, chi2_pq, chi2_qp);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, wass_pq, wass_qp);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, bhatt_pq, bhatt_qp);

    /* 4. Triangle Inequality for Wasserstein-1 metric: W1(P, R) <= W1(P, Q) + W1(Q, R) */
    double wass_pr = 0.0, wass_qr = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_wasserstein_1d(p, r, &wass_pr));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_cmp_wasserstein_1d(q, r, &wass_qr));

    TEST_ASSERT_TRUE(wass_pr <= wass_pq + wass_qr + 1e-9);

    histo_destroy(p);
    histo_destroy(q);
    histo_destroy(r);
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
    RUN_TEST(test_cmp_mathematical_invariants_and_metrics);
    return UNITY_END();
}
