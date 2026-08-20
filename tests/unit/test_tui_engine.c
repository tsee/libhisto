#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 700

#include "unity.h"
#include "tui_term.h"
#include "tui_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void setUp(void) {}
void tearDown(void) {}

void test_tui_frame_buffer(void) {
    tui_frame_t frame;
    tui_frame_init(&frame, 64);
    TEST_ASSERT_NOT_NULL(frame.buf);
    TEST_ASSERT_EQUAL_UINT64(0, frame.len);

    tui_frame_puts(&frame, "Hello, ");
    tui_frame_printf(&frame, "world! Number: %d", 42);
    TEST_ASSERT_EQUAL_STRING("Hello, world! Number: 42", frame.buf);
    TEST_ASSERT_EQUAL_UINT64(strlen("Hello, world! Number: 42"), frame.len);

    tui_frame_clear(&frame);
    TEST_ASSERT_EQUAL_UINT64(0, frame.len);
    TEST_ASSERT_EQUAL_STRING("", frame.buf);

    tui_frame_free(&frame);
    TEST_ASSERT_NULL(frame.buf);
}

void test_tui_color_and_mono(void) {
    char color_ansi[64];
    tui_term_get_color(0.5, false, color_ansi, sizeof(color_ansi));
    TEST_ASSERT_TRUE(strlen(color_ansi) > 0);
    TEST_ASSERT_TRUE(strstr(color_ansi, "\033[38;2;") != NULL);

    char mono_ansi[64];
    tui_term_get_color(0.5, true, mono_ansi, sizeof(mono_ansi));
    TEST_ASSERT_EQUAL_STRING("", mono_ansi);
}

void test_tui_engine_streaming_and_snapshot(void) {
    int fds[2];
    TEST_ASSERT_EQUAL(0, pipe(fds));

    FILE *in_fp = fdopen(fds[0], "r");
    TEST_ASSERT_NOT_NULL(in_fp);

    tui_engine_t eng;
    TEST_ASSERT_TRUE(tui_engine_init(&eng, in_fp, false, 20, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2));
    TEST_ASSERT_TRUE(tui_engine_start(&eng));

    /* Write 50 lines to the pipe */
    for (int i = 1; i <= 50; i++) {
        char buf[32];
        int len = snprintf(buf, sizeof(buf), "%d\n", i);
        TEST_ASSERT_EQUAL(len, write(fds[1], buf, len));
    }
    close(fds[1]); // Signal EOF

    /* Wait for ingestion thread to finish reading */
    int timeout_ms = 1000;
    while (!tui_engine_is_finished(&eng) && timeout_ms > 0) {
        usleep(10000);
        timeout_ms -= 10;
    }
    usleep(50000); // Allow thread to flush batches

    /* Get snapshot */
    histo_t *snap = tui_engine_get_snapshot_1d(&eng);
    TEST_ASSERT_NOT_NULL(snap);
    TEST_ASSERT_EQUAL_UINT64(50, histo_num_entries(snap));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 50.0, histo_total_weight(snap));

    double mean = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_mean(snap, &mean));
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 25.5, mean);
    histo_destroy(snap);

    /* Test reservoir rebuild with 50 bins */
    histo_t *rebuilt = NULL;
    TEST_ASSERT_TRUE(tui_engine_rebuild_1d(&eng, 50, 0.0, 100.0, &rebuilt));
    TEST_ASSERT_NOT_NULL(rebuilt);
    TEST_ASSERT_EQUAL_UINT32(50, histo_nbins(rebuilt));
    TEST_ASSERT_EQUAL_UINT64(50, histo_num_entries(rebuilt));
    histo_destroy(rebuilt);

    /* Test clear */
    tui_engine_clear(&eng);
    histo_t *cleared_snap = tui_engine_get_snapshot_1d(&eng);
    TEST_ASSERT_NOT_NULL(cleared_snap);
    TEST_ASSERT_EQUAL_UINT64(0, histo_num_entries(cleared_snap));
    histo_destroy(cleared_snap);

    tui_engine_free(&eng);
    fclose(in_fp);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_tui_frame_buffer);
    RUN_TEST(test_tui_color_and_mono);
    RUN_TEST(test_tui_engine_streaming_and_snapshot);
    return UNITY_END();
}
