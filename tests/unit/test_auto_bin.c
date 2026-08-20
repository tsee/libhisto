#include "unity.h"
#include "histo/histo.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

void setUp(void) {}
void tearDown(void) {}

void test_auto_bin_normal(void) {
    const size_t n = 1000;
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

    uint32_t nbins_fd = 0, nbins_scott = 0, nbins_sturges = 0, nbins_doane = 0, nbins_knuth = 0;
    double min_v = 0.0, max_v = 0.0;

    TEST_ASSERT_EQUAL(HISTO_OK, histo_estimate_bins_fd(n, vals, &nbins_fd, &min_v, &max_v));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_estimate_bins_scott(n, vals, &nbins_scott, &min_v, &max_v));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_estimate_bins_sturges(n, vals, &nbins_sturges, &min_v, &max_v));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_estimate_bins_doane(n, vals, &nbins_doane, &min_v, &max_v));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_estimate_bins_knuth(n, vals, &nbins_knuth, &min_v, &max_v));

    /* Sturges: ceil(log2(1000) + 1) = 11 */
    TEST_ASSERT_EQUAL_UINT32(11, nbins_sturges);
    TEST_ASSERT_TRUE(nbins_fd >= 10 && nbins_fd <= 60);
    TEST_ASSERT_TRUE(nbins_scott >= 10 && nbins_scott <= 60);
    TEST_ASSERT_TRUE(nbins_doane >= 10 && nbins_doane <= 25);
    TEST_ASSERT_TRUE(nbins_knuth >= 5 && nbins_knuth <= 100);

    /* Test histo_create_auto */
    histo_t *h_auto = histo_create_auto(n, vals, HISTO_BIN_RULE_AUTO, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h_auto);
    TEST_ASSERT_EQUAL_UINT32(nbins_fd, histo_nbins(h_auto));
    TEST_ASSERT_EQUAL_UINT64((uint64_t)n, histo_num_entries(h_auto));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, (double)n, histo_total_weight(h_auto));

    histo_destroy(h_auto);
    free(vals);
}

void test_auto_bin_edge_cases(void) {
    double identical[] = { 42.0, 42.0, 42.0, 42.0, 42.0 };
    uint32_t nbins = 0;
    double min_v = 0.0, max_v = 0.0;

    TEST_ASSERT_EQUAL(HISTO_OK, histo_estimate_bins_fd(5, identical, &nbins, &min_v, &max_v));
    TEST_ASSERT_EQUAL_UINT32(1, nbins);
    TEST_ASSERT_EQUAL_DOUBLE(41.5, min_v);
    TEST_ASSERT_EQUAL_DOUBLE(42.5, max_v);

    histo_t *h_ident = histo_create_auto(5, identical, HISTO_BIN_RULE_FD, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h_ident);
    TEST_ASSERT_EQUAL_UINT32(1, histo_nbins(h_ident));
    TEST_ASSERT_EQUAL_UINT64(5, histo_num_entries(h_ident));
    histo_destroy(h_ident);

    double with_nans[] = { 1.0, NAN, 2.0, INFINITY, 3.0, -INFINITY, 4.0, 5.0 };
    TEST_ASSERT_EQUAL(HISTO_OK, histo_estimate_bins_fd(8, with_nans, &nbins, &min_v, &max_v));
    TEST_ASSERT_TRUE(nbins >= 1);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, min_v);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, max_v);

    double single[] = { 10.0 };
    TEST_ASSERT_EQUAL(HISTO_OK, histo_estimate_bins_scott(1, single, &nbins, &min_v, &max_v));
    TEST_ASSERT_EQUAL_UINT32(1, nbins);

    TEST_ASSERT_EQUAL(HISTO_ERR_INVALID_ARG, histo_estimate_bins_fd(0, NULL, &nbins, &min_v, &max_v));
    TEST_ASSERT_NULL(histo_create_auto(0, NULL, HISTO_BIN_RULE_AUTO, 0));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_auto_bin_normal);
    RUN_TEST(test_auto_bin_edge_cases);
    return UNITY_END();
}
