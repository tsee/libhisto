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

static void render_top_border(tui_frame_t *f, const char *title, const char *badge, int width) {
    tui_frame_printf(f, "\033[1m┌─ %s ", title);
    int title_cols = 3 + (int)strlen(title) + 1; // "┌─ " (3) + title + " " (1)
    int badge_cols = (badge && badge[0]) ? (tui_visual_width(badge) + 4) : 2; // " " + badge + " ─┐"
    int dashes = width - title_cols - badge_cols;
    if (dashes < 0) dashes = 0;
    for (int i = 0; i < dashes; ++i) tui_frame_puts(f, "─");
    if (badge && badge[0]) {
        tui_frame_printf(f, " %s ─┐\033[0m\r\n", badge);
    } else {
        tui_frame_puts(f, "─┐\033[0m\r\n");
    }
}

static void render_divider(tui_frame_t *f, int width) {
    tui_frame_puts(f, "├");
    for (int i = 0; i < width - 2; ++i) tui_frame_puts(f, "─");
    tui_frame_puts(f, "┤\r\n");
}

static void render_bottom_border(tui_frame_t *f, int width, bool newline) {
    tui_frame_puts(f, "└");
    for (int i = 0; i < width - 2; ++i) tui_frame_puts(f, "─");
    tui_frame_puts(f, "┘");
    if (newline) tui_frame_puts(f, "\r\n");
}

static void render_1d_bars_viewport(tui_frame_t *f, const tui_state_t *st, const histo_t *h, int width, int max_rows) {
    if (!h || max_rows <= 0) {
        for (int r = 0; r < max_rows; ++r) tui_render_row(f, "", width, true);
        return;
    }

    uint32_t nbins = histo_nbins(h);
    if (nbins == 0) {
        for (int r = 0; r < max_rows; ++r) tui_render_row(f, "", width, true);
        return;
    }

    double max_content = 0.0;
    for (uint32_t i = 0; i < nbins; ++i) {
        double c = 0.0;
        histo_bin_content(h, i, &c);
        if (c > max_content) max_content = c;
    }

    double max_scaled = (st->scale_mode == SCALE_LOG_Y || st->scale_mode == SCALE_LOG_LOG) ?
                        log10(max_content + 1.0) : max_content;
    if (max_scaled <= 0.0) max_scaled = 1.0;

    uint32_t step = (nbins > (uint32_t)max_rows) ? (nbins + (uint32_t)max_rows - 1) / (uint32_t)max_rows : 1;
    int rows_drawn = 0;

    for (uint32_t i = 0; i < nbins && rows_drawn < max_rows; i += step) {
        double lower = 0.0, upper = 0.0, content = 0.0;
        histo_bin_bounds(h, i, &lower, &upper);
        histo_bin_content(h, i, &content);

        char row_buf[1024];
        char bounds_str[32];
        if (upper >= 10000.0 || (lower > 0.0 && lower < 0.01)) {
            snprintf(bounds_str, sizeof(bounds_str), "[%6.2g, %6.2g)", lower, upper);
        } else {
            snprintf(bounds_str, sizeof(bounds_str), "[%6.2f, %6.2f)", lower, upper);
        }

        char count_str[32];
        if (content == floor(content) && content >= 0.0 && content < 1e9) {
            snprintf(count_str, sizeof(count_str), "%7.0f", content);
        } else if (fabs(content) < 1e6) {
            snprintf(count_str, sizeof(count_str), "%7.2f", content);
        } else {
            snprintf(count_str, sizeof(count_str), "%7.2g", content);
        }

        // Available width for bar: width - prefix - borders
        // prefix: bounds (16) + " │ " (3) + count (7) + " │ " (3) = 29 columns
        int prefix_cols = 29;
        int bar_max = width - 4 - prefix_cols;
        if (bar_max < 2) bar_max = 2;

        double val_scaled = (st->scale_mode == SCALE_LOG_Y || st->scale_mode == SCALE_LOG_LOG) ?
                            log10(content + 1.0) : content;
        double frac = val_scaled / max_scaled;
        if (frac < 0.0) frac = 0.0;
        if (frac > 1.0) frac = 1.0;

        double bar_len = frac * (double)bar_max;
        int full_chars = (int)bar_len;
        int rem_eighths = (int)((bar_len - (double)full_chars) * 8.0);
        if (rem_eighths > 8) rem_eighths = 8;
        if (rem_eighths < 0) rem_eighths = 0;

        char color_ansi[32] = "";
        tui_term_get_color(frac, st->monochrome, color_ansi, sizeof(color_ansi));

        int pos = snprintf(row_buf, sizeof(row_buf), "%-16s │ %6s │ %s", bounds_str, count_str, color_ansi);

        for (int k = 0; k < full_chars && pos + 4 < (int)sizeof(row_buf); ++k) {
            pos += snprintf(row_buf + pos, sizeof(row_buf) - pos, "█");
        }
        if (rem_eighths > 0 && full_chars < bar_max && pos + 4 < (int)sizeof(row_buf)) {
            pos += snprintf(row_buf + pos, sizeof(row_buf) - pos, "%s", BLOCKS_UTF8[rem_eighths]);
        }
        if (color_ansi[0] && pos + 8 < (int)sizeof(row_buf)) {
            pos += snprintf(row_buf + pos, sizeof(row_buf) - pos, "\033[0m");
        }

        tui_render_row(f, row_buf, width, true);
        rows_drawn++;
    }

    // Fill remaining budget rows so height is strictly invariant
    while (rows_drawn < max_rows) {
        tui_render_row(f, "", width, true);
        rows_drawn++;
    }
}

static void render_help_viewport(tui_frame_t *f, int width, int max_rows) {
    const char *help_lines[] = {
        "── Interactive Help Cheatsheet ───────────────────────────────────────────",
        "",
        "NAVIGATION & VIEWPORT              DISPLAY & SCALING",
        "  + / -       Zoom in / out          l   Cycle Log scales (Y, X, Log-Log)",
        "  h / l, ←/→  Pan viewport left/right  L   Scale Selector Modal",
        "  0           Reset to full range    a   Toggle Dynamic Auto-Range",
        "  r / R       Rebin coarser / finer  C   Toggle TrueColor / Monochrome",
        "  k           Toggle KDE curve       f   Toggle Curve Fit overlay",
        "COMMANDS & PROMPT (:)",
        "  :bins <N>       Set bin count (e.g. :b 80)     [Space] Freeze / Unfreeze display",
        "  :range A B      Set range (e.g. :r 0 100)      c       Clear all accumulators",
        "  :autorange [on|off] Toggle dynamic autorange   q       Quit cleanly",
        "  :help           Open full help modal",
        "",
        "Press [?], [Esc], or [Space] to dismiss this help window."
    };

    size_t n_lines = sizeof(help_lines) / sizeof(help_lines[0]);
    int rows_drawn = 0;

    for (size_t i = 0; i < n_lines && rows_drawn < max_rows; ++i) {
        tui_render_row(f, help_lines[i], width, true);
        rows_drawn++;
    }
    while (rows_drawn < max_rows) {
        tui_render_row(f, "", width, true);
        rows_drawn++;
    }
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
    } else if (strncasecmp(cmd, "scale ", 6) == 0 || strncasecmp(cmd, "sc ", 3) == 0) {
        const char *arg = strchr(cmd, ' ');
        if (arg) {
            while (*arg == ' ') arg++;
            if (strcasecmp(arg, "logy") == 0 || strcasecmp(arg, "y") == 0) {
                st->scale_mode = SCALE_LOG_Y;
                tui_engine_rebuild_1d(eng, 50, 0, 0, NULL);
            } else if (strcasecmp(arg, "logx") == 0 || strcasecmp(arg, "x") == 0) {
                st->scale_mode = SCALE_LOG_X;
                tui_engine_rebuild_1d_log(eng, 50, NULL);
            } else if (strcasecmp(arg, "loglog") == 0 || strcasecmp(arg, "xy") == 0) {
                st->scale_mode = SCALE_LOG_LOG;
                tui_engine_rebuild_1d_log(eng, 50, NULL);
            } else {
                st->scale_mode = SCALE_LINEAR;
                tui_engine_rebuild_1d(eng, 50, 0, 0, NULL);
            }
        }
    } else if (strncasecmp(cmd, "autorange", 9) == 0 || (strncasecmp(cmd, "a", 1) == 0 && (cmd[1] == ' ' || cmd[1] == '\0'))) {
        const char *arg = strchr(cmd, ' ');
        if (arg) {
            while (*arg == ' ') arg++;
            if (strcasecmp(arg, "on") == 0 || strcasecmp(arg, "1") == 0 || strcasecmp(arg, "true") == 0) {
                tui_engine_set_autorange(eng, true, eng->auto_range_threshold);
            } else if (strcasecmp(arg, "off") == 0 || strcasecmp(arg, "0") == 0 || strcasecmp(arg, "false") == 0) {
                tui_engine_set_autorange(eng, false, eng->auto_range_threshold);
            } else {
                double thresh = atof(arg);
                if (thresh > 0.0 && thresh < 1.0) {
                    tui_engine_set_autorange(eng, true, thresh);
                }
            }
        } else {
            tui_engine_set_autorange(eng, !eng->auto_range, eng->auto_range_threshold);
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
            if (ev.type == TUI_KEY_CTRL_C || (ev.type == TUI_KEY_CHAR && ev.ch == 3)) {
                eng.running = false;
                break;
            }
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
                            if (st.scale_mode == SCALE_LOG_X || st.scale_mode == SCALE_LOG_LOG) {
                                tui_engine_rebuild_1d_log(&eng, 50, NULL);
                            } else {
                                tui_engine_rebuild_1d(&eng, 50, 0, 0, NULL);
                            }
                            if (st.paused && st.frozen_snapshot) {
                                histo_destroy(st.frozen_snapshot);
                                if (st.scale_mode == SCALE_LOG_X || st.scale_mode == SCALE_LOG_LOG) {
                                    tui_engine_rebuild_1d_log(&eng, 50, &st.frozen_snapshot);
                                } else {
                                    tui_engine_rebuild_1d(&eng, 50, 0, 0, &st.frozen_snapshot);
                                }
                            }
                            break;
                        case 'k':
                            st.show_kde = !st.show_kde;
                            break;
                        case 'f':
                            st.show_fit = !st.show_fit;
                            break;
                        case 'a':
                        case 'A':
                            tui_engine_set_autorange(&eng, !eng.auto_range, eng.auto_range_threshold);
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

        int term_cols = 80, term_rows = 24;
        tui_term_get_size(&term_cols, &term_rows);
        if (term_cols < 40) term_cols = 40;
        if (term_rows < 10) term_rows = 10;

        int cols = term_cols - 1;
        int rows = term_rows;

        histo_t *snap = st.paused ? st.frozen_snapshot : tui_engine_get_snapshot_1d(&eng);

        tui_frame_clear(&frame);
        tui_frame_puts(&frame, "\033[H"); // Cursor home

        // Row 1: Top border with badge
        char badge[64] = "";
        uint64_t n_entries = snap ? histo_num_entries(snap) : 0;
        if (st.paused) {
            snprintf(badge, sizeof(badge), "[PAUSED | N=%lu]", (unsigned long)n_entries);
        } else if (tui_engine_is_finished(&eng)) {
            snprintf(badge, sizeof(badge), "[EOF | N=%lu]", (unsigned long)n_entries);
        } else {
            double rate = eng.current_rate_ops;
            if (rate >= 1e6) snprintf(badge, sizeof(badge), "[LIVE: %.1fM/s | N=%lu]", rate / 1e6, (unsigned long)n_entries);
            else if (rate >= 1e3) snprintf(badge, sizeof(badge), "[LIVE: %.1fk/s | N=%lu]", rate / 1e3, (unsigned long)n_entries);
            else snprintf(badge, sizeof(badge), "[LIVE: %.0f/s | N=%lu]", rate, (unsigned long)n_entries);
        }
        render_top_border(&frame, "libhisto top", badge, cols);

        // Row 2: Stats summary
        char stats_buf[256];
        if (snap && n_entries > 0) {
            double mean = 0, sdev = 0, med = 0, iqr = 0, p95 = 0, p99 = 0;
            histo_mean(snap, &mean);
            histo_std_dev(snap, &sdev);
            histo_median(snap, &med);
            histo_iqr(snap, &iqr);
            histo_quantile(snap, 0.95, &p95);
            histo_quantile(snap, 0.99, &p99);
            snprintf(stats_buf, sizeof(stats_buf), "Mean: %.2f ± %.2f │ Med: %.2f │ IQR: %.2f │ P95: %.2f │ P99: %.2f",
                     mean, sdev, med, iqr, p95, p99);
        } else {
            snprintf(stats_buf, sizeof(stats_buf), "Waiting for streaming samples...");
        }
        tui_render_row(&frame, stats_buf, cols, true);

        // Row 3: Divider
        render_divider(&frame, cols);

        // Row 4: Subheader
        char subhdr[256];
        if (snap) {
            double r_min = 0, r_max = 0;
            histo_range(snap, &r_min, &r_max);
            uint32_t nb = histo_nbins(snap);
            const char *sc = (st.scale_mode == SCALE_LOG_Y) ? "LOG-Y" :
                             (st.scale_mode == SCALE_LOG_X) ? "LOG-X" :
                             (st.scale_mode == SCALE_LOG_LOG) ? "LOG-LOG" : "LIN";
            snprintf(subhdr, sizeof(subhdr), "Range: [%.2f, %.2f] │ Bins: %u (Δ=%.2f) │ Scale: %s │ Auto: %s %s%s",
                     r_min, r_max, nb, (r_max - r_min) / (nb ? (double)nb : 1.0), sc,
                     eng.auto_range ? "ON" : "OFF",
                     st.show_kde ? "│ KDE " : "", st.show_fit ? "│ FIT " : "");
        } else {
            snprintf(subhdr, sizeof(subhdr), "Initializing...");
        }
        tui_render_row(&frame, subhdr, cols, true);

        // Viewport rows budget: rows - 7 lines (Top, Stats, Div, Subhdr, Div, Footer, Bottom)
        int viewport_rows = rows - 7;
        if (viewport_rows < 3) viewport_rows = 3;

        if (st.modal == MODAL_HELP) {
            render_help_viewport(&frame, cols, viewport_rows);
        } else {
            render_1d_bars_viewport(&frame, &st, snap, cols, viewport_rows);
        }

        // Row rows - 2: Divider
        render_divider(&frame, cols);

        // Row rows - 1: Footer / Command
        if (st.cmd_active) {
            char cmd_line[300];
            snprintf(cmd_line, sizeof(cmd_line), ":%s\033[7m \033[0m", st.cmd_buf);
            tui_render_row(&frame, cmd_line, cols, true);
        } else {
            const char *hints = "[Space] Freeze  [a] Auto  [l] Log  [k] KDE  [f] Fit  [r/R] Rebin  [:] Cmd  [?] Help  [q]";
            tui_render_row(&frame, hints, cols, true);
        }

        // Row rows: Bottom border (no trailing newline to avoid scrolling on final cell)
        render_bottom_border(&frame, cols, false);
        tui_frame_puts(&frame, "\033[J"); // Erase below in case window grew or shrank

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
