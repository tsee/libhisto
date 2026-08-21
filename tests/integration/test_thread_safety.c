/*
 * Multi-threaded concurrency and thread safety tests under concurrent access.
 */

#include "unity.h"
#include "histo/histo.h"
#include <pthread.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#define NUM_THREADS 8
#define SAMPLES_PER_THREAD 100000

/* --- Test 1: Concurrent Readers --- */

typedef struct {
    const histo_t *shared_histo;
    double expected_mean;
    double expected_variance;
    double expected_integral;
    void *expected_buf;
    size_t expected_size;
} reader_args_t;

void *reader_worker(void *arg) {
    reader_args_t *args = (reader_args_t *)arg;
    const histo_t *h = args->shared_histo;

    double mean = 0, var = 0, integ = 0, q = 0, bc = 0, err = 0;
    histo_mean(h, &mean);
    histo_variance(h, &var);
    histo_integral(h, 0, histo_nbins(h)-1, &integ);
    histo_quantile(h, 0.5, &q);
    histo_bin_content(h, histo_nbins(h)/2, &bc);
    histo_bin_error(h, histo_nbins(h)/2, &err);

    void *buf = NULL;
    size_t size = 0;
    histo_serialize_binary(h, &buf, &size);

    TEST_ASSERT_DOUBLE_WITHIN(1e-9, args->expected_mean, mean);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, args->expected_variance, var);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, args->expected_integral, integ);
    TEST_ASSERT_EQUAL(args->expected_size, size);
    
    TEST_ASSERT_EQUAL(0, memcmp(args->expected_buf, buf, size));

    histo_free_buffer(buf);

    return NULL;
}

void test_concurrent_readers(void) {
    histo_t *h = histo_create_uniform(100, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
    for (int i = 0; i < 10000; i++) {
        histo_fill(h, 50.0 + (i % 20) - 10.0);
    }
    
    reader_args_t args;
    args.shared_histo = h;
    histo_mean(h, &args.expected_mean);
    histo_variance(h, &args.expected_variance);
    histo_integral(h, 0, histo_nbins(h)-1, &args.expected_integral);
    histo_serialize_binary(h, &args.expected_buf, &args.expected_size);

    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, reader_worker, &args);
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    histo_free_buffer(args.expected_buf);
    histo_destroy(h);
}

/* --- Test 2: Parallel Map-Reduce Accumulation --- */

typedef struct {
    histo_t *local_histo;
    int thread_id;
} map_reduce_args_t;

void *worker_fill(void *arg) {
    map_reduce_args_t *args = (map_reduce_args_t *)arg;
    for (int i = 0; i < SAMPLES_PER_THREAD; i++) {
        double val = (args->thread_id * 10.0) + (i % 100) * 0.1;
        histo_fill(args->local_histo, val);
    }
    return NULL;
}

void test_parallel_map_reduce_accumulation(void) {
    histo_t *seq_histo = histo_create_uniform(100, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
    histo_t *par_histo = histo_create_uniform(100, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
    
    // Sequential truth
    for (int tid = 0; tid < NUM_THREADS; tid++) {
        for (int i = 0; i < SAMPLES_PER_THREAD; i++) {
            double val = (tid * 10.0) + (i % 100) * 0.1;
            histo_fill(seq_histo, val);
        }
    }

    // Parallel
    pthread_t threads[NUM_THREADS];
    map_reduce_args_t args[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].thread_id = i;
        args[i].local_histo = histo_create_uniform(100, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2);
        pthread_create(&threads[i], NULL, worker_fill, &args[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        histo_add(par_histo, args[i].local_histo);
        histo_destroy(args[i].local_histo);
    }

    // Compare
    double seq_mean = 0, par_mean = 0;
    histo_mean(seq_histo, &seq_mean);
    histo_mean(par_histo, &par_mean);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, seq_mean, par_mean);

    double seq_var = 0, par_var = 0;
    histo_variance(seq_histo, &seq_var);
    histo_variance(par_histo, &par_var);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, seq_var, par_var);

    TEST_ASSERT_EQUAL_DOUBLE(histo_total_weight(seq_histo), histo_total_weight(par_histo));
    TEST_ASSERT_EQUAL(histo_num_entries(seq_histo), histo_num_entries(par_histo));
    
    for (uint32_t i = 0; i < histo_nbins(seq_histo); i++) {
        double s_c = 0, p_c = 0;
        histo_bin_content(seq_histo, i, &s_c);
        histo_bin_content(par_histo, i, &p_c);
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, s_c, p_c);
    }

    histo_destroy(seq_histo);
    histo_destroy(par_histo);
}

/* --- Test 3: Concurrent Cloning and Serialization --- */

typedef struct {
    const histo_t *base_histo;
} clone_args_t;

void *worker_clone(void *arg) {
    clone_args_t *args = (clone_args_t *)arg;
    
    histo_t *clone = histo_clone(args->base_histo, false);
    TEST_ASSERT_NOT_NULL(clone);
    
    histo_reset(clone);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, histo_total_weight(clone));
    
    for (int i = 0; i < 1000; i++) {
        histo_fill(clone, 50.0);
    }
    
    void *buf = NULL;
    size_t size = 0;
    histo_serialize_binary(clone, &buf, &size);
    
    histo_t *deser = NULL;
    histo_deserialize_binary(buf, size, &deser);
    
    double m1=0, m2=0;
    histo_mean(clone, &m1);
    histo_mean(deser, &m2);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, m1, m2);
    
    histo_free_buffer(buf);
    histo_destroy(deser);
    histo_destroy(clone);
    
    return NULL;
}

void test_concurrent_cloning_and_serialization(void) {
    histo_t *h = histo_create_uniform(50, 0.0, 50.0, HISTO_FLAG_TRACK_SUMW2);
    for (int i = 0; i < 5000; i++) {
        histo_fill(h, i % 50);
    }
    
    clone_args_t args;
    args.base_histo = h;
    
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, worker_clone, &args);
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    histo_destroy(h);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_concurrent_readers);
    RUN_TEST(test_parallel_map_reduce_accumulation);
    RUN_TEST(test_concurrent_cloning_and_serialization);
    return UNITY_END();
}
