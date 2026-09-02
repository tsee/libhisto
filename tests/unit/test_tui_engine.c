/*
 * Unit tests for TUI engine: streaming, windowing, zoom/pan, and concurrency.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include "unity.h"
#include "tui_term.h"
#include "tui_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <process.h>
#define pipe(fds) _pipe((fds), 4096, _O_BINARY)
#define write _write
#define close _close
#define getpid _getpid
#define usleep(usec) Sleep((DWORD)((usec) / 1000 > 0 ? (usec) / 1000 : 1))
#else
#include <sys/types.h>
#include <unistd.h>
#endif

static const char* get_temp_dir(void) {
#if defined(_WIN32)
    const char *tmp = getenv("TEMP");
    if (!tmp) tmp = getenv("TMP");
    if (!tmp) tmp = ".";
    return tmp;
#else
    const char *tmp = getenv("TMPDIR");
    if (!tmp) tmp = "/tmp";
    return tmp;
#endif
}

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
    TEST_ASSERT_TRUE(tui_engine_init(&eng, in_fp, false, 20, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2, false));
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
    TEST_ASSERT_TRUE(tui_engine_init(&eng, in_fp, false, 20, 0.0, 10.0, HISTO_FLAG_TRACK_SUMW2, false));
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

void test_tui_engine_weights(void) {
    int fds[2];
    TEST_ASSERT_EQUAL(0, pipe(fds));

    FILE *in_fp = fdopen(fds[0], "r");
    TEST_ASSERT_NOT_NULL(in_fp);

    tui_engine_t eng;
    /* Ingest with weights = true */
    TEST_ASSERT_TRUE(tui_engine_init(&eng, in_fp, false, 10, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2, true));
    TEST_ASSERT_TRUE(tui_engine_start(&eng));

    /* Write 20 lines with weight 2.5: "50.0 2.5\n" */
    for (int i = 0; i < 20; i++) {
        const char *line = "50.0 2.5\n";
        TEST_ASSERT_EQUAL((int)strlen(line), write(fds[1], line, strlen(line)));
    }
    close(fds[1]);

    int timeout_ms = 1000;
    while (!tui_engine_is_finished(&eng) && timeout_ms > 0) {
        usleep(10000);
        timeout_ms -= 10;
    }
    usleep(50000);

    histo_t *snap = tui_engine_get_snapshot_1d(&eng);
    TEST_ASSERT_NOT_NULL(snap);
    TEST_ASSERT_EQUAL_UINT64(20, histo_num_entries(snap));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 50.0, histo_total_weight(snap)); // 20 * 2.5 = 50.0
    histo_destroy(snap);

    /* Test reservoir rebuild with weights preserved */
    histo_t *rebuilt = NULL;
    TEST_ASSERT_TRUE(tui_engine_rebuild_1d(&eng, 25, 0.0, 100.0, &rebuilt));
    TEST_ASSERT_NOT_NULL(rebuilt);
    TEST_ASSERT_EQUAL_UINT64(20, histo_num_entries(rebuilt));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 50.0, histo_total_weight(rebuilt));
    histo_destroy(rebuilt);

    tui_engine_free(&eng);
    fclose(in_fp);
}

void test_tui_engine_zoom_pan_1d(void) {
    int fds[2];
    TEST_ASSERT_EQUAL(0, pipe(fds));

    FILE *in_fp = fdopen(fds[0], "r");
    TEST_ASSERT_NOT_NULL(in_fp);

    tui_engine_t eng;
    TEST_ASSERT_TRUE(tui_engine_init(&eng, in_fp, false, 20, 0.0, 100.0, 0, false));
    TEST_ASSERT_TRUE(tui_engine_start(&eng));

    for (int i = 0; i <= 100; i++) {
        char buf[32];
        int len = snprintf(buf, sizeof(buf), "%d\n", i);
        TEST_ASSERT_EQUAL(len, write(fds[1], buf, len));
    }
    close(fds[1]);

    int timeout_ms = 1000;
    while (!tui_engine_is_finished(&eng) && timeout_ms > 0) {
        usleep(10000);
        timeout_ms -= 10;
    }
    usleep(50000);

    /* Initial range is [0.0, 100.0] */
    histo_t *snap = NULL;
    /* Zoom in by 0.8 (factor 0.8 -> span 80.0 centered at 50.0: [10.0, 90.0]) */
    TEST_ASSERT_TRUE(tui_engine_zoom_1d(&eng, 0.80, &snap));
    TEST_ASSERT_NOT_NULL(snap);
    TEST_ASSERT_FALSE(eng.auto_range);
    double rmin = 0.0, rmax = 0.0;
    histo_range(snap, &rmin, &rmax);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 10.0, rmin);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 90.0, rmax);
    histo_destroy(snap);

    /* Pan right by +0.10 (+8.0 -> [18.0, 98.0]) */
    TEST_ASSERT_TRUE(tui_engine_pan_1d(&eng, 0.10, &snap));
    TEST_ASSERT_NOT_NULL(snap);
    histo_range(snap, &rmin, &rmax);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 18.0, rmin);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 98.0, rmax);
    histo_destroy(snap);

    /* Pan left by -0.10 (-8.0 -> [10.0, 90.0]) */
    TEST_ASSERT_TRUE(tui_engine_pan_1d(&eng, -0.10, &snap));
    TEST_ASSERT_NOT_NULL(snap);
    histo_range(snap, &rmin, &rmax);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 10.0, rmin);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 90.0, rmax);
    histo_destroy(snap);

    /* Zoom out by 1.25 (span 80.0 * 1.25 = 100.0 centered at 50.0: [0.0, 100.0]) */
    TEST_ASSERT_TRUE(tui_engine_zoom_1d(&eng, 1.25, &snap));
    TEST_ASSERT_NOT_NULL(snap);
    histo_range(snap, &rmin, &rmax);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 0.0, rmin);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 100.0, rmax);
    histo_destroy(snap);

    tui_engine_free(&eng);
    fclose(in_fp);
}

void test_tui_engine_zoom_pan_2d(void) {
    int fds[2];
    TEST_ASSERT_EQUAL(0, pipe(fds));

    FILE *in_fp = fdopen(fds[0], "r");
    TEST_ASSERT_NOT_NULL(in_fp);

    tui_engine_t eng;
    TEST_ASSERT_TRUE(tui_engine_init(&eng, in_fp, true, 20, 0.0, 100.0, 0, false));
    TEST_ASSERT_TRUE(tui_engine_start(&eng));

    for (int i = 0; i <= 50; i++) {
        char buf[64];
        int len = snprintf(buf, sizeof(buf), "%d %d\n", i * 2, i * 2);
        TEST_ASSERT_EQUAL(len, write(fds[1], buf, len));
    }
    close(fds[1]);

    int timeout_ms = 1000;
    while (!tui_engine_is_finished(&eng) && timeout_ms > 0) {
        usleep(10000);
        timeout_ms -= 10;
    }
    usleep(50000);

    histo2d_t *snap2d = NULL;
    /* Zoom in 2D by 0.8 */
    TEST_ASSERT_TRUE(tui_engine_zoom_2d(&eng, 0.80, &snap2d));
    TEST_ASSERT_NOT_NULL(snap2d);
    histo2d_axis_t ax, ay;
    histo2d_axis_x(snap2d, &ax);
    histo2d_axis_y(snap2d, &ay);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 10.0, ax.min);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 90.0, ax.max);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 10.0, ay.min);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 90.0, ay.max);
    histo2d_destroy(snap2d);

    /* Pan X right +0.10 and Y up +0.20 */
    TEST_ASSERT_TRUE(tui_engine_pan_2d(&eng, 0.10, 0.20, &snap2d));
    TEST_ASSERT_NOT_NULL(snap2d);
    histo2d_axis_x(snap2d, &ax);
    histo2d_axis_y(snap2d, &ay);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 18.0, ax.min);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 98.0, ax.max);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 26.0, ay.min);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 106.0, ay.max);
    histo2d_destroy(snap2d);

    tui_engine_free(&eng);
    fclose(in_fp);
}

void test_tui_term_mouse_and_shift_tab_parsing(void) {
#if defined(_WIN32)
    TEST_IGNORE_MESSAGE("TTY pipe simulation of ANSI escape sequences not applicable on Windows (Win32 Console API is used)");
#else
    int fds[2];
    TEST_ASSERT_EQUAL(0, pipe(fds));

    /* 1. SGR Mouse Click at (col 12, row 7): \033[<0;12;7M */
    const char *click_seq = "\033[<0;12;7M";
    TEST_ASSERT_EQUAL((int)strlen(click_seq), write(fds[1], click_seq, strlen(click_seq)));
    tui_key_event_t ev1 = tui_term_read_key(fds[0], 100);
    TEST_ASSERT_EQUAL(TUI_KEY_MOUSE_CLICK, ev1.type);
    TEST_ASSERT_EQUAL_INT(12, ev1.mouse_x);
    TEST_ASSERT_EQUAL_INT(7, ev1.mouse_y);

    /* 2. SGR Mouse Scroll Up at (col 25, row 10): \033[<64;25;10M */
    const char *scroll_up_seq = "\033[<64;25;10M";
    TEST_ASSERT_EQUAL((int)strlen(scroll_up_seq), write(fds[1], scroll_up_seq, strlen(scroll_up_seq)));
    tui_key_event_t ev2 = tui_term_read_key(fds[0], 100);
    TEST_ASSERT_EQUAL(TUI_KEY_MOUSE_SCROLL_UP, ev2.type);
    TEST_ASSERT_EQUAL_INT(25, ev2.mouse_x);
    TEST_ASSERT_EQUAL_INT(10, ev2.mouse_y);

    /* 3. SGR Mouse Scroll Down at (col 30, row 15): \033[<65;30;15M */
    const char *scroll_dn_seq = "\033[<65;30;15M";
    TEST_ASSERT_EQUAL((int)strlen(scroll_dn_seq), write(fds[1], scroll_dn_seq, strlen(scroll_dn_seq)));
    tui_key_event_t ev3 = tui_term_read_key(fds[0], 100);
    TEST_ASSERT_EQUAL(TUI_KEY_MOUSE_SCROLL_DOWN, ev3.type);
    TEST_ASSERT_EQUAL_INT(30, ev3.mouse_x);
    TEST_ASSERT_EQUAL_INT(15, ev3.mouse_y);

    /* 4. Shift-Tab escape sequence: \033[Z */
    const char *shift_tab_seq = "\033[Z";
    TEST_ASSERT_EQUAL((int)strlen(shift_tab_seq), write(fds[1], shift_tab_seq, strlen(shift_tab_seq)));
    tui_key_event_t ev4 = tui_term_read_key(fds[0], 100);
    TEST_ASSERT_EQUAL(TUI_KEY_TAB, ev4.type);
    TEST_ASSERT_EQUAL_INT(-1, ev4.ch);

    close(fds[0]);
    close(fds[1]);
#endif
}

void test_tui_engine_multi_column_switching(void) {
    int fds[2];
    TEST_ASSERT_EQUAL(0, pipe(fds));

    FILE *in_fp = fdopen(fds[0], "r");
    TEST_ASSERT_NOT_NULL(in_fp);

    tui_engine_t eng;
    TEST_ASSERT_TRUE(tui_engine_init(&eng, in_fp, false, 1000, 0.0, 1000.0, HISTO_FLAG_TRACK_SUMW2, false));
    TEST_ASSERT_TRUE(tui_engine_start(&eng));

    /* Write 30 rows with 4 columns: Col1=10, Col2=100, Col3=500, Col4=900 */
    for (int i = 0; i < 30; i++) {
        const char *line = "10.0  100.0  500.0  900.0\n";
        TEST_ASSERT_EQUAL((int)strlen(line), write(fds[1], line, strlen(line)));
    }
    close(fds[1]);

    int timeout_ms = 1000;
    while (!tui_engine_is_finished(&eng) && timeout_ms > 0) {
        usleep(10000);
        timeout_ms -= 10;
    }
    usleep(50000);

    /* Initial active column is 1: Mean should be ~10.0 */
    histo_t *snap1 = tui_engine_get_snapshot_1d(&eng);
    TEST_ASSERT_NOT_NULL(snap1);
    double m1 = 0;
    histo_mean(snap1, &m1);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 10.0, m1);
    histo_destroy(snap1);

    /* Switch to column 2: Mean should be ~100.0 */
    histo_t *snap2 = NULL;
    TEST_ASSERT_TRUE(tui_engine_set_column(&eng, 2, 0, 0, &snap2, NULL));
    TEST_ASSERT_NOT_NULL(snap2);
    double m2 = 0;
    histo_mean(snap2, &m2);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 100.0, m2);
    histo_destroy(snap2);

    /* Switch to column 3: Mean should be ~500.0 */
    histo_t *snap3 = NULL;
    TEST_ASSERT_TRUE(tui_engine_set_column(&eng, 3, 0, 0, &snap3, NULL));
    TEST_ASSERT_NOT_NULL(snap3);
    double m3 = 0;
    histo_mean(snap3, &m3);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 500.0, m3);
    histo_destroy(snap3);

    /* Switch to column 4: Mean should be ~900.0 */
    histo_t *snap4 = NULL;
    TEST_ASSERT_TRUE(tui_engine_set_column(&eng, 4, 0, 0, &snap4, NULL));
    TEST_ASSERT_NOT_NULL(snap4);
    double m4 = 0;
    histo_mean(snap4, &m4);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 900.0, m4);
    histo_destroy(snap4);

    tui_engine_free(&eng);
    fclose(in_fp);
}

void test_tui_engine_rolling_window(void) {
    int fds[2];
    TEST_ASSERT_EQUAL(0, pipe(fds));

    FILE *in_fp = fdopen(fds[0], "r");
    TEST_ASSERT_NOT_NULL(in_fp);

    tui_engine_t eng;
    TEST_ASSERT_TRUE(tui_engine_init(&eng, in_fp, false, 110, 0.0, 110.0, 0, false));
    TEST_ASSERT_TRUE(tui_engine_start(&eng));

    /* Write 100 samples from 1.0 to 100.0 */
    for (int i = 1; i <= 100; i++) {
        char buf[32];
        int len = snprintf(buf, sizeof(buf), "%d\n", i);
        TEST_ASSERT_EQUAL(len, write(fds[1], buf, len));
    }
    close(fds[1]);

    int timeout_ms = 1000;
    while (!tui_engine_is_finished(&eng) && timeout_ms > 0) {
        usleep(10000);
        timeout_ms -= 10;
    }
    usleep(50000);

    /* Restrict window to last 10 samples (91 to 100) */
    histo_t *win_snap = NULL;
    TEST_ASSERT_TRUE(tui_engine_set_window(&eng, 10, &win_snap, NULL));
    TEST_ASSERT_NOT_NULL(win_snap);
    TEST_ASSERT_EQUAL_UINT64(10, histo_num_entries(win_snap));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 10.0, histo_total_weight(win_snap));

    double mean = 0.0;
    histo_mean(win_snap, &mean);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 95.5, mean);
    histo_destroy(win_snap);

    /* Reset window to 0 (all samples) */
    histo_t *all_snap = NULL;
    TEST_ASSERT_TRUE(tui_engine_set_window(&eng, 0, &all_snap, NULL));
    TEST_ASSERT_NOT_NULL(all_snap);
    TEST_ASSERT_EQUAL_UINT64(100, histo_num_entries(all_snap));
    histo_destroy(all_snap);

    tui_engine_free(&eng);
    fclose(in_fp);
}

void test_tui_engine_exponential_decay(void) {
    int fds[2];
    TEST_ASSERT_EQUAL(0, pipe(fds));

    FILE *in_fp = fdopen(fds[0], "r");
    TEST_ASSERT_NOT_NULL(in_fp);

    tui_engine_t eng;
    TEST_ASSERT_TRUE(tui_engine_init(&eng, in_fp, false, 50, 0.0, 100.0, 0, false));
    tui_engine_set_decay(&eng, 0.5); /* decay rate lambda = 0.5 */
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 0.5, eng.decay_lambda);

    TEST_ASSERT_TRUE(tui_engine_start(&eng));

    /* Write 10 samples */
    for (int i = 1; i <= 10; i++) {
        char buf[32];
        int len = snprintf(buf, sizeof(buf), "%d\n", i * 5);
        TEST_ASSERT_EQUAL(len, write(fds[1], buf, len));
    }
    close(fds[1]);

    int timeout_ms = 1000;
    while (!tui_engine_is_finished(&eng) && timeout_ms > 0) {
        usleep(10000);
        timeout_ms -= 10;
    }
    usleep(50000);

    histo_t *snap = tui_engine_get_snapshot_1d(&eng);
    TEST_ASSERT_NOT_NULL(snap);
    TEST_ASSERT_TRUE(histo_total_weight(snap) > 0.0);
    histo_destroy(snap);

    tui_engine_free(&eng);
    fclose(in_fp);
}

void test_tui_engine_snapshot_export_roundtrip(void) {
    int fds[2];
    TEST_ASSERT_EQUAL(0, pipe(fds));

    FILE *in_fp = fdopen(fds[0], "r");
    TEST_ASSERT_NOT_NULL(in_fp);

    tui_engine_t eng;
    TEST_ASSERT_TRUE(tui_engine_init(&eng, in_fp, false, 25, 0.0, 100.0, 0, false));
    TEST_ASSERT_TRUE(tui_engine_start(&eng));

    for (int i = 1; i <= 50; i++) {
        char buf[32];
        int len = snprintf(buf, sizeof(buf), "%d\n", i);
        TEST_ASSERT_EQUAL(len, write(fds[1], buf, len));
    }
    close(fds[1]);

    int timeout_ms = 1000;
    while (!tui_engine_is_finished(&eng) && timeout_ms > 0) {
        usleep(10000);
        timeout_ms -= 10;
    }
    usleep(50000);

    /* Export binary snapshot to temp file */
    char bin_path[256];
    snprintf(bin_path, sizeof(bin_path), "%s/test_snapshot_%d.histo", get_temp_dir(), (int)getpid());
    TEST_ASSERT_TRUE(tui_engine_export_snapshot(&eng, bin_path, false));

    /* Read back file and deserialize */
    FILE *bfp = fopen(bin_path, "rb");
    TEST_ASSERT_NOT_NULL(bfp);
    fseek(bfp, 0, SEEK_END);
    long sz = ftell(bfp);
    fseek(bfp, 0, SEEK_SET);
    void *buf = malloc((size_t)sz);
    TEST_ASSERT_EQUAL((size_t)sz, fread(buf, 1, (size_t)sz, bfp));
    fclose(bfp);

    histo_t *deser = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_deserialize_binary(buf, (size_t)sz, &deser));
    TEST_ASSERT_NOT_NULL(deser);
    TEST_ASSERT_EQUAL_UINT64(50, histo_num_entries(deser));
    double d_mean = 0.0;
    histo_mean(deser, &d_mean);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 25.5, d_mean);
    histo_destroy(deser);
    free(buf);
    remove(bin_path);

    /* Export JSON snapshot */
    char json_path[256];
    snprintf(json_path, sizeof(json_path), "%s/test_snapshot_%d.json", get_temp_dir(), (int)getpid());
    TEST_ASSERT_TRUE(tui_engine_export_snapshot(&eng, json_path, true));

    FILE *jfp = fopen(json_path, "r");
    TEST_ASSERT_NOT_NULL(jfp);
    fseek(jfp, 0, SEEK_END);
    long jsz = ftell(jfp);
    fseek(jfp, 0, SEEK_SET);
    char *jbuf = (char *)malloc((size_t)jsz + 1);
    TEST_ASSERT_EQUAL((size_t)jsz, fread(jbuf, 1, (size_t)jsz, jfp));
    jbuf[jsz] = '\0';
    fclose(jfp);

    histo_t *j_deser = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_deserialize_json(jbuf, &j_deser));
    TEST_ASSERT_NOT_NULL(j_deser);
    TEST_ASSERT_EQUAL_UINT64(50, histo_num_entries(j_deser));
    histo_destroy(j_deser);
    free(jbuf);
    remove(json_path);

    tui_engine_free(&eng);
    fclose(in_fp);
}

void test_tui_engine_delimiter_variations(void) {
    int fds[2];
    TEST_ASSERT_EQUAL(0, pipe(fds));

    FILE *in_fp = fdopen(fds[0], "r");
    TEST_ASSERT_NOT_NULL(in_fp);

    tui_engine_t eng;
    TEST_ASSERT_TRUE(tui_engine_init(&eng, in_fp, false, 50, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2, false));
    TEST_ASSERT_TRUE(tui_engine_start(&eng));

    /* Stream diverse delimiter patterns, comments, blanks, malformed tokens */
    const char *lines = 
        "# Initial comment line\n"
        "   \n"  /* Blank line */
        "10.0, 20.0, 30.0\n"               /* Commas */
        "15.0; 25.0; 35.0\n"               /* Semicolons */
        "20.0\t30.0\t40.0\n"               /* Tabs */
        "  25.0    35.0    45.0  \n"       /* Multiple spaces with leading/trailing */
        "30.0,  40.0; \t 50.0  # mixed\n"  /* Mixed delimiters and inline comment */
        "35.0, corrupt_text, 55.0\n"       /* Mid-line non-numeric token */
        "40.0\n";                          /* Single column line */

    TEST_ASSERT_EQUAL((int)strlen(lines), write(fds[1], lines, strlen(lines)));
    close(fds[1]);

    int timeout_ms = 1000;
    while (!tui_engine_is_finished(&eng) && timeout_ms > 0) {
        usleep(10000);
        timeout_ms -= 10;
    }
    usleep(50000);

    histo_t *snap1 = tui_engine_get_snapshot_1d(&eng);
    TEST_ASSERT_NOT_NULL(snap1);
    /* There should be 7 valid sample lines parsed */
    TEST_ASSERT_EQUAL_UINT64(7, histo_num_entries(snap1));
    histo_destroy(snap1);

    /* Switch to column 2 */
    histo_t *snap2 = NULL;
    TEST_ASSERT_TRUE(tui_engine_set_column(&eng, 2, 0, 0, &snap2, NULL));
    TEST_ASSERT_NOT_NULL(snap2);
    /* For column 2:
     * Line 1: 20
     * Line 2: 25
     * Line 3: 30
     * Line 4: 35
     * Line 5: 40
     * Line 6: 0.0 (corrupt_text terminated parsing, fallback/zero padded)
     * Line 7: 40.0 (single col falls back to col 1)
     */
    TEST_ASSERT_EQUAL_UINT64(7, histo_num_entries(snap2));
    histo_destroy(snap2);

    /* Out of bounds column selection (e.g. column 10 -> fallback to column 1) */
    histo_t *snap_fallback = NULL;
    TEST_ASSERT_TRUE(tui_engine_set_column(&eng, 10, 0, 0, &snap_fallback, NULL));
    TEST_ASSERT_NOT_NULL(snap_fallback);
    TEST_ASSERT_EQUAL_UINT64(7, histo_num_entries(snap_fallback));
    histo_destroy(snap_fallback);

    tui_engine_free(&eng);
    fclose(in_fp);
}

void test_tui_engine_windowing_extremes(void) {
    int fds[2];
    TEST_ASSERT_EQUAL(0, pipe(fds));

    FILE *in_fp = fdopen(fds[0], "r");
    TEST_ASSERT_NOT_NULL(in_fp);

    tui_engine_t eng;
    TEST_ASSERT_TRUE(tui_engine_init(&eng, in_fp, false, 50, 0.0, 3000.0, HISTO_FLAG_EXACT_MOMENTS, false));
    TEST_ASSERT_TRUE(tui_engine_start(&eng));

    /* Ingest 2,500 samples */
    const int total_samples = 2500;
    for (int i = 1; i <= total_samples; i++) {
        char buf[32];
        int len = snprintf(buf, sizeof(buf), "%d\n", i);
        TEST_ASSERT_EQUAL(len, write(fds[1], buf, len));
    }
    close(fds[1]);

    int timeout_ms = 3000;
    while (!tui_engine_is_finished(&eng) && timeout_ms > 0) {
        usleep(10000);
        timeout_ms -= 10;
    }
    usleep(50000);

    /* 1. Window size = 1 (only the very latest sample: 2500) */
    histo_t *win1 = NULL;
    TEST_ASSERT_TRUE(tui_engine_set_window(&eng, 1, &win1, NULL));
    TEST_ASSERT_NOT_NULL(win1);
    TEST_ASSERT_EQUAL_UINT64(1, histo_num_entries(win1));
    double m1 = 0;
    histo_mean(win1, &m1);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 2500.0, m1);
    histo_destroy(win1);

    /* 2. Window size = 100 (samples 2401 to 2500, mean = 2450.5) */
    histo_t *win100 = NULL;
    TEST_ASSERT_TRUE(tui_engine_set_window(&eng, 100, &win100, NULL));
    TEST_ASSERT_NOT_NULL(win100);
    TEST_ASSERT_EQUAL_UINT64(100, histo_num_entries(win100));
    double m100 = 0;
    histo_mean(win100, &m100);
    TEST_ASSERT_DOUBLE_WITHIN(2.0, 2450.5, m100);
    histo_destroy(win100);

    /* 3. Window size exceeding total sample count (e.g. 50000 -> clamps to total samples 2500) */
    histo_t *win_excess = NULL;
    TEST_ASSERT_TRUE(tui_engine_set_window(&eng, 50000, &win_excess, NULL));
    TEST_ASSERT_NOT_NULL(win_excess);
    TEST_ASSERT_EQUAL_UINT64(2500, histo_num_entries(win_excess));
    double m_excess = 0;
    histo_mean(win_excess, &m_excess);
    TEST_ASSERT_DOUBLE_WITHIN(5.0, 1250.5, m_excess);
    histo_destroy(win_excess);

    tui_engine_free(&eng);
    fclose(in_fp);
}

void test_tui_engine_2d_snapshot_export_roundtrip(void) {
    int fds[2];
    TEST_ASSERT_EQUAL(0, pipe(fds));

    FILE *in_fp = fdopen(fds[0], "r");
    TEST_ASSERT_NOT_NULL(in_fp);

    tui_engine_t eng;
    TEST_ASSERT_TRUE(tui_engine_init(&eng, in_fp, true, 20, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2, false));
    TEST_ASSERT_TRUE(tui_engine_start(&eng));

    for (int i = 1; i <= 40; i++) {
        char buf[64];
        int len = snprintf(buf, sizeof(buf), "%d %d\n", i * 2, i * 2);
        TEST_ASSERT_EQUAL(len, write(fds[1], buf, len));
    }
    close(fds[1]);

    int timeout_ms = 1000;
    while (!tui_engine_is_finished(&eng) && timeout_ms > 0) {
        usleep(10000);
        timeout_ms -= 10;
    }
    usleep(50000);

    /* Export 2D binary snapshot */
    char bin_path[256];
    snprintf(bin_path, sizeof(bin_path), "%s/test_2d_snapshot_%d.histo", get_temp_dir(), (int)getpid());
    TEST_ASSERT_TRUE(tui_engine_export_snapshot(&eng, bin_path, false));

    FILE *bfp = fopen(bin_path, "rb");
    TEST_ASSERT_NOT_NULL(bfp);
    fseek(bfp, 0, SEEK_END);
    long sz = ftell(bfp);
    fseek(bfp, 0, SEEK_SET);
    void *buf = malloc((size_t)sz);
    TEST_ASSERT_EQUAL((size_t)sz, fread(buf, 1, (size_t)sz, bfp));
    fclose(bfp);

    histo2d_t *deser2d = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_deserialize_binary(buf, (size_t)sz, &deser2d));
    TEST_ASSERT_NOT_NULL(deser2d);
    TEST_ASSERT_EQUAL_UINT64(40, histo2d_num_entries(deser2d));
    TEST_ASSERT_EQUAL_UINT32(20, histo2d_nbins_x(deser2d));
    TEST_ASSERT_EQUAL_UINT32(20, histo2d_nbins_y(deser2d));
    histo2d_destroy(deser2d);
    free(buf);
    remove(bin_path);

    /* Export 2D JSON snapshot */
    char json_path[256];
    snprintf(json_path, sizeof(json_path), "%s/test_2d_snapshot_%d.json", get_temp_dir(), (int)getpid());
    TEST_ASSERT_TRUE(tui_engine_export_snapshot(&eng, json_path, true));

    FILE *jfp = fopen(json_path, "r");
    TEST_ASSERT_NOT_NULL(jfp);
    fseek(jfp, 0, SEEK_END);
    long jsz = ftell(jfp);
    fseek(jfp, 0, SEEK_SET);
    char *jbuf = (char *)malloc((size_t)jsz + 1);
    TEST_ASSERT_EQUAL((size_t)jsz, fread(jbuf, 1, (size_t)jsz, jfp));
    jbuf[jsz] = '\0';
    fclose(jfp);

    histo2d_t *j_deser2d = NULL;
    TEST_ASSERT_EQUAL(HISTO_OK, histo2d_deserialize_json(jbuf, &j_deser2d));
    TEST_ASSERT_NOT_NULL(j_deser2d);
    TEST_ASSERT_EQUAL_UINT64(40, histo2d_num_entries(j_deser2d));
    histo2d_destroy(j_deser2d);
    free(jbuf);
    remove(json_path);

    tui_engine_free(&eng);
    fclose(in_fp);
}

void test_tui_engine_stress_concurrent_ops(void) {
    int fds[2];
    TEST_ASSERT_EQUAL(0, pipe(fds));

    FILE *in_fp = fdopen(fds[0], "r");
    TEST_ASSERT_NOT_NULL(in_fp);

    tui_engine_t eng;
    TEST_ASSERT_TRUE(tui_engine_init(&eng, in_fp, false, 50, 0.0, 1000.0, 0, false));
    TEST_ASSERT_TRUE(tui_engine_start(&eng));

    /* Concurrently write to pipe while driving TUI operations */
    for (int iter = 0; iter < 500; iter++) {
        char buf[64];
        int len = snprintf(buf, sizeof(buf), "%d  %d  %d\n", iter, iter * 2, iter * 3);
        TEST_ASSERT_EQUAL(len, write(fds[1], buf, len));

        if (iter % 25 == 0) {
            histo_t *snap = NULL;
            tui_engine_zoom_1d(&eng, 0.9, &snap);
            if (snap) histo_destroy(snap);

            tui_engine_pan_1d(&eng, 0.05, &snap);
            if (snap) histo_destroy(snap);

            tui_engine_set_column(&eng, (iter % 3) + 1, 0, 0, &snap, NULL);
            if (snap) histo_destroy(snap);

            tui_engine_set_window(&eng, 50, &snap, NULL);
            if (snap) histo_destroy(snap);

            tui_engine_set_decay(&eng, 0.01);
        }
    }
    close(fds[1]);

    int timeout_ms = 2000;
    while (!tui_engine_is_finished(&eng) && timeout_ms > 0) {
        usleep(10000);
        timeout_ms -= 10;
    }
    usleep(50000);

    histo_t *final_snap = tui_engine_get_snapshot_1d(&eng);
    TEST_ASSERT_NOT_NULL(final_snap);
    TEST_ASSERT_TRUE(histo_num_entries(final_snap) > 0);
    histo_destroy(final_snap);

    tui_engine_free(&eng);
    fclose(in_fp);
}

void test_tui_engine_binary_streaming(void) {
    int fds[2];
    TEST_ASSERT_EQUAL(0, pipe(fds));

    FILE *in_fp = fdopen(fds[0], "rb");
    TEST_ASSERT_NOT_NULL(in_fp);

    tui_engine_t eng;
    TEST_ASSERT_TRUE(tui_engine_init(&eng, in_fp, false, 20, 0.0, 110.0, HISTO_FLAG_TRACK_SUMW2, false));
    tui_engine_set_binary(&eng, true);
    TEST_ASSERT_TRUE(tui_engine_start(&eng));

    /* Write 100 binary doubles */
    double raw_data[100];
    for (int i = 0; i < 100; ++i) {
        raw_data[i] = (double)(i + 1);
    }
    ssize_t written = write(fds[1], raw_data, sizeof(raw_data));
    TEST_ASSERT_EQUAL((ssize_t)sizeof(raw_data), written);
    close(fds[1]);

    int timeout_ms = 1000;
    while (!tui_engine_is_finished(&eng) && timeout_ms > 0) {
        usleep(10000);
        timeout_ms -= 10;
    }
    usleep(50000);

    histo_t *snap = tui_engine_get_snapshot_1d(&eng);
    TEST_ASSERT_NOT_NULL(snap);
    TEST_ASSERT_EQUAL_UINT64(100, histo_num_entries(snap));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 100.0, histo_total_weight(snap));

    double mean = 0.0;
    TEST_ASSERT_EQUAL(HISTO_OK, histo_mean(snap, &mean));
    TEST_ASSERT_DOUBLE_WITHIN(0.5, 50.5, mean);
    histo_destroy(snap);

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
    RUN_TEST(test_tui_engine_binary_streaming);
    RUN_TEST(test_tui_engine_autorange_p1_p99);
    RUN_TEST(test_tui_engine_weights);
    RUN_TEST(test_tui_engine_zoom_pan_1d);
    RUN_TEST(test_tui_engine_zoom_pan_2d);
    RUN_TEST(test_tui_term_mouse_and_shift_tab_parsing);
    RUN_TEST(test_tui_engine_multi_column_switching);
    RUN_TEST(test_tui_engine_rolling_window);
    RUN_TEST(test_tui_engine_exponential_decay);
    RUN_TEST(test_tui_engine_snapshot_export_roundtrip);
    RUN_TEST(test_tui_engine_delimiter_variations);
    RUN_TEST(test_tui_engine_windowing_extremes);
    RUN_TEST(test_tui_engine_2d_snapshot_export_roundtrip);
    RUN_TEST(test_tui_engine_stress_concurrent_ops);
    return UNITY_END();
}

