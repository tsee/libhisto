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
    for(int i=1; i<256; i++) fit_seed[i] = i;
    write_file("tests/fuzz/corpus/fit/seed1.bin", fit_seed, sizeof(fit_seed));

    histo_destroy(h_unif);
    histo_destroy(h_var);

    return 0;
}
