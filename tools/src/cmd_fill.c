#include "cli_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>

static void print_fill_usage(void) {
    printf("Usage: histo-fill [OPTIONS] [FILE...]\n");
    printf("       histo fill [OPTIONS] [FILE...]\n\n");
    printf("Reads streaming data, aggregates into a histogram, and emits the serialized result.\n\n");
    printf("Geometry Options:\n");
    printf("  -n, --bins=<N>           Number of uniform bins (default: 50)\n");
    printf("      --min=<X>            Lower boundary (required unless --auto-range)\n");
    printf("      --max=<X>            Upper boundary (required unless --auto-range)\n");
    printf("      --edges=<E0,E1,...>  Variable bin edges (comma-separated)\n");
    printf("      --auto-range         Buffer input to determine min/max automatically\n\n");
    printf("Input Parsing Options:\n");
    printf("  -w, --weights            Input contains weights: reads 'x weight' pairs\n");
    printf("      --value-col=<COL>    1-based column for sample coordinate (default: 1)\n");
    printf("      --weights-col=<COL>  1-based column for sample weight (default: 2)\n");
    printf("      --delimiter=<CHAR>   Field delimiter character (default: whitespace)\n");
    printf("      --binary-f64         Read raw Little-Endian double binary stream\n");
    printf("      --merge              Read and add/merge incoming serialized histograms\n\n");
    printf("Histogram Features & Transformations:\n");
    printf("      --sumw2              Enable sum_w2 error tracking (default: ON)\n");
    printf("      --no-sumw2           Disable sum_w2 error tracking\n");
    printf("      --exact-moments      Enable online exact Welford moments\n");
    printf("      --rebin=<FACTOR>     Rebin uniform histogram by integer factor\n");
    printf("      --slice=<MIN:MAX>    Slice bin sub-range [MIN, MAX]\n");
    printf("      --cdf                Generate Cumulative Distribution Function (CDF)\n");
    printf("      --normalize=<AREA>   Scale histogram total weight to target area\n\n");
    printf("Output & Streaming Options:\n");
    printf("  -o, --output=<FORMAT>    Output format: binary (default for pipes), json, tsv, table\n");
    printf("  -f, --output-file=<FILE> Output destination (default: stdout)\n");
    printf("      --emit-every=<N>     Emit intermediate snapshot every N samples\n");
    printf("      --emit-interval=<S>  Emit intermediate snapshot every S seconds\n");
    printf("  -h, --help               Show this help message\n");
}

static histo_status_t emit_histogram(const histo_t *h, const char *fmt, FILE *out_fp,
                                     uint32_t rebin_factor, bool do_cdf, double norm_area) {
    if (!h || !out_fp) return HISTO_ERR_INVALID_ARG;
    histo_t *emit_h = (histo_t *)h;
    bool needs_free = false;

    if (rebin_factor > 1) {
        histo_t *rebinned = histo_rebin(emit_h, rebin_factor);
        if (rebinned) {
            if (needs_free) histo_destroy(emit_h);
            emit_h = rebinned;
            needs_free = true;
        }
    }

    if (do_cdf) {
        histo_t *cdf_h = histo_cdf(emit_h, 1.0);
        if (cdf_h) {
            if (needs_free) histo_destroy(emit_h);
            emit_h = cdf_h;
            needs_free = true;
        }
    }

    if (norm_area > 0.0) {
        if (!needs_free) {
            emit_h = histo_clone(emit_h, false);
            needs_free = true;
        }
        histo_normalize(emit_h, norm_area);
    }

    histo_status_t status = HISTO_OK;
    if (strcmp(fmt, "binary") == 0 || strcmp(fmt, "bin") == 0) {
        void *buf = NULL;
        size_t size = 0;
        status = histo_serialize_binary(emit_h, &buf, &size);
        if (status == HISTO_OK) {
            fwrite(buf, 1, size, out_fp);
            fflush(out_fp);
            histo_free_buffer(buf);
        }
    } else if (strcmp(fmt, "json") == 0) {
        char *json = NULL;
        status = histo_serialize_json(emit_h, &json);
        if (status == HISTO_OK) {
            fprintf(out_fp, "%s\n", json);
            fflush(out_fp);
            histo_free_buffer(json);
        }
    } else if (strcmp(fmt, "tsv") == 0) {
        uint32_t n = histo_nbins(emit_h);
        for (uint32_t i = 0; i < n; ++i) {
            double lower = 0.0, upper = 0.0, content = 0.0, err = 0.0;
            histo_bin_bounds(emit_h, i, &lower, &upper);
            histo_bin_content(emit_h, i, &content);
            histo_bin_error(emit_h, i, &err);
            fprintf(out_fp, "%u\t%.8g\t%.8g\t%.8g\t%.8g\n", i, lower, upper, content, err);
        }
        fflush(out_fp);
    } else { /* table */
        uint32_t n = histo_nbins(emit_h);
        fprintf(out_fp, "Bin\tLower\tUpper\tContent\tError\n");
        for (uint32_t i = 0; i < n; ++i) {
            double lower = 0.0, upper = 0.0, content = 0.0, err = 0.0;
            histo_bin_bounds(emit_h, i, &lower, &upper);
            histo_bin_content(emit_h, i, &content);
            histo_bin_error(emit_h, i, &err);
            fprintf(out_fp, "[%u]\t%.4f\t%.4f\t%.4f\t%.4f\n", i, lower, upper, content, err);
        }
        fflush(out_fp);
    }

    if (needs_free) {
        histo_destroy(emit_h);
    }
    return status;
}

int cmd_fill_main(int argc, char **argv) {
    uint32_t nbins = 50;
    double range_min = 0.0, range_max = 0.0;
    bool has_min = false, has_max = false;
    bool auto_range = false;
    bool has_weights = false;
    int val_col = 1;
    int w_col = 2;
    char delim = '\0';
    bool binary_input = false;
    bool merge_mode = false;
    uint32_t flags = HISTO_FLAG_TRACK_SUMW2;
    uint32_t rebin_factor = 1;
    bool do_cdf = false;
    double norm_area = 0.0;
    const char *out_format = NULL;
    const char *out_file = NULL;
    uint64_t emit_every = 0;
    double emit_interval = 0.0;
    double *var_edges = NULL;
    uint32_t n_edges = 0;

    /* Parse command line arguments */
    int file_start = argc;
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_fill_usage();
            return 0;
        } else if (strncmp(arg, "-n=", 3) == 0) {
            nbins = (uint32_t)atoi(arg + 3);
        } else if (strcmp(arg, "-n") == 0 && i + 1 < argc) {
            nbins = (uint32_t)atoi(argv[++i]);
        } else if (strncmp(arg, "--bins=", 7) == 0) {
            nbins = (uint32_t)atoi(arg + 7);
        } else if (strncmp(arg, "--min=", 6) == 0) {
            range_min = atof(arg + 6);
            has_min = true;
        } else if (strncmp(arg, "--max=", 6) == 0) {
            range_max = atof(arg + 6);
            has_max = true;
        } else if (strcmp(arg, "--auto-range") == 0) {
            auto_range = true;
        } else if (strcmp(arg, "-w") == 0 || strcmp(arg, "--weights") == 0) {
            has_weights = true;
        } else if (strncmp(arg, "--value-col=", 12) == 0) {
            val_col = atoi(arg + 12);
        } else if (strncmp(arg, "--weights-col=", 14) == 0) {
            w_col = atoi(arg + 14);
            has_weights = true;
        } else if (strncmp(arg, "--delimiter=", 12) == 0) {
            delim = arg[12];
        } else if (strcmp(arg, "--binary-f64") == 0) {
            binary_input = true;
        } else if (strcmp(arg, "--merge") == 0) {
            merge_mode = true;
        } else if (strcmp(arg, "--sumw2") == 0) {
            flags |= HISTO_FLAG_TRACK_SUMW2;
        } else if (strcmp(arg, "--no-sumw2") == 0) {
            flags &= ~HISTO_FLAG_TRACK_SUMW2;
        } else if (strcmp(arg, "--exact-moments") == 0) {
            flags |= HISTO_FLAG_EXACT_MOMENTS;
        } else if (strncmp(arg, "--rebin=", 8) == 0) {
            rebin_factor = (uint32_t)atoi(arg + 8);
        } else if (strcmp(arg, "--cdf") == 0) {
            do_cdf = true;
        } else if (strncmp(arg, "--normalize=", 12) == 0) {
            norm_area = atof(arg + 12);
        } else if (strncmp(arg, "-o=", 3) == 0) {
            out_format = arg + 3;
        } else if (strcmp(arg, "-o") == 0 && i + 1 < argc) {
            out_format = argv[++i];
        } else if (strncmp(arg, "--output=", 9) == 0) {
            out_format = arg + 9;
        } else if (strncmp(arg, "-f=", 3) == 0) {
            out_file = arg + 3;
        } else if (strcmp(arg, "-f") == 0 && i + 1 < argc) {
            out_file = argv[++i];
        } else if (strncmp(arg, "--output-file=", 14) == 0) {
            out_file = arg + 14;
        } else if (strncmp(arg, "--emit-every=", 13) == 0) {
            emit_every = (uint64_t)strtoull(arg + 13, NULL, 10);
        } else if (strncmp(arg, "--emit-interval=", 16) == 0) {
            emit_interval = atof(arg + 16);
        } else if (strncmp(arg, "--edges=", 8) == 0) {
            /* Parse comma-separated edges */
            const char *p = arg + 8;
            size_t edge_cap = 16;
            var_edges = (double *)malloc(edge_cap * sizeof(double));
            n_edges = 0;
            while (*p) {
                if (n_edges >= edge_cap) {
                    edge_cap *= 2;
                    var_edges = (double *)realloc(var_edges, edge_cap * sizeof(double));
                }
                char *endp = NULL;
                var_edges[n_edges++] = strtod(p, &endp);
                if (endp == p) break;
                if (*endp == ',') p = endp + 1;
                else p = endp;
            }
        } else if (arg[0] == '-' && arg[1] != '\0') {
            fprintf(stderr, "Unknown option '%s'. Run 'histo-fill --help' for usage.\n", arg);
            if (var_edges) free(var_edges);
            return 1;
        } else {
            file_start = i;
            break;
        }
    }

    /* Determine output format default */
    if (!out_format) {
        out_format = (cli_is_stdout_tty() && !out_file) ? "json" : "binary";
    }

    FILE *out_fp = stdout;
    if (out_file && strcmp(out_file, "-") != 0) {
        out_fp = fopen(out_file, "wb");
        if (!out_fp) {
            fprintf(stderr, "Error: Cannot open output file '%s'\n", out_file);
            if (var_edges) free(var_edges);
            return 1;
        }
    }

    /* Auto-range buffer if needed */
    double *auto_samples = NULL;
    double *auto_weights = NULL;
    size_t auto_count = 0;
    size_t auto_cap = 0;

    if (!has_min || !has_max) {
        if (!var_edges) {
            auto_range = true;
        }
    }

    histo_t *h = NULL;
    if (!auto_range) {
        if (var_edges && n_edges >= 2) {
            h = histo_create_variable(n_edges - 1, var_edges, flags);
        } else {
            if (range_min >= range_max) {
                range_min = 0.0;
                range_max = 100.0;
            }
            h = histo_create_uniform(nbins, range_min, range_max, flags);
        }
        if (!h) {
            fprintf(stderr, "Error: Failed to initialize histogram.\n");
            if (var_edges) free(var_edges);
            if (out_fp != stdout) fclose(out_fp);
            return 1;
        }
    }

    /* Process input files or stdin */
    int num_files = argc - file_start;
    const char *default_files[] = {"-"};
    const char **files = (num_files > 0) ? (const char **)(argv + file_start) : default_files;
    int nfiles = (num_files > 0) ? num_files : 1;

    uint64_t sample_count = 0;
    double last_emit_time = cli_get_time_sec();

    for (int f = 0; f < nfiles; ++f) {
        FILE *in_fp = NULL;
        if (strcmp(files[f], "-") == 0) {
            in_fp = stdin;
        } else {
            in_fp = fopen(files[f], binary_input || merge_mode ? "rb" : "r");
            if (!in_fp) {
                fprintf(stderr, "Warning: Cannot open input file '%s'\n", files[f]);
                continue;
            }
        }

        if (merge_mode) {
            histo_t *incoming = NULL;
            while (cli_read_histogram_from_stream(in_fp, &incoming) == HISTO_OK) {
                if (!h) {
                    h = incoming;
                } else {
                    histo_add(h, incoming);
                    histo_destroy(incoming);
                }
            }
        } else if (binary_input) {
            double val_buf[2];
            size_t needed = has_weights ? 2 : 1;
            while (fread(val_buf, sizeof(double), needed, in_fp) == needed) {
                double x = val_buf[0];
                double w = has_weights ? val_buf[1] : 1.0;
                if (auto_range) {
                    if (auto_count >= auto_cap) {
                        auto_cap = auto_cap == 0 ? 1024 : auto_cap * 2;
                        auto_samples = (double *)realloc(auto_samples, auto_cap * sizeof(double));
                        if (has_weights) auto_weights = (double *)realloc(auto_weights, auto_cap * sizeof(double));
                    }
                    auto_samples[auto_count] = x;
                    if (has_weights) auto_weights[auto_count] = w;
                    auto_count++;
                } else {
                    histo_fill_w(h, x, w);
                    sample_count++;
                    if ((emit_every > 0 && sample_count % emit_every == 0) ||
                        (emit_interval > 0.0 && (cli_get_time_sec() - last_emit_time) >= emit_interval)) {
                        emit_histogram(h, out_format, out_fp, rebin_factor, do_cdf, norm_area);
                        last_emit_time = cli_get_time_sec();
                    }
                }
            }
        } else {
            /* Text line parsing */
            char line[2048];
            while (fgets(line, sizeof(line), in_fp)) {
                /* Skip blank / comment lines */
                char *p = line;
                while (*p && isspace((unsigned char)*p)) p++;
                if (*p == '\0' || *p == '#') continue;

                double x = 0.0, w = 1.0;
                bool parsed_x = false;

                if (delim != '\0') {
                    /* Custom delimiter splitting */
                    int col = 1;
                    char *tok = strtok(p, (char[]){delim, '\0'});
                    while (tok) {
                        if (col == val_col) {
                            x = atof(tok);
                            parsed_x = true;
                        } else if (has_weights && col == w_col) {
                            w = atof(tok);
                        }
                        tok = strtok(NULL, (char[]){delim, '\0'});
                        col++;
                    }
                } else {
                    /* Whitespace separated */
                    int col = 1;
                    char *tok = strtok(p, " \t\r\n");
                    while (tok) {
                        if (col == val_col) {
                            x = atof(tok);
                            parsed_x = true;
                        } else if (has_weights && col == w_col) {
                            w = atof(tok);
                        }
                        tok = strtok(NULL, " \t\r\n");
                        col++;
                    }
                }

                if (!parsed_x) continue;

                if (auto_range) {
                    if (auto_count >= auto_cap) {
                        auto_cap = auto_cap == 0 ? 1024 : auto_cap * 2;
                        auto_samples = (double *)realloc(auto_samples, auto_cap * sizeof(double));
                        if (has_weights) auto_weights = (double *)realloc(auto_weights, auto_cap * sizeof(double));
                    }
                    auto_samples[auto_count] = x;
                    if (has_weights) auto_weights[auto_count] = w;
                    auto_count++;
                } else {
                    histo_fill_w(h, x, w);
                    sample_count++;
                    if ((emit_every > 0 && sample_count % emit_every == 0) ||
                        (emit_interval > 0.0 && (cli_get_time_sec() - last_emit_time) >= emit_interval)) {
                        emit_histogram(h, out_format, out_fp, rebin_factor, do_cdf, norm_area);
                        last_emit_time = cli_get_time_sec();
                    }
                }
            }
        }

        if (in_fp != stdin) fclose(in_fp);
    }

    /* If auto-range was active, calculate min/max and populate histogram */
    if (auto_range) {
        if (auto_count == 0) {
            range_min = 0.0;
            range_max = 100.0;
        } else {
            range_min = auto_samples[0];
            range_max = auto_samples[0];
            for (size_t i = 1; i < auto_count; ++i) {
                if (auto_samples[i] < range_min) range_min = auto_samples[i];
                if (auto_samples[i] > range_max) range_max = auto_samples[i];
            }
            if (fabs(range_max - range_min) < 1e-12) {
                range_min -= 1.0;
                range_max += 1.0;
            } else {
                /* Add a tiny padding so maximum value falls strictly inside top bin */
                double pad = (range_max - range_min) * 1e-6;
                range_max += pad;
            }
        }

        h = histo_create_uniform(nbins, range_min, range_max, flags);
        if (h) {
            if (has_weights && auto_weights) {
                histo_fill_n(h, auto_count, auto_samples, auto_weights);
            } else {
                histo_fill_n(h, auto_count, auto_samples, NULL);
            }
        }
        if (auto_samples) free(auto_samples);
        if (auto_weights) free(auto_weights);
    }

    if (h) {
        emit_histogram(h, out_format, out_fp, rebin_factor, do_cdf, norm_area);
        histo_destroy(h);
    }

    if (var_edges) free(var_edges);
    if (out_fp != stdout) fclose(out_fp);
    return 0;
}
