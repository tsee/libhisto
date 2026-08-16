#include "unity.h"
#include "histo/histo.h"
#include <math.h>
#include <stdlib.h>

void setUp(void) {}
void tearDown(void) {}

/* End-to-end signal + background subtraction and cross-section measurement workflow */
void test_signal_background_workflow(void) {
    const uint32_t nbins = 50;
    const double min_val = 0.0;
    const double max_val = 100.0;

    histo_t *data = histo_create_uniform(nbins, min_val, max_val, HISTO_FLAG_TRACK_SUMW2);
    histo_t *bkg = histo_create_uniform(nbins, min_val, max_val, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_NOT_NULL(bkg);

    /* Fill background: flat distribution with 1000 events */
    for (int i = 0; i < 1000; ++i) {
        double val = (double)(i % 100) * 0.98 + 1.0;
        histo_fill(bkg, val);
    }

    /* Fill data: background + Gaussian signal peak around 50.0 */
    for (int i = 0; i < 1000; ++i) {
        double val = (double)(i % 100) * 0.98 + 1.0;
        histo_fill(data, val);
    }
    for (int i = 0; i < 200; ++i) {
        /* Pseudo Gaussian around 50 */
        double val = 50.0 + ((double)(i % 11) - 5.0) * 1.5;
        histo_fill(data, val);
    }

    TEST_ASSERT_EQUAL_DOUBLE(1000.0, histo_total_weight(bkg));
    TEST_ASSERT_EQUAL_DOUBLE(1200.0, histo_total_weight(data));

    /* Scale background by normalization factor 1.0 */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_scale(bkg, 1.0));

    /* Subtract background from data to isolate signal */
    TEST_ASSERT_EQUAL(HISTO_OK, histo_subtract(data, bkg));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 200.0, histo_total_weight(data));

    /* Compute signal median and mean */
    double median = 0.0, mean = 0.0, std_dev = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_median(data, &median));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_mean(data, &mean));
    TEST_ASSERT_EQUAL(HISTO_OK, histo_std_dev(data, &std_dev));

    TEST_ASSERT_DOUBLE_WITHIN(2.0, 50.0, median);
    TEST_ASSERT_DOUBLE_WITHIN(2.0, 50.0, mean);

    /* Serialize signal histogram to binary, reload, and verify */
    void *buf = NULL;
    size_t buf_size = 0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_serialize_binary(data, &buf, &buf_size));

    histo_t *reloaded = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_deserialize_binary(buf, buf_size, &reloaded));
    TEST_ASSERT_NOT_NULL(reloaded);

    double reloaded_mean = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_mean(reloaded, &reloaded_mean));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, mean, reloaded_mean);

    /* Generate CDF of signal */
    histo_t *cdf = histo_cdf(reloaded, 1.0);
    TEST_ASSERT_NOT_NULL(cdf);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, histo_total_weight(cdf));

    histo_destroy(data);
    histo_destroy(bkg);
    histo_free_buffer(buf);
    histo_destroy(reloaded);
    histo_destroy(cdf);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_signal_background_workflow);
    return UNITY_END();
}
