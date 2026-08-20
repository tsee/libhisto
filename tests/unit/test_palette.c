#include "unity.h"
#include "cli_palette.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

void setUp(void) {}
void tearDown(void) {}

void test_palette_name_parsing(void) {
    /* Standard names */
    TEST_ASSERT_EQUAL(HISTO_PALETTE_VIRIDIS, histo_palette_from_name("viridis"));
    TEST_ASSERT_EQUAL(HISTO_PALETTE_PLASMA, histo_palette_from_name("plasma"));
    TEST_ASSERT_EQUAL(HISTO_PALETTE_INFERNO, histo_palette_from_name("inferno"));
    TEST_ASSERT_EQUAL(HISTO_PALETTE_MAGMA, histo_palette_from_name("magma"));
    TEST_ASSERT_EQUAL(HISTO_PALETTE_TURBO, histo_palette_from_name("turbo"));
    TEST_ASSERT_EQUAL(HISTO_PALETTE_CIVIDIS, histo_palette_from_name("cividis"));
    TEST_ASSERT_EQUAL(HISTO_PALETTE_GRAYSCALE, histo_palette_from_name("grayscale"));
    TEST_ASSERT_EQUAL(HISTO_PALETTE_GRAYSCALE, histo_palette_from_name("gray"));
    TEST_ASSERT_EQUAL(HISTO_PALETTE_GRAYSCALE, histo_palette_from_name("grey"));
    TEST_ASSERT_EQUAL(HISTO_PALETTE_GRAYSCALE, histo_palette_from_name("mono"));
    TEST_ASSERT_EQUAL(HISTO_PALETTE_RAINBOW, histo_palette_from_name("rainbow"));
    TEST_ASSERT_EQUAL(HISTO_PALETTE_RAINBOW, histo_palette_from_name("spectral"));

    /* Case-insensitivity */
    TEST_ASSERT_EQUAL(HISTO_PALETTE_VIRIDIS, histo_palette_from_name("VIRIDIS"));
    TEST_ASSERT_EQUAL(HISTO_PALETTE_PLASMA, histo_palette_from_name("Plasma"));
    TEST_ASSERT_EQUAL(HISTO_PALETTE_INFERNO, histo_palette_from_name("InFeRnO"));
    TEST_ASSERT_EQUAL(HISTO_PALETTE_TURBO, histo_palette_from_name("TURBO"));

    /* Fallback on invalid / NULL */
    TEST_ASSERT_EQUAL(HISTO_PALETTE_VIRIDIS, histo_palette_from_name(NULL));
    TEST_ASSERT_EQUAL(HISTO_PALETTE_VIRIDIS, histo_palette_from_name(""));
    TEST_ASSERT_EQUAL(HISTO_PALETTE_VIRIDIS, histo_palette_from_name("nonexistent_colormap_xyz"));
}

void test_palette_name_strings(void) {
    TEST_ASSERT_EQUAL_STRING("viridis", histo_palette_name(HISTO_PALETTE_VIRIDIS));
    TEST_ASSERT_EQUAL_STRING("plasma", histo_palette_name(HISTO_PALETTE_PLASMA));
    TEST_ASSERT_EQUAL_STRING("inferno", histo_palette_name(HISTO_PALETTE_INFERNO));
    TEST_ASSERT_EQUAL_STRING("magma", histo_palette_name(HISTO_PALETTE_MAGMA));
    TEST_ASSERT_EQUAL_STRING("turbo", histo_palette_name(HISTO_PALETTE_TURBO));
    TEST_ASSERT_EQUAL_STRING("cividis", histo_palette_name(HISTO_PALETTE_CIVIDIS));
    TEST_ASSERT_EQUAL_STRING("grayscale", histo_palette_name(HISTO_PALETTE_GRAYSCALE));
    TEST_ASSERT_EQUAL_STRING("rainbow", histo_palette_name(HISTO_PALETTE_RAINBOW));
    TEST_ASSERT_EQUAL_STRING("viridis", histo_palette_name((histo_palette_t)999));
}

void test_palette_rgb_sampling_bounds(void) {
    int r = -1, g = -1, b = -1;

    for (int p = 0; p < (int)HISTO_PALETTE_COUNT; ++p) {
        histo_palette_t pal = (histo_palette_t)p;

        /* Clamping tests: < 0.0 and > 1.0 */
        histo_palette_sample_rgb(pal, -0.5, &r, &g, &b);
        TEST_ASSERT_TRUE(r >= 0 && r <= 255);
        TEST_ASSERT_TRUE(g >= 0 && g <= 255);
        TEST_ASSERT_TRUE(b >= 0 && b <= 255);

        int r0 = -1, g0 = -1, b0 = -1;
        histo_palette_sample_rgb(pal, 0.0, &r0, &g0, &b0);
        TEST_ASSERT_EQUAL(r0, r);
        TEST_ASSERT_EQUAL(g0, g);
        TEST_ASSERT_EQUAL(b0, b);

        histo_palette_sample_rgb(pal, 1.5, &r, &g, &b);
        TEST_ASSERT_TRUE(r >= 0 && r <= 255);
        TEST_ASSERT_TRUE(g >= 0 && g <= 255);
        TEST_ASSERT_TRUE(b >= 0 && b <= 255);

        int r1 = -1, g1 = -1, b1 = -1;
        histo_palette_sample_rgb(pal, 1.0, &r1, &g1, &b1);
        TEST_ASSERT_EQUAL(r1, r);
        TEST_ASSERT_EQUAL(g1, g);
        TEST_ASSERT_EQUAL(b1, b);

        /* Smooth progression check across 100 intervals */
        for (int step = 0; step <= 100; ++step) {
            double frac = (double)step / 100.0;
            histo_palette_sample_rgb(pal, frac, &r, &g, &b);
            TEST_ASSERT_TRUE(r >= 0 && r <= 255);
            TEST_ASSERT_TRUE(g >= 0 && g <= 255);
            TEST_ASSERT_TRUE(b >= 0 && b <= 255);
        }
    }
}

void test_palette_grayscale_consistency(void) {
    int r = 0, g = 0, b = 0;

    for (int step = 0; step <= 20; ++step) {
        double frac = (double)step / 20.0;
        histo_palette_sample_rgb(HISTO_PALETTE_GRAYSCALE, frac, &r, &g, &b);
        TEST_ASSERT_EQUAL(r, g);
        TEST_ASSERT_EQUAL(g, b);
        int expected = (int)(frac * 255.0 + 0.5);
        TEST_ASSERT_EQUAL(expected, r);
    }
}

void test_palette_ansi_formatting(void) {
    char buf[64];

    /* Foreground ANSI */
    histo_palette_sample_ansi_fg(HISTO_PALETTE_VIRIDIS, 0.0, buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\033[38;2;"));
    TEST_ASSERT_EQUAL('m', buf[strlen(buf) - 1]);

    /* Background ANSI */
    histo_palette_sample_ansi_bg(HISTO_PALETTE_PLASMA, 1.0, buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\033[48;2;"));
    TEST_ASSERT_EQUAL('m', buf[strlen(buf) - 1]);

    /* NULL / small buffer safety */
    histo_palette_sample_ansi_fg(HISTO_PALETTE_TURBO, 0.5, NULL, 0);
    histo_palette_sample_ansi_bg(HISTO_PALETTE_TURBO, 0.5, buf, 0);
    histo_palette_sample_ansi_fg(HISTO_PALETTE_TURBO, 0.5, buf, 5);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_palette_name_parsing);
    RUN_TEST(test_palette_name_strings);
    RUN_TEST(test_palette_rgb_sampling_bounds);
    RUN_TEST(test_palette_grayscale_consistency);
    RUN_TEST(test_palette_ansi_formatting);
    return UNITY_END();
}
