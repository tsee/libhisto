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

    /* Test reservoir rebuild with logarithmic X bins */
    histo_t *rebuilt_log = NULL;
    TEST_ASSERT_TRUE(tui_engine_rebuild_1d_log(&eng, 40, &rebuilt_log));
    TEST_ASSERT_NOT_NULL(rebuilt_log);
    TEST_ASSERT_EQUAL_UINT32(40, histo_nbins(rebuilt_log));
    TEST_ASSERT_EQUAL_UINT64(50, histo_num_entries(rebuilt_log));
    TEST_ASSERT_EQUAL(HISTO_BIN_VARIABLE, histo_bin_type(rebuilt_log));
    histo_destroy(rebuilt_log);

    /* Test clear */
    tui_engine_clear(&eng);
    histo_t *cleared_snap = tui_engine_get_snapshot_1d(&eng);
    TEST_ASSERT_NOT_NULL(cleared_snap);
    TEST_ASSERT_EQUAL_UINT64(0, histo_num_entries(cleared_snap));
    histo_destroy(cleared_snap);

    tui_engine_free(&eng);
    fclose(in_fp);
}

void test_tui_visual_width(void) {
    TEST_ASSERT_EQUAL_INT(0, tui_visual_width(""));
    TEST_ASSERT_EQUAL_INT(5, tui_visual_width("hello"));
    TEST_ASSERT_EQUAL_INT(5, tui_visual_width("\033[1;31mhello\033[0m")); // ANSI escapes = 0 cols
    TEST_ASSERT_EQUAL_INT(1, tui_visual_width("│")); // 3-byte UTF-8 box char = 1 col
    TEST_ASSERT_EQUAL_INT(1, tui_visual_width("±")); // 2-byte UTF-8 symbol = 1 col
    TEST_ASSERT_EQUAL_INT(1, tui_visual_width("Δ")); // 2-byte UTF-8 symbol = 1 col
    TEST_ASSERT_EQUAL_INT(4, tui_visual_width("┌─┐│")); // 4 box chars = 4 cols
}

void test_tui_render_row_geometry(void) {
    int test_widths[] = { 40, 60, 80, 120, 160 };
    const char *test_contents[] = {
        "",
        "Short text",
        "Mean: 45.23 ± 5.12 │ Med: 45.00 │ IQR: 7.20 │ P95: 55.30 │ P99: 62.10",
        "\033[38;2;255;0;0m[  0.00,  10.00) │   1520 │ ████████████████████████████████████████\033[0m",
        "An extremely excessively long row of text designed to test boundary truncation when the line exceeds terminal column limits"
    };

    for (size_t w = 0; w < sizeof(test_widths) / sizeof(test_widths[0]); ++w) {
        int width = test_widths[w];
        for (size_t c = 0; c < sizeof(test_contents) / sizeof(test_contents[0]); ++c) {
            tui_frame_t frame;
            tui_frame_init(&frame, 256);
            tui_render_row(&frame, test_contents[c], width, false);

            int vis_width = tui_visual_width(frame.buf);
            TEST_ASSERT_EQUAL_INT_MESSAGE(width, vis_width, "Row visual width must strictly match target terminal width");
            tui_frame_free(&frame);
        }
    }
}

void test_tui_engine_autorange_p1_p99(void) {
    int fds[2];
    TEST_ASSERT_EQUAL(0, pipe(fds));

    FILE *in_fp = fdopen(fds[0], "r");
    TEST_ASSERT_NOT_NULL(in_fp);

    tui_engine_t eng;
    /* Initial range [0, 10], but stream samples will be [100, 200] with extreme outliers */
    TEST_ASSERT_TRUE(tui_engine_init(&eng, in_fp, false, 20, 0.0, 10.0, HISTO_FLAG_TRACK_SUMW2));
    eng.last_autorange_time_sec = 0.0; /* Reset timer so autorange can trigger immediately */
    TEST_ASSERT_TRUE(tui_engine_start(&eng));

    /* Write 200 normal samples in [100, 200] */
    for (int i = 0; i < 200; i++) {
        char buf[32];
        int val = 100 + (i % 100);
        int len = snprintf(buf, sizeof(buf), "%d\n", val);
        TEST_ASSERT_EQUAL(len, write(fds[1], buf, len));
    }
    /* Write 2 extreme outlier samples (-10000 and +99999) */
    const char *outliers = "-10000\n99999\n";
    TEST_ASSERT_EQUAL((int)strlen(outliers), write(fds[1], outliers, strlen(outliers)));

    close(fds[1]); // Signal EOF

    int timeout_ms = 1000;
    while (!tui_engine_is_finished(&eng) && timeout_ms > 0) {
        usleep(10000);
        timeout_ms -= 10;
    }
    usleep(50000);

    /* Get snapshot - this will invoke check_and_autorange_locked */
    histo_t *snap = tui_engine_get_snapshot_1d(&eng);
    TEST_ASSERT_NOT_NULL(snap);

    double rmin = 0, rmax = 0;
    histo_range(snap, &rmin, &rmax);

    /* p1 should be around 100, p99 around 199. With 5% margin, range should be approx [95, 205], rejecting -10000 and 99999 */
    TEST_ASSERT_TRUE(rmin >= 90.0 && rmin <= 105.0);
    TEST_ASSERT_TRUE(rmax >= 195.0 && rmax <= 210.0);

    /* Check that the vast majority of samples are within the active histogram range */
    double in_range_w = 0.0;
    for (uint32_t b = 0; b < histo_nbins(snap); ++b) {
        double c = 0.0;
        histo_bin_content(snap, b, &c);
        in_range_w += c;
    }
    TEST_ASSERT_TRUE(in_range_w >= 195.0);

    histo_destroy(snap);

    /* Test set_autorange disabled */
    tui_engine_set_autorange(&eng, false, 0.05);
    TEST_ASSERT_FALSE(eng.auto_range);

    tui_engine_free(&eng);
    fclose(in_fp);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_tui_frame_buffer);
    RUN_TEST(test_tui_color_and_mono);
    RUN_TEST(test_tui_visual_width);
    RUN_TEST(test_tui_render_row_geometry);
    RUN_TEST(test_tui_engine_streaming_and_snapshot);
    RUN_TEST(test_tui_engine_autorange_p1_p99);
    return UNITY_END();
}
