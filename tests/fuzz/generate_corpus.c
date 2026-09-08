/*
 * Corpus generator constructing valid seed inputs for fuzz testing targets.
 */

#include "histo/histo.h"
#include "histo/histo2d.h"
#include "histo/sketch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if defined(_WIN32)
#include <direct.h>
#define mkdir_p(p) _mkdir(p)
#else
#include <sys/stat.h>
#define mkdir_p(p) mkdir(p, 0755)
#endif

static void write_file(const char *path, const void *data, size_t size) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "Failed to open %s for writing\n", path);
        return;
    }
    if (size > 0 && data) {
        fwrite(data, 1, size, f);
    }
    fclose(f);
    printf("Wrote %s (%zu bytes)\n", path, size);
}

int main(void) {
    mkdir_p("tests/fuzz/corpus");
    mkdir_p("tests/fuzz/corpus/binary");
    mkdir_p("tests/fuzz/corpus/json");
    mkdir_p("tests/fuzz/corpus/sketch");
    mkdir_p("tests/fuzz/corpus/fill");
    mkdir_p("tests/fuzz/corpus/binary_2d");
    mkdir_p("tests/fuzz/corpus/json_2d");
    mkdir_p("tests/fuzz/corpus/fit");
    mkdir_p("tests/fuzz/corpus/bpftrace");
    mkdir_p("tests/fuzz/corpus/cli_opt");
    mkdir_p("tests/fuzz/corpus/tui_command");
    mkdir_p("tests/fuzz/corpus/kde");
    mkdir_p("tests/fuzz/corpus/auto_bin");
    mkdir_p("tests/fuzz/corpus/simd_diff");
    mkdir_p("tests/fuzz/corpus/roundtrip");

    /* ===================================================================== */
    /* Binary Corpus                                                         */
    /* ===================================================================== */
    /* 1. Uniform v2 binary */
    histo_t *h_unif = histo_create_uniform(20, 0.0, 100.0, HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS);
    for (int i = 0; i < 100; i++) histo_fill_w(h_unif, (double)i, 1.5);
    void *buf_unif = NULL;
    size_t sz_unif = 0;
    histo_serialize_binary(h_unif, &buf_unif, &sz_unif);
    write_file("tests/fuzz/corpus/binary/uniform_v2.bin", buf_unif, sz_unif);
    histo_free_buffer(buf_unif);

    /* 2. Variable v2 binary */
    double edges[] = {0.0, 5.0, 15.0, 30.0, 60.0, 100.0};
    histo_t *h_var = histo_create_variable(5, edges, HISTO_FLAG_NONE);
    for (int i = 0; i < 50; i++) histo_fill(h_var, (double)(i * 2));
    void *buf_var = NULL;
    size_t sz_var = 0;
    histo_serialize_binary(h_var, &buf_var, &sz_var);
    write_file("tests/fuzz/corpus/binary/variable_v2.bin", buf_var, sz_var);
    histo_free_buffer(buf_var);

    /* 3. Format V1 binary (to seed migration) */
    void *buf_v1 = NULL;
    size_t sz_v1 = 0;
    histo_serialize_binary(h_unif, &buf_v1, &sz_v1);
    /* Change version header field at offset 0x08 to 1 */
    ((uint8_t*)buf_v1)[8] = 1;
    ((uint8_t*)buf_v1)[9] = 0;
    write_file("tests/fuzz/corpus/binary/uniform_v1_migration.bin", buf_v1, sz_v1);
    histo_free_buffer(buf_v1);

    /* 4. Corrupt magic */
    uint8_t corrupt_hdr[256];
    memset(corrupt_hdr, 0xAA, sizeof(corrupt_hdr));
    write_file("tests/fuzz/corpus/binary/corrupt_header.bin", corrupt_hdr, sizeof(corrupt_hdr));

    /* 5. Truncated binary */
    write_file("tests/fuzz/corpus/binary/truncated_header.bin", corrupt_hdr, 64);

    /* ===================================================================== */
    /* JSON Corpus                                                           */
    /* ===================================================================== */
    char *json_unif = NULL;
    histo_serialize_json(h_unif, &json_unif);
    write_file("tests/fuzz/corpus/json/uniform.json", json_unif, strlen(json_unif));
    histo_free_buffer(json_unif);

    char *json_var = NULL;
    histo_serialize_json(h_var, &json_var);
    write_file("tests/fuzz/corpus/json/variable.json", json_var, strlen(json_var));
    histo_free_buffer(json_var);

    const char *json_edge = "{\"nbins\": 4, \"min\": 0.0, \"max\": 40.0, \"bins\": [1.0, 2.0, 3.0, 4.0], \"total\": {\"weight\": 10.0, \"entries\": 4}}";
    write_file("tests/fuzz/corpus/json/simple_valid.json", json_edge, strlen(json_edge));

    const char *json_malformed = "{\"nbins\": 10, \"min\": 0.0, \"max\": [invalid";
    write_file("tests/fuzz/corpus/json/malformed_syntax.json", json_malformed, strlen(json_malformed));

    const char *json_nested = "{\"a\":{\"b\":{\"c\":{\"nbins\":10,\"bins\":[]}}}}";
    write_file("tests/fuzz/corpus/json/nested_adversarial.json", json_nested, strlen(json_nested));

    /* ===================================================================== */
    /* Sketch Corpus                                                         */
    /* ===================================================================== */
    histo_sketch_t *sketch = histo_sketch_create(0.01, 512);
    for (double v = -50.0; v <= 50.0; v += 0.5) {
        histo_sketch_insert(sketch, v);
    }
    void *sketch_buf = NULL;
    size_t sketch_sz = 0;
    histo_sketch_serialize_binary(sketch, &sketch_buf, &sketch_sz);
    write_file("tests/fuzz/corpus/sketch/valid_sketch.bin", sketch_buf, sketch_sz);
    if (sketch_buf) free(sketch_buf);
    histo_sketch_destroy(sketch);

    uint8_t trunc_sketch[16] = {0x01, 0x02, 0x03, 0x04};
    write_file("tests/fuzz/corpus/sketch/truncated_sketch.bin", trunc_sketch, sizeof(trunc_sketch));

    /* ===================================================================== */
    /* Fill Stream Corpus                                                    */
    /* ===================================================================== */
    /* 1. Normal uniform sample stream */
    uint8_t fill_unif[256];
    memset(fill_unif, 0, sizeof(fill_unif));
    fill_unif[0] = 0x00; /* Uniform */
    fill_unif[1] = 0x03; /* Flags: SUMW2 | EXACT_MOMENTS */
    uint16_t nb = 10;
    memcpy(fill_unif + 2, &nb, 2);
    double min_d = -50.0, max_d = 50.0;
    memcpy(fill_unif + 4, &min_d, 8);
    memcpy(fill_unif + 12, &max_d, 8);
    for (int i = 0; i < 20; i++) {
        double d = (double)(i - 10) * 4.5;
        memcpy(fill_unif + 20 + (i * 8), &d, 8);
    }
    write_file("tests/fuzz/corpus/fill/uniform_stream.bin", fill_unif, sizeof(fill_unif));

    /* 2. Special IEEE-754 numbers */
    uint8_t ieee_stream[256];
    memset(ieee_stream, 0, sizeof(ieee_stream));
    ieee_stream[0] = 0x00;
    ieee_stream[1] = 0x03;
    nb = 20;
    memcpy(ieee_stream + 2, &nb, 2);
    double min_v = 0.0, max_v = 100.0;
    memcpy(ieee_stream + 4, &min_v, 8);
    memcpy(ieee_stream + 12, &max_v, 8);
    double specials[] = {
        0.0, -0.0, INFINITY, -INFINITY, NAN, -NAN,
        1e-315, 5e-324, 1e308, -1e308, 1e-308, 42.0,
        100.0, -1.0, 101.0, 99.9999999999
    };
    memcpy(ieee_stream + 20, specials, sizeof(specials));
    write_file("tests/fuzz/corpus/fill/ieee754_specials.bin", ieee_stream, 20 + sizeof(specials));


    /* ===================================================================== */
    /* 2D Binary Corpus                                                      */
    /* ===================================================================== */
    histo2d_t *h2_unif = histo2d_create_uniform(10, 0.0, 10.0, 10, 0.0, 10.0, HISTO_FLAG_TRACK_SUMW2);
    for (int i = 0; i < 50; i++) histo2d_fill(h2_unif, i % 10, i % 10);
    void *buf2_unif = NULL;
    size_t sz2_unif = 0;
    histo2d_serialize_binary_alloc(h2_unif, &buf2_unif, &sz2_unif);
    write_file("tests/fuzz/corpus/binary_2d/uniform_2d.bin", buf2_unif, sz2_unif);
    
    /* 2D JSON Corpus */
    char *json2_unif = NULL;
    size_t out_sz2 = 0;
    histo2d_serialize_json_alloc(h2_unif, &json2_unif, &out_sz2);
    write_file("tests/fuzz/corpus/json_2d/uniform_2d.json", json2_unif, strlen(json2_unif));
    
    if (buf2_unif) histo_free_buffer(buf2_unif);
    if (json2_unif) histo_free_buffer(json2_unif);
    histo2d_destroy(h2_unif);
    
    /* ===================================================================== */
    /* Fit Corpus                                                            */
    /* ===================================================================== */
    uint8_t fit_seed[256];
    memset(fit_seed, 0, sizeof(fit_seed));
    fit_seed[0] = 0; // type 0
    for(int i=1; i<256; i++) fit_seed[i] = (uint8_t)i;
    write_file("tests/fuzz/corpus/fit/seed1.bin", fit_seed, sizeof(fit_seed));

    /* ===================================================================== */
    /* bpftrace Corpus                                                       */
    /* ===================================================================== */
    const char *bpf_pow2 =
        "@vfs_read_latency:\n"
        "[0]                    2 |@@@@@@                                  |\n"
        "[1]                    1 |@@@                                     |\n"
        "[2, 4)                 4 |@@@@@@@@@@@@                            |\n"
        "[4, 8)                12 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@    |\n"
        "[8, 16)                7 |@@@@@@@@@@@@@@@@@@@                     |\n";
    write_file("tests/fuzz/corpus/bpftrace/power2.txt", bpf_pow2, strlen(bpf_pow2));

    const char *bpf_linear =
        "@bytes:\n"
        "[0, 10) 100\n"
        "[10, 20) 250\n"
        "[20, 30) 75\n";
    write_file("tests/fuzz/corpus/bpftrace/linear.txt", bpf_linear, strlen(bpf_linear));

    const char *bpf_bcc =
        "0 -> 1 : 10 |***|\n"
        "2 -> 3 : 25 |*******|\n"
        "4 -> 5 : 5  |*|\n";
    write_file("tests/fuzz/corpus/bpftrace/bcc.txt", bpf_bcc, strlen(bpf_bcc));

    /* ===================================================================== */
    /* CLI Options Corpus                                                    */
    /* ===================================================================== */
    const char *cli_basic = "--bins 100 --min -10.5 --max 50.0 -v";
    write_file("tests/fuzz/corpus/cli_opt/basic.txt", cli_basic, strlen(cli_basic));

    const char *cli_bundled = "-b 200 -ve -f json";
    write_file("tests/fuzz/corpus/cli_opt/bundled.txt", cli_bundled, strlen(cli_bundled));

    const char *cli_equals = "--bins=50 --min=0.0 --max=100.0 --exact";
    write_file("tests/fuzz/corpus/cli_opt/equals.txt", cli_equals, strlen(cli_equals));

    /* ===================================================================== */
    /* TUI Command Corpus                                                    */
    /* ===================================================================== */
    const char *tui_cmd1 = ":bins 100\n";
    write_file("tests/fuzz/corpus/tui_command/bins.txt", tui_cmd1, strlen(tui_cmd1));

    const char *tui_cmd2 = ":range -20.0 80.0\n";
    write_file("tests/fuzz/corpus/tui_command/range.txt", tui_cmd2, strlen(tui_cmd2));

    const char *tui_cmd3 = ":window 500\n:decay 0.05\n:col 2\n:palette plasma\n";
    write_file("tests/fuzz/corpus/tui_command/multi.txt", tui_cmd3, strlen(tui_cmd3));

    /* ===================================================================== */
    /* KDE Corpus                                                            */
    /* ===================================================================== */
#define KDE_SEED_BUF_SZ (10 + 32 * sizeof(double))
    uint8_t kde_buf1[KDE_SEED_BUF_SZ];
    kde_buf1[0] = 0; // Gaussian
    kde_buf1[1] = 0; // Silverman
    double kde_bw = 1.0;
    memcpy(kde_buf1 + 2, &kde_bw, sizeof(double));
    for (int i = 0; i < 32; i++) {
        double val = -3.0 + (double)i * 0.2;
        memcpy(kde_buf1 + 10 + i * sizeof(double), &val, sizeof(double));
    }
    write_file("tests/fuzz/corpus/kde/seed_gaussian.bin", kde_buf1, sizeof(kde_buf1));

    /* ===================================================================== */
    /* Auto-Bin Corpus                                                       */
    /* ===================================================================== */
    double auto_samples[64];
    for (int i = 0; i < 64; i++) {
        auto_samples[i] = sin((double)i) * 10.0 + 20.0;
    }
    write_file("tests/fuzz/corpus/auto_bin/seed_normal.bin", auto_samples, sizeof(auto_samples));

    /* ===================================================================== */
    /* SIMD Diff Corpus                                                      */
    /* ===================================================================== */
#define SIMD_SEED_BUF_SZ (18 + 64 * sizeof(double))
    uint8_t simd_buf[SIMD_SEED_BUF_SZ];
    simd_buf[0] = 0; // offset 0
    simd_buf[1] = 32; // 32 bins
    double s_rmin = -50.0, s_rmax = 50.0;
    memcpy(simd_buf + 2, &s_rmin, sizeof(double));
    memcpy(simd_buf + 10, &s_rmax, sizeof(double));
    for (int i = 0; i < 64; i++) {
        double val = -40.0 + (double)i * 1.25;
        memcpy(simd_buf + 18 + i * sizeof(double), &val, sizeof(double));
    }
    write_file("tests/fuzz/corpus/simd_diff/seed_aligned.bin", simd_buf, sizeof(simd_buf));

    simd_buf[0] = 3; // offset 3 (unaligned)
    write_file("tests/fuzz/corpus/simd_diff/seed_unaligned.bin", simd_buf, sizeof(simd_buf));

    /* ===================================================================== */
    /* Roundtrip Corpus                                                      */
    /* ===================================================================== */
    void *rt_1d = NULL;
    size_t rt_1d_sz = 0;
    histo_serialize_binary(h_unif, &rt_1d, &rt_1d_sz);
    if (rt_1d) {
        write_file("tests/fuzz/corpus/roundtrip/seed_1d.bin", rt_1d, rt_1d_sz);
        histo_free_buffer(rt_1d);
    }

    histo_destroy(h_unif);
    histo_destroy(h_var);

    return 0;
}
