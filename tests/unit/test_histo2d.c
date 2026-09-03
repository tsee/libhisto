/*
 * Unit tests for 2D bivariate histograms: creation, fills, and slicing.
 */

#include "unity.h"
#include "histo/histo2d.h"
#include <math.h>
#include <stdlib.h>
#include <float.h>

void setUp(void) {}
void tearDown(void) {}

/* -------------------------------------------------------------------------
 * 1. Lifecycle & Creation across all 4 Binning Combinations
 * ------------------------------------------------------------------------- */

void test_histo2d_lifecycle_uniform_uniform(void) {
    histo2d_t *h = histo2d_create_uniform(10, 0.0, 100.0, 20, -5.0, 5.0, HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    TEST_ASSERT_NOT_NULL(h);
    TEST_ASSERT_EQUAL_UINT32(10, histo2d_nbins_x(h));
    TEST_ASSERT_EQUAL_UINT32(20, histo2d_nbins_y(h));
    TEST_ASSERT_EQUAL_UINT64(0, histo2d_num_entries(h));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, histo2d_total_weight(h));

    histo2d_axis_t ax, ay;
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_axis_x(h, &ax));
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_axis_y(h, &ay));
    TEST_ASSERT_EQUAL(HISTO_BIN_UNIFORM, ax.type);
    TEST_ASSERT_EQUAL(HISTO_BIN_UNIFORM, ay.type);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, ax.min);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 100.0, ax.max);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -5.0, ay.min);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0, ay.max);

    histo2d_destroy(h);
}

void test_histo2d_lifecycle_variable_variable(void) {
    double xedges[] = {0.0, 1.0, 5.0, 20.0, 100.0};
    double yedges[] = {-10.0, -1.0, 0.0, 1.0, 10.0};
    histo2d_t *h = histo2d_create_variable(4, xedges, 4, yedges, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h);
    TEST_ASSERT_EQUAL_UINT32(4, histo2d_nbins_x(h));
    TEST_ASSERT_EQUAL_UINT32(4, histo2d_nbins_y(h));

    histo2d_axis_t ax, ay;
    histo2d_axis_x(h, &ax);
    histo2d_axis_y(h, &ay);
    TEST_ASSERT_EQUAL(HISTO_BIN_VARIABLE, ax.type);
    TEST_ASSERT_EQUAL(HISTO_BIN_VARIABLE, ay.type);

    histo2d_destroy(h);
}

void test_histo2d_lifecycle_mixed_models(void) {
    double yedges[] = {1.0, 2.0, 4.0, 8.0, 16.0};
    /* Uniform X, Variable Y */
    histo2d_t *h_uv = histo2d_create_uniform_variable(5, 0.0, 50.0, 4, yedges, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h_uv);
    TEST_ASSERT_EQUAL_UINT32(5, histo2d_nbins_x(h_uv));
    TEST_ASSERT_EQUAL_UINT32(4, histo2d_nbins_y(h_uv));
    histo2d_destroy(h_uv);

    double xedges[] = {-100.0, -10.0, 0.0, 10.0, 100.0};
    /* Variable X, Uniform Y */
    histo2d_t *h_vu = histo2d_create_variable_uniform(4, xedges, 10, -1.0, 1.0, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h_vu);
    TEST_ASSERT_EQUAL_UINT32(4, histo2d_nbins_x(h_vu));
    TEST_ASSERT_EQUAL_UINT32(10, histo2d_nbins_y(h_vu));
    histo2d_destroy(h_vu);
}

void test_histo2d_clone_and_reset(void) {
    histo2d_t *h = histo2d_create_uniform(4, 0.0, 4.0, 4, 0.0, 4.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h);

    histo2d_fill_w(h, 1.5, 2.5, 3.0);
    histo2d_fill_w(h, 0.5, 0.5, 2.0);
    TEST_ASSERT_EQUAL_UINT64(2, histo2d_num_entries(h));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0, histo2d_total_weight(h));

    /* Full Clone */
    histo2d_t *clone_full = histo2d_clone(h, false);
    TEST_ASSERT_NOT_NULL(clone_full);
    TEST_ASSERT_EQUAL_UINT64(2, histo2d_num_entries(clone_full));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0, histo2d_total_weight(clone_full));

    double w;
    histo2d_bin_content(clone_full, 1, 2, &w);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, w);
    histo2d_destroy(clone_full);

    /* Empty Clone */
    histo2d_t *clone_empty = histo2d_clone(h, true);
    TEST_ASSERT_NOT_NULL(clone_empty);
    TEST_ASSERT_EQUAL_UINT64(0, histo2d_num_entries(clone_empty));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, histo2d_total_weight(clone_empty));
    histo2d_destroy(clone_empty);

    /* Reset */
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_reset(h));
    TEST_ASSERT_EQUAL_UINT64(0, histo2d_num_entries(h));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, histo2d_total_weight(h));
    histo2d_bin_content(h, 1, 2, &w);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, w);

    histo2d_destroy(h);
}

/* -------------------------------------------------------------------------
 * 2. Ingestion Pipelines: Single, Weighted, Batch SoA, Strided AoS, Fill-Bin
 * ------------------------------------------------------------------------- */

void test_histo2d_ingestion_single_and_weighted(void) {
    histo2d_t *h = histo2d_create_uniform(5, 0.0, 5.0, 5, 0.0, 5.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h);

    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_fill(h, 2.5, 3.5));
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_fill_w(h, 2.5, 3.5, 4.0));

    double content, err, sum_w2;
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_bin_content(h, 2, 3, &content));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0, content);

    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_bin_sum_w2(h, 2, 3, &sum_w2));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0 * 1.0 + 4.0 * 4.0, sum_w2); /* 17.0 */

    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_bin_error(h, 2, 3, &err));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, sqrt(17.0), err);

    histo2d_destroy(h);
}

void test_histo2d_batch_fill_n_and_strided(void) {
    histo2d_t *h1 = histo2d_create_uniform(10, 0.0, 10.0, 10, 0.0, 10.0, HISTO_FLAG_TRACK_SUMW2);
    histo2d_t *h2 = histo2d_create_uniform(10, 0.0, 10.0, 10, 0.0, 10.0, HISTO_FLAG_TRACK_SUMW2);

    const size_t N = 1000;
    double *x = (double*)malloc(N * sizeof(double));
    double *y = (double*)malloc(N * sizeof(double));
    double *w = (double*)malloc(N * sizeof(double));

    typedef struct { double x; double y; double w; } sample_t;
    sample_t *samples = (sample_t*)malloc(N * sizeof(sample_t));

    for (size_t i = 0; i < N; ++i) {
        x[i] = (double)(i % 10) + 0.5;
        y[i] = (double)((i * 3) % 10) + 0.5;
        w[i] = 1.5;

        samples[i].x = x[i];
        samples[i].y = y[i];
        samples[i].w = w[i];
    }

    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_fill_n(h1, N, x, y, w));
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_fill_strided(h2, N,
                                                    &samples[0].x, sizeof(sample_t),
                                                    &samples[0].y, sizeof(sample_t),
                                                    &samples[0].w, sizeof(sample_t)));

    TEST_ASSERT_EQUAL_UINT64(N, histo2d_num_entries(h1));
    TEST_ASSERT_EQUAL_UINT64(N, histo2d_num_entries(h2));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, N * 1.5, histo2d_total_weight(h1));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, N * 1.5, histo2d_total_weight(h2));

    for (uint32_t ix = 0; ix < 10; ++ix) {
        for (uint32_t iy = 0; iy < 10; ++iy) {
            double c1, c2;
            histo2d_bin_content(h1, ix, iy, &c1);
            histo2d_bin_content(h2, ix, iy, &c2);
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, c1, c2);
        }
    }

    free(x); free(y); free(w); free(samples);
    histo2d_destroy(h1);
    histo2d_destroy(h2);
}

void test_histo2d_fill_bin_direct(void) {
    histo2d_t *h = histo2d_create_uniform(4, 0.0, 4.0, 4, 0.0, 4.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_fill_bin(h, 1, 2, 7.5));
    TEST_ASSERT_EQUAL(HISTO_ERR_OUT_OF_RANGE, histo2d_fill_bin(h, 4, 2, 1.0));
    TEST_ASSERT_EQUAL(HISTO_ERR_OUT_OF_RANGE, histo2d_fill_bin(h, 1, 4, 1.0));

    double content;
    histo2d_bin_content(h, 1, 2, &content);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 7.5, content);
    histo2d_destroy(h);
}

/* -------------------------------------------------------------------------
 * 3. 9-Region Guard Partitioning & Boundary Testing
 * ------------------------------------------------------------------------- */

void test_histo2d_9_guard_regions(void) {
    histo2d_t *h = histo2d_create_uniform(10, 0.0, 10.0, 10, 0.0, 10.0, HISTO_FLAG_TRACK_SUMW2);
    TEST_ASSERT_NOT_NULL(h);

    /* Center (in-range) */
    histo2d_fill_w(h, 5.0, 5.0, 10.0);
    /* East (x >= xmax, y in-range) */
    histo2d_fill_w(h, 12.0, 5.0, 2.0);
    /* North (x in-range, y >= ymax) */
    histo2d_fill_w(h, 5.0, 15.0, 3.0);
    /* South (x in-range, y < ymin) */
    histo2d_fill_w(h, 5.0, -2.0, 4.0);
    /* West (x < xmin, y in-range) */
    histo2d_fill_w(h, -3.0, 5.0, 5.0);
    /* North-East (x >= xmax, y >= ymax) */
    histo2d_fill_w(h, 12.0, 12.0, 6.0);
    /* North-West (x < xmin, y >= ymax) */
    histo2d_fill_w(h, -2.0, 12.0, 7.0);
    /* South-East (x >= xmax, y < ymin) */
    histo2d_fill_w(h, 15.0, -5.0, 8.0);
    /* South-West (x < xmin, y < ymin) */
    histo2d_fill_w(h, -5.0, -5.0, 9.0);

    /* Non-finite NaN sample */
    TEST_ASSERT_EQUAL(HISTO_ERR_NON_FINITE, histo2d_fill(h, NAN, 5.0));
    TEST_ASSERT_EQUAL(HISTO_ERR_NON_FINITE, histo2d_fill(h, 5.0, INFINITY));
    TEST_ASSERT_EQUAL_UINT64(2, histo2d_nan_count(h));

    double w;
    uint64_t c;

    histo2d_region_content(h, HISTO2D_REGION_CENTER, &w, &c);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 10.0, w); TEST_ASSERT_EQUAL_UINT64(1, c);

    histo2d_region_content(h, HISTO2D_REGION_EAST, &w, &c);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, w); TEST_ASSERT_EQUAL_UINT64(1, c);

    histo2d_region_content(h, HISTO2D_REGION_NORTH, &w, &c);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, w); TEST_ASSERT_EQUAL_UINT64(1, c);

    histo2d_region_content(h, HISTO2D_REGION_SOUTH, &w, &c);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.0, w); TEST_ASSERT_EQUAL_UINT64(1, c);

    histo2d_region_content(h, HISTO2D_REGION_WEST, &w, &c);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0, w); TEST_ASSERT_EQUAL_UINT64(1, c);

    histo2d_region_content(h, HISTO2D_REGION_NORTH_EAST, &w, &c);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 6.0, w); TEST_ASSERT_EQUAL_UINT64(1, c);

    histo2d_region_content(h, HISTO2D_REGION_NORTH_WEST, &w, &c);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 7.0, w); TEST_ASSERT_EQUAL_UINT64(1, c);

    histo2d_region_content(h, HISTO2D_REGION_SOUTH_EAST, &w, &c);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 8.0, w); TEST_ASSERT_EQUAL_UINT64(1, c);

    histo2d_region_content(h, HISTO2D_REGION_SOUTH_WEST, &w, &c);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, w); TEST_ASSERT_EQUAL_UINT64(1, c);

    /* Test accessors: flags and region_sum_w2 */
    TEST_ASSERT_EQUAL_UINT32(HISTO_FLAG_TRACK_SUMW2, histo2d_flags(h));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.0, histo2d_region_sum_w2(h, HISTO2D_REGION_EAST));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9.0, histo2d_region_sum_w2(h, HISTO2D_REGION_NORTH));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 16.0, histo2d_region_sum_w2(h, HISTO2D_REGION_SOUTH));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, histo2d_region_sum_w2(NULL, HISTO2D_REGION_NORTH));

    /* In-range total weight should only be the center weight */
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 10.0, histo2d_total_weight(h));
    TEST_ASSERT_EQUAL_UINT64(1, histo2d_num_entries(h));

    histo2d_destroy(h);
}

void test_histo2d_exact_boundary_transitions(void) {
    histo2d_t *h = histo2d_create_uniform(4, 0.0, 4.0, 4, 0.0, 4.0, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h);

    /* Lower inclusive bound x=0.0, y=0.0 -> cell (0, 0) */
    int64_t ix, iy;
    histo2d_find_bin(h, 0.0, 0.0, &ix, &iy);
    TEST_ASSERT_EQUAL_INT64(0, ix);
    TEST_ASSERT_EQUAL_INT64(0, iy);

    /* Exact bin boundary x=1.0, y=2.0 -> cell (1, 2) */
    histo2d_find_bin(h, 1.0, 2.0, &ix, &iy);
    TEST_ASSERT_EQUAL_INT64(1, ix);
    TEST_ASSERT_EQUAL_INT64(2, iy);

    /* Upper exclusive bound x=4.0, y=4.0 -> overflow */
    histo2d_find_bin(h, 4.0, 4.0, &ix, &iy);
    TEST_ASSERT_EQUAL_INT64(4, ix);
    TEST_ASSERT_EQUAL_INT64(4, iy);

    histo2d_region_t reg;
    histo2d_find_region(h, 4.0, 4.0, &reg);
    TEST_ASSERT_EQUAL(HISTO2D_REGION_NORTH_EAST, reg);

    histo2d_destroy(h);
}

/* -------------------------------------------------------------------------
 * 4. Geometry & Coordinate Queries
 * ------------------------------------------------------------------------- */

void test_histo2d_bounds_and_centers(void) {
    double xedges[] = {0.0, 2.0, 10.0};
    double yedges[] = {100.0, 200.0, 500.0};
    histo2d_t *h = histo2d_create_variable(2, xedges, 2, yedges, HISTO_FLAG_NONE);
    TEST_ASSERT_NOT_NULL(h);

    double xmin, xmax, ymin, ymax, cx, cy;
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_bin_bounds(h, 0, 1, &xmin, &xmax, &ymin, &ymax));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, xmin);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, xmax);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 200.0, ymin);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 500.0, ymax);

    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_bin_center(h, 0, 1, &cx, &cy));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, cx);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 350.0, cy);

    histo2d_destroy(h);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_histo2d_lifecycle_uniform_uniform);
    RUN_TEST(test_histo2d_lifecycle_variable_variable);
    RUN_TEST(test_histo2d_lifecycle_mixed_models);
    RUN_TEST(test_histo2d_clone_and_reset);
    RUN_TEST(test_histo2d_ingestion_single_and_weighted);
    RUN_TEST(test_histo2d_batch_fill_n_and_strided);
    RUN_TEST(test_histo2d_fill_bin_direct);
    RUN_TEST(test_histo2d_9_guard_regions);
    RUN_TEST(test_histo2d_exact_boundary_transitions);
    RUN_TEST(test_histo2d_bounds_and_centers);
    return UNITY_END();
}
