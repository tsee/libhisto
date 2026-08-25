/*
 * Unit tests for shared memory binary ring buffer IPC protocol.
 */

#include "unity.h"
#include "tui_shm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

void setUp(void) {}
void tearDown(void) {}

void test_shm_create_open_close(void) {
    char shm_name[64];
    snprintf(shm_name, sizeof(shm_name), "/histo_test_shm_lc_%d", (int)getpid());
    histo_shm_t producer_shm;
    histo_shm_t consumer_shm;

    bool ok = histo_shm_create(&producer_shm, shm_name, 1024, sizeof(double), 0);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_NOT_NULL(producer_shm.ring);
    TEST_ASSERT_EQUAL_UINT64(HISTO_SHM_MAGIC, producer_shm.ring->magic);
    TEST_ASSERT_EQUAL_UINT32(1, producer_shm.ring->version);
    TEST_ASSERT_EQUAL_UINT32(sizeof(double), producer_shm.ring->entry_size);
    TEST_ASSERT_EQUAL_UINT64(1023, producer_shm.ring->mask);

    ok = histo_shm_open(&consumer_shm, shm_name);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_NOT_NULL(consumer_shm.ring);
    TEST_ASSERT_EQUAL_UINT64(producer_shm.ring->magic, consumer_shm.ring->magic);

    histo_shm_close(&consumer_shm);
    histo_shm_close(&producer_shm);
}

void test_shm_push_and_pop_batch(void) {
    char shm_name[64];
    snprintf(shm_name, sizeof(shm_name), "/histo_test_shm_stream_%d", (int)getpid());
    histo_shm_t producer;
    histo_shm_t consumer;

    TEST_ASSERT_TRUE(histo_shm_create(&producer, shm_name, 512, sizeof(double), 0));
    TEST_ASSERT_TRUE(histo_shm_open(&consumer, shm_name));

    /* Push 100 samples */
    for (int i = 0; i < 100; ++i) {
        double val = (double)i * 1.5;
        TEST_ASSERT_TRUE(histo_shm_push(producer.ring, &val));
    }

    double batch[128];
    size_t popped = histo_shm_pop_batch(consumer.ring, batch, 60);
    TEST_ASSERT_EQUAL_UINT64(60, popped);
    for (size_t i = 0; i < 60; ++i) {
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, (double)i * 1.5, batch[i]);
    }

    /* Pop remainder */
    popped = histo_shm_pop_batch(consumer.ring, batch, 128);
    TEST_ASSERT_EQUAL_UINT64(40, popped);
    for (size_t i = 0; i < 40; ++i) {
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, (double)(60 + i) * 1.5, batch[i]);
    }

    /* Buffer should now be empty */
    popped = histo_shm_pop_batch(consumer.ring, batch, 128);
    TEST_ASSERT_EQUAL_UINT64(0, popped);

    histo_shm_close(&consumer);
    histo_shm_close(&producer);
}

void test_shm_wrap_around_and_overrun(void) {
    char shm_name[64];
    snprintf(shm_name, sizeof(shm_name), "/histo_test_shm_wrap_%d", (int)getpid());
    histo_shm_t producer;
    histo_shm_t consumer;

    /* Capacity = 16 */
    TEST_ASSERT_TRUE(histo_shm_create(&producer, shm_name, 16, sizeof(double), 0));
    TEST_ASSERT_TRUE(histo_shm_open(&consumer, shm_name));

    /* Push 30 samples (exceeds capacity of 16, should drop oldest 14) */
    for (int i = 0; i < 30; ++i) {
        double val = (double)i;
        histo_shm_push(producer.ring, &val);
    }

    TEST_ASSERT_EQUAL_UINT64(14, producer.ring->dropped);

    double batch[32];
    size_t popped = histo_shm_pop_batch(consumer.ring, batch, 32);
    TEST_ASSERT_EQUAL_UINT64(16, popped);

    /* Should read samples 14 to 29 */
    for (size_t i = 0; i < 16; ++i) {
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, (double)(14 + i), batch[i]);
    }

    histo_shm_close(&consumer);
    histo_shm_close(&producer);
}

void test_shm_2d_entries(void) {
    char shm_name[64];
    snprintf(shm_name, sizeof(shm_name), "/histo_test_shm_2d_%d", (int)getpid());
    histo_shm_t producer;
    histo_shm_t consumer;

    typedef struct {
        double x;
        double y;
    } point2d_t;

    TEST_ASSERT_TRUE(histo_shm_create(&producer, shm_name, 64, sizeof(point2d_t), 1));
    TEST_ASSERT_TRUE(histo_shm_open(&consumer, shm_name));

    for (int i = 0; i < 20; ++i) {
        point2d_t pt = {(double)i, (double)i * 2.0};
        histo_shm_push(producer.ring, &pt);
    }

    point2d_t batch[32];
    size_t popped = histo_shm_pop_batch(consumer.ring, batch, 32);
    TEST_ASSERT_EQUAL_UINT64(20, popped);

    for (size_t i = 0; i < 20; ++i) {
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, (double)i, batch[i].x);
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, (double)i * 2.0, batch[i].y);
    }

    histo_shm_close(&consumer);
    histo_shm_close(&producer);
}

void test_shm_invalid_args(void) {
    histo_shm_t shm;
    TEST_ASSERT_FALSE(histo_shm_create(NULL, "/invalid", 64, sizeof(double), 0));
    TEST_ASSERT_FALSE(histo_shm_create(&shm, NULL, 64, sizeof(double), 0));
    TEST_ASSERT_FALSE(histo_shm_create(&shm, "/invalid", 0, sizeof(double), 0));
    TEST_ASSERT_FALSE(histo_shm_create(&shm, "/invalid", 64, 0, 0));
    TEST_ASSERT_FALSE(histo_shm_open(NULL, "/invalid"));
    TEST_ASSERT_FALSE(histo_shm_open(&shm, NULL));
    TEST_ASSERT_FALSE(histo_shm_push(NULL, NULL));
    TEST_ASSERT_EQUAL_UINT64(0, histo_shm_pop_batch(NULL, NULL, 0));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_shm_create_open_close);
    RUN_TEST(test_shm_push_and_pop_batch);
    RUN_TEST(test_shm_wrap_around_and_overrun);
    RUN_TEST(test_shm_2d_entries);
    RUN_TEST(test_shm_invalid_args);
    return UNITY_END();
}
