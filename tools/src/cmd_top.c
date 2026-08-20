#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 700

#include "cli_common.h"
#include "tui_term.h"
#include "tui_engine.h"
#include "histo/fit.h"
#include "histo/kde.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>

typedef enum {
    VIEW_1D_BARS = 0,
    VIEW_SPARKLINE,
    VIEW_CDF,
    VIEW_2D_HEATMAP
} tui_view_mode_t;

typedef enum {
    SCALE_LINEAR = 0,
    SCALE_LOG_Y,
    SCALE_LOG_X,
    SCALE_LOG_LOG
} tui_scale_mode_t;

typedef enum {
    MODAL_NONE = 0,
    MODAL_HELP,
    MODAL_SCALE_PICKER
} tui_modal_type_t;

typedef struct {
    tui_view_mode_t view_mode;
    tui_scale_mode_t scale_mode;
    tui_modal_type_t modal;
    bool show_kde;
    bool show_fit;
    bool show_errors;
    bool monochrome;
    bool paused;
    bool cmd_active;

    char cmd_buf[256];
    size_t cmd_len;

    char status_msg[128];
    double status_msg_time;

    histo_t *frozen_snapshot;
} tui_state_t;

static const char *const BLOCKS_UTF8[9] = { " ", "▏", "▎", "▍", "▌", "▋", "▊", "▉", "█" };

static void render_header(tui_frame_t *f, const tui_state_t *st, const tui_engine_t *eng, const histo_t *h, int width) {
    uint64_t n = h ? histo_num_entries(h) : 0;
    double w = h ? histo_total_weight(h) : 0.0;
    double rate = eng ? eng->current_rate_ops : 0.0;

    char badge[64];
    if (st->paused) {
        snprintf(badge, sizeof(badge), "[PAUSED (VIEW FROZEN) | N=%lu]", (unsigned long)n);
    } else if (eng && eng->finished_reading) {
        snprintf(badge, sizeof(badge), "[STREAM FINISHED | N=%lu]", (unsigned long)n);
    } else {
        if (rate >= 1e6) {
            snprintf(badge, sizeof(badge), "[LIVE: %.1f M-ops/s | N=%lu]", rate / 1e6, (unsigned long)n);
        } else if (rate >= 1e3) {
            snprintf(badge, sizeof(badge), "[LIVE: %.1f k-ops/s | N=%lu]", rate / 1e3, (unsigned long)n);
        } else {
            snprintf(badge, sizeof(badge), "[LIVE: %.0f ops/s | N=%lu]", rate, (unsigned long)n);
        }
    }

    tui_frame_printf(f, "\033[1m┌─ libhisto top ");
    int pad1 = width - 18 - (int)strlen(badge);
    for (int i = 0; i < pad1; ++i) tui_frame_puts(f, "─");
    tui_frame_printf(f, " %s ─┐\033[0m\n", badge);

    if (h && n > 0) {
        double mean = 0.0, sdev = 0.0, median = 0.0, iqr = 0.0, p95 = 0.0, p99 = 0.0;
        histo_mean(h, &mean);
        histo_std_dev(h, &sdev);
        histo_median(h, &median);
        histo_iqr(h, &iqr);
        histo_quantile(h, 0.95, &p95);
        histo_quantile(h, 0.99, &p99);

        char stats_line[512];
        snprintf(stats_line, sizeof(stats_line),
                 "│ Mean: %.2f ± %.2f │ Med: %.2f │ IQR: %.2f │ P95: %.2f │ P99: %.2f │ W: %.1f",
                 mean, sdev, median, iqr, p95, p99, w);
        int pad2 = width - (int)strlen(stats_line) - 1;
        tui_frame_puts(f, stats_line);
        for (int i = 0; i < pad2; ++i) tui_frame_puts(f, " ");
        tui_frame_puts(f, "│\n");
    } else {
        char empty_line[] = "│ Waiting for streaming samples...";
        tui_frame_puts(f, empty_line);
        int pad2 = width - (int)strlen(empty_line) - 1;
        for (int i = 0; i < pad2; ++i) tui_frame_puts(f, " ");
        tui_frame_puts(f, "│\n");
    }

    tui_frame_puts(f, "├");
    for (int i = 0; i < width - 2; ++i) tui_frame_puts(f, "─");
    tui_frame_puts(f, "┤\n");
}

static void render_1d_bars(tui_frame_t *f, const tui_state_t *st, const histo_t *h, int width, int height) {
    if (!h) return;
    uint32_t nbins = histo_nbins(h);
    if (nbins == 0) return;

    double rmin = 0.0, rmax = 0.0;
    histo_range(h, &rmin, &rmax);

    const char *scale_str = (st->scale_mode == SCALE_LOG_Y) ? "LOG Y" :
                            (st->scale_mode == SCALE_LOG_X) ? "LOG X" :
                            (st->scale_mode == SCALE_LOG_LOG) ? "LOG-LOG" : "LINEAR";

    char subhdr[256];
    snprintf(subhdr, sizeof(subhdr), "│ Range: [%.2f, %.2f] │ Bins: %u (Δx=%.2f) │ Scale: %s %s",
             rmin, rmax, nbins, (rmax - rmin) / (double)nbins, scale_str,
             st->show_kde ? "│ KDE: ON " : st->show_fit ? "│ FIT: ON " : "");
    tui_frame_puts(f, subhdr);
    int pad = width - (int)strlen(subhdr) - 1;
    for (int i = 0; i < pad; ++i) tui_frame_puts(f, " ");
    tui_frame_puts(f, "│\n");

    /* Find max bin content */
    double max_content = 0.0;
    for (uint32_t i = 0; i < nbins; ++i) {
        double c = 0.0;
        histo_bin_content(h, i, &c);
        if (c > max_content) max_content = c;
    }

    double max_scaled = (st->scale_mode == SCALE_LOG_Y || st->scale_mode == SCALE_LOG_LOG) ?
                        log10(max_content + 1.0) : max_content;
    if (max_scaled <= 0.0) max_scaled = 1.0;

    int max_rows = height - 7;
    if (max_rows < 3) max_rows = 3;
    uint32_t step = (nbins > (uint32_t)max_rows) ? (nbins + (uint32_t)max_rows - 1) / (uint32_t)max_rows : 1;

    for (uint32_t i = 0; i < nbins; i += step) {
        double lower = 0.0, upper = 0.0, content = 0.0, err = 0.0;
        histo_bin_bounds(h, i, &lower, &upper);
        histo_bin_content(h, i, &content);
        if (st->show_errors) histo_bin_error(h, i, &err);

        char bounds_str[32];
        snprintf(bounds_str, sizeof(bounds_str), "[%6.2f, %6.2f)", lower, upper);

        char count_str[32];
        if (content == floor(content) && content >= 0.0 && content < 1e12) {
            snprintf(count_str, sizeof(count_str), "%6.0f", content);
        } else {
            snprintf(count_str, sizeof(count_str), "%6.2g", content);
        }

        tui_frame_printf(f, "│ %-16s │ %s │ ", bounds_str, count_str);

        int prefix_len = 16 + 2 + 6 + 3 + 2; /* bounds + count + separators */
        int bar_max_chars = width - prefix_len - 3;
        if (bar_max_chars < 5) bar_max_chars = 5;

        double val_scaled = (st->scale_mode == SCALE_LOG_Y || st->scale_mode == SCALE_LOG_LOG) ?
                            log10(content + 1.0) : content;
        double frac = val_scaled / max_scaled;
        if (frac < 0.0) frac = 0.0;
        if (frac > 1.0) frac = 1.0;

        double bar_len = frac * (double)bar_max_chars;
        int full_chars = (int)bar_len;
        int rem_eighths = (int)((bar_len - (double)full_chars) * 8.0);
        if (rem_eighths > 8) rem_eighths = 8;
        if (rem_eighths < 0) rem_eighths = 0;

        char color_ansi[32] = "";
        tui_term_get_color(frac, st->monochrome, color_ansi, sizeof(color_ansi));
        if (color_ansi[0]) tui_frame_puts(f, color_ansi);

        for (int k = 0; k < full_chars; ++k) tui_frame_puts(f, "█");
        if (rem_eighths > 0 && full_chars < bar_max_chars) {
            tui_frame_puts(f, BLOCKS_UTF8[rem_eighths]);
            full_chars++;
        }
        if (color_ansi[0]) tui_frame_puts(f, "\033[0m");

        int line_pad = bar_max_chars - full_chars;
        for (int k = 0; k < line_pad; ++k) tui_frame_puts(f, " ");
        tui_frame_puts(f, "│\n");
    }
}

static void render_help_modal(tui_frame_t *f, int width, int height) {
    (void)height;
    const char *help_lines[] = {
        "┌─ libhisto top ─ Interactive Help Cheatsheet ──────────────────────────────────────────────┐",
        "│                                                                                           │",
        "│ NAVIGATION & VIEWPORT              DISPLAY & SCALING              ANALYSIS & CURVES       │",
        "│   + / -       Zoom range in / out    l     Cycle Log scale (X/Y)    k   Toggle KDE curve  │",
        "│   h / l, ←/→  Pan view left / right  L     Scale Selector Modal     f   Toggle Curve Fit  │",
        "│   0           Reset to full range    e     Toggle Error bars (±σ)   C   Toggle Color/Mono │",
        "│   r / R       Rebin coarser / finer  S     Toggle Sparkline mode    Tab Cycle 1D/2D/CDF   │",
        "│                                                                                           │",
        "│ COMMANDS & PROMPT (:)              STREAM CONTROL                                         │",
        "│   :bins <N>   Set exact bin count    Space Pause / Freeze display (Ingestion continues)   │",
        "│   :range A B  Set bounds [A, B]      c     Clear / Reset all counters                     │",
        "│   :fit <m>    Fit (gauss, exp, poly) s     Export snapshot to JSON                        │",
        "│   :kde <k>    KDE (gauss, epanech)   q     Quit and restore terminal                      │",
        "│                                                                                           │",
        "│ Press [?], [Esc], or [Space] to dismiss this help window.                                 │",
        "└───────────────────────────────────────────────────────────────────────────────────────────┘"
    };

    size_t n_lines = sizeof(help_lines) / sizeof(help_lines[0]);
    int modal_width = (int)strlen(help_lines[0]);
    int left_margin = (width > modal_width) ? (width - modal_width) / 2 : 0;

    for (size_t i = 0; i < n_lines; ++i) {
        for (int m = 0; m < left_margin; ++m) tui_frame_puts(f, " ");
        tui_frame_puts(f, "\033[1;37;44m");
        tui_frame_puts(f, help_lines[i]);
        tui_frame_puts(f, "\033[0m\n");
    }
}

static void render_footer(tui_frame_t *f, const tui_state_t *st, int width) {
    tui_frame_puts(f, "├");
    for (int i = 0; i < width - 2; ++i) tui_frame_puts(f, "─");
    tui_frame_puts(f, "┤\n");

    if (st->cmd_active) {
        tui_frame_printf(f, "│ :%s\033[7m \033[0m", st->cmd_buf);
        int pad = width - 4 - (int)strlen(st->cmd_buf);
        for (int i = 0; i < pad; ++i) tui_frame_puts(f, " ");
        tui_frame_puts(f, "│\n");
    } else {
        const char *hints = "│ [Space] Freeze  [l] Log  [k] KDE  [f] Fit  [r/R] Rebin  [+/-] Zoom  [:] Cmd  [?] Help  [q]";
        tui_frame_puts(f, hints);
        int pad = width - (int)strlen(hints) - 1;
        for (int i = 0; i < pad; ++i) tui_frame_puts(f, " ");
        tui_frame_puts(f, "│\n");
    }

    tui_frame_puts(f, "└");
    for (int i = 0; i < width - 2; ++i) tui_frame_puts(f, "─");
    tui_frame_puts(f, "┘\n");
}

static void handle_command(tui_state_t *st, tui_engine_t *eng, const char *cmd) {
    while (*cmd && isspace((unsigned char)*cmd)) cmd++;
    if (*cmd == '\0') return;

    if (strncasecmp(cmd, "bins ", 5) == 0 || strncasecmp(cmd, "b ", 2) == 0) {
        const char *arg = strchr(cmd, ' ');
        if (arg) {
            uint32_t n = (uint32_t)atoi(arg + 1);
            if (n > 0 && n <= 100000) {
                tui_engine_rebuild_1d(eng, n, 0, 0, NULL);
            }
        }
    } else if (strncasecmp(cmd, "range ", 6) == 0 || strncasecmp(cmd, "r ", 2) == 0) {
        double rmin = 0, rmax = 0;
        if (sscanf(cmd + (*cmd == 'r' ? 2 : 6), "%lf %lf", &rmin, &rmax) == 2 && rmin < rmax) {
            tui_engine_rebuild_1d(eng, 50, rmin, rmax, NULL);
        }
    } else if (strcasecmp(cmd, "clear") == 0 || strcasecmp(cmd, "reset") == 0) {
        tui_engine_clear(eng);
    } else if (strcasecmp(cmd, "help") == 0 || strcasecmp(cmd, "h") == 0) {
        st->modal = MODAL_HELP;
    } else if (strcasecmp(cmd, "quit") == 0 || strcasecmp(cmd, "q") == 0) {
        eng->running = false;
    }
}

int cmd_top_main(int argc, char **argv) {
    bool is_2d = false;
    uint32_t nbins = 50;
    double rmin = 0.0, rmax = 100.0;
    bool monochrome = false;
    uint32_t flags = HISTO_FLAG_TRACK_SUMW2 | HISTO_FLAG_EXACT_MOMENTS;
    const char *file_arg = NULL;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            printf("Usage: histo top [OPTIONS] [FILE]\n\n"
                   "Real-time interactive terminal monitor for streaming distributions.\n\n"
                   "Options:\n"
                   "  -n, --bins=<N>     Initial number of bins (default: 50)\n"
                   "      --min=<X>      Initial lower boundary\n"
                   "      --max=<X>      Initial upper boundary\n"
                   "      --2d           Enable 2D bivariate mode\n"
                   "  -M, --mono         Monochrome mode (disable ANSI colors)\n"
                   "  -h, --help         Show this help message\n");
            return 0;
        } else if (strncmp(arg, "--bins=", 7) == 0) {
            nbins = (uint32_t)atoi(arg + 7);
        } else if (strcmp(arg, "--2d") == 0) {
            is_2d = true;
        } else if (strcmp(arg, "-M") == 0 || strcmp(arg, "--mono") == 0) {
            monochrome = true;
        } else if (arg[0] != '-') {
            file_arg = arg;
        }
    }

    FILE *in_fp = stdin;
    if (file_arg && strcmp(file_arg, "-") != 0) {
        in_fp = fopen(file_arg, "r");
        if (!in_fp) {
            fprintf(stderr, "Error: Cannot open input file '%s'\n", file_arg);
            return 1;
        }
    }

    tui_engine_t eng;
    if (!tui_engine_init(&eng, in_fp, is_2d, nbins, rmin, rmax, flags)) {
        fprintf(stderr, "Error: Failed to initialize TUI engine.\n");
        if (in_fp != stdin) fclose(in_fp);
        return 1;
    }

    if (!tui_term_init() || !tui_term_raw_enter()) {
        fprintf(stderr, "Error: Failed to initialize terminal raw mode.\n");
        tui_engine_free(&eng);
        if (in_fp != stdin) fclose(in_fp);
        return 1;
    }

    tui_state_t st;
    memset(&st, 0, sizeof(st));
    st.monochrome = monochrome;
    st.view_mode = is_2d ? VIEW_2D_HEATMAP : VIEW_1D_BARS;

    tui_engine_start(&eng);

    int tty_fd = tui_term_get_tty_fd();

    tui_frame_t frame;
    tui_frame_init(&frame, 65536);

    while (eng.running) {
        tui_key_event_t ev = tui_term_read_key(tty_fd, 33);
        if (ev.type != TUI_KEY_NONE) {
            if (st.cmd_active) {
                if (ev.type == TUI_KEY_ESC) {
                    st.cmd_active = false;
                    st.cmd_len = 0;
                    st.cmd_buf[0] = '\0';
                } else if (ev.type == TUI_KEY_ENTER) {
                    st.cmd_active = false;
                    handle_command(&st, &eng, st.cmd_buf);
                    st.cmd_len = 0;
                    st.cmd_buf[0] = '\0';
                } else if (ev.type == TUI_KEY_BACKSPACE) {
                    if (st.cmd_len > 0) {
                        st.cmd_buf[--st.cmd_len] = '\0';
                    }
                } else if (ev.type == TUI_KEY_CHAR && ev.ch >= 32 && ev.ch < 127) {
                    if (st.cmd_len + 1 < sizeof(st.cmd_buf)) {
                        st.cmd_buf[st.cmd_len++] = (char)ev.ch;
                        st.cmd_buf[st.cmd_len] = '\0';
                    }
                }
            } else if (st.modal != MODAL_NONE) {
                if (ev.type == TUI_KEY_ESC || (ev.type == TUI_KEY_CHAR && (ev.ch == '?' || ev.ch == ' ' || ev.ch == 'q'))) {
                    st.modal = MODAL_NONE;
                }
            } else {
                if (ev.type == TUI_KEY_CHAR) {
                    switch (ev.ch) {
                        case 'q':
                        case 'Q':
                            eng.running = false;
                            break;
                        case ' ':
                            st.paused = !st.paused;
                            if (st.paused) {
                                if (st.frozen_snapshot) histo_destroy(st.frozen_snapshot);
                                st.frozen_snapshot = tui_engine_get_snapshot_1d(&eng);
                            } else {
                                if (st.frozen_snapshot) {
                                    histo_destroy(st.frozen_snapshot);
                                    st.frozen_snapshot = NULL;
                                }
                            }
                            break;
                        case ':':
                            st.cmd_active = true;
                            st.cmd_len = 0;
                            st.cmd_buf[0] = '\0';
                            break;
                        case '?':
                            st.modal = MODAL_HELP;
                            break;
                        case 'l':
                            st.scale_mode = (st.scale_mode == SCALE_LINEAR) ? SCALE_LOG_Y :
                                            (st.scale_mode == SCALE_LOG_Y) ? SCALE_LOG_X :
                                            (st.scale_mode == SCALE_LOG_X) ? SCALE_LOG_LOG : SCALE_LINEAR;
                            break;
                        case 'k':
                            st.show_kde = !st.show_kde;
                            break;
                        case 'f':
                            st.show_fit = !st.show_fit;
                            break;
                        case 'e':
                            st.show_errors = !st.show_errors;
                            break;
                        case 'C':
                            st.monochrome = !st.monochrome;
                            break;
                        case 'c':
                            tui_engine_clear(&eng);
                            if (st.frozen_snapshot) {
                                histo_destroy(st.frozen_snapshot);
                                st.frozen_snapshot = NULL;
                            }
                            break;
                        case 'r':
                            tui_engine_rebuild_1d(&eng, (nbins > 10) ? nbins / 2 : 5, 0, 0, NULL);
                            break;
                        case 'R':
                            tui_engine_rebuild_1d(&eng, nbins * 2, 0, 0, NULL);
                            break;
                        default:
                            break;
                    }
                }
            }
        }

        int cols = 80, rows = 24;
        tui_term_get_size(&cols, &rows);

        histo_t *snap = st.paused ? st.frozen_snapshot : tui_engine_get_snapshot_1d(&eng);

        tui_frame_clear(&frame);
        tui_frame_puts(&frame, "\033[H"); // Cursor home
        render_header(&frame, &st, &eng, snap, cols);
        render_1d_bars(&frame, &st, snap, cols, rows);

        if (st.modal == MODAL_HELP) {
            render_help_modal(&frame, cols, rows);
        }

        render_footer(&frame, &st, cols);
        tui_frame_flush(&frame, stdout);

        if (!st.paused && snap) {
            histo_destroy(snap);
        }
    }

    if (st.frozen_snapshot) {
        histo_destroy(st.frozen_snapshot);
    }

    tui_frame_free(&frame);
    tui_term_restore();
    tui_engine_free(&eng);
    if (in_fp != stdin) fclose(in_fp);
    return 0;
}
