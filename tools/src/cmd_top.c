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
    bool log_z;  /* 2D: logarithmic intensity scaling */

    char cmd_buf[256];
    size_t cmd_len;

    char status_msg[128];
    double status_msg_time;

    histo_t *frozen_snapshot;
    histo2d_t *frozen_snapshot_2d;
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

static void get_viridis_bg(double fraction, bool monochrome, char *out_ansi, size_t max_len) {
    if (monochrome) {
        static const char density[] = " .:-=+*#%@";
        int idx = (int)(fraction * 9.0);
        if (idx < 0) idx = 0;
        if (idx > 9) idx = 9;
        snprintf(out_ansi, max_len, "%c", density[idx]);
        return;
    }
    if (fraction < 0.0) fraction = 0.0;
    if (fraction > 1.0) fraction = 1.0;
    int r = 0, g = 0, b = 0;
    if (fraction < 0.25) {
        double t = fraction / 0.25;
        r = (int)((1.0 - t) * 68 + t * 59);
        g = (int)((1.0 - t) * 1 + t * 82);
        b = (int)((1.0 - t) * 84 + t * 139);
    } else if (fraction < 0.5) {
        double t = (fraction - 0.25) / 0.25;
        r = (int)((1.0 - t) * 59 + t * 33);
        g = (int)((1.0 - t) * 82 + t * 145);
        b = (int)((1.0 - t) * 139 + t * 140);
    } else if (fraction < 0.75) {
        double t = (fraction - 0.5) / 0.25;
        r = (int)((1.0 - t) * 33 + t * 94);
        g = (int)((1.0 - t) * 145 + t * 201);
        b = (int)((1.0 - t) * 140 + t * 98);
    } else {
        double t = (fraction - 0.75) / 0.25;
        r = (int)((1.0 - t) * 94 + t * 253);
        g = (int)((1.0 - t) * 201 + t * 231);
        b = (int)((1.0 - t) * 98 + t * 37);
    }
    snprintf(out_ansi, max_len, "\033[48;2;%d;%d;%dm", r, g, b);
}

static void render_2d_heatmap_viewport(tui_frame_t *f, const tui_state_t *st, const histo2d_t *h, int width, int max_rows) {
    if (!h || max_rows <= 0) {
        for (int r = 0; r < max_rows; ++r) tui_render_row(f, "", width, true);
        return;
    }

    uint32_t nx = histo2d_nbins_x(h);
    uint32_t ny = histo2d_nbins_y(h);
    if (nx == 0 || ny == 0) {
        for (int r = 0; r < max_rows; ++r) tui_render_row(f, "", width, true);
        return;
    }

    histo2d_axis_t ax, ay;
    histo2d_axis_x(h, &ax);
    histo2d_axis_y(h, &ay);

    double max_content = 0.0;
    for (uint32_t ix = 0; ix < nx; ++ix) {
        for (uint32_t iy = 0; iy < ny; ++iy) {
            double c = 0.0;
            histo2d_bin_content(h, ix, iy, &c);
            if (c > max_content) max_content = c;
        }
    }
    double max_scaled = (st->log_z && max_content > 0.0) ? log10(max_content + 1.0) : max_content;
    if (max_scaled <= 0.0) max_scaled = 1.0;

    /* Y-axis label width: "  yval │ " = ~10 chars */
    int label_cols = 10;
    /* Available width for heatmap cells: 2 chars per X bin */
    int avail_cols = width - label_cols - 4;
    uint32_t x_step = 1;
    if ((int)(nx * 2) > avail_cols && avail_cols > 0) {
        x_step = (nx * 2 + (uint32_t)avail_cols - 1) / (uint32_t)avail_cols;
    }

    /* Y rows: map ny bins to max_rows, stepping if needed */
    /* Reserve 2 rows for X-axis line and labels */
    int heatmap_rows = max_rows - 2;
    if (heatmap_rows < 1) heatmap_rows = 1;
    uint32_t y_step = (ny > (uint32_t)heatmap_rows) ? (ny + (uint32_t)heatmap_rows - 1) / (uint32_t)heatmap_rows : 1;

    int rows_drawn = 0;

    /* Render Y rows from top (ny-1) to bottom (0) */
    for (int iy = (int)ny - 1; iy >= 0 && rows_drawn < heatmap_rows; iy -= (int)y_step) {
        char row_buf[4096];
        double ymin_b, ymax_b, dummy;
        histo2d_bin_bounds(h, 0, (uint32_t)iy, &dummy, &dummy, &ymin_b, &ymax_b);
        double y_center = 0.5 * (ymin_b + ymax_b);

        int pos = snprintf(row_buf, sizeof(row_buf), "%7.1f │ ", y_center);

        for (uint32_t ix = 0; ix < nx && pos + 32 < (int)sizeof(row_buf); ix += x_step) {
            double c = 0.0;
            histo2d_bin_content(h, ix, (uint32_t)iy, &c);
            double val_scaled = (st->log_z && c > 0.0) ? log10(c + 1.0) : c;
            double frac = (val_scaled > 0.0) ? (val_scaled / max_scaled) : 0.0;

            if (st->monochrome) {
                static const char density[] = " .:-=+*#%@";
                int idx = (int)(frac * 9.0);
                if (idx < 0) idx = 0;
                if (idx > 9) idx = 9;
                pos += snprintf(row_buf + pos, sizeof(row_buf) - pos, "%c ", density[idx]);
            } else {
                char bg[32];
                get_viridis_bg(frac, false, bg, sizeof(bg));
                pos += snprintf(row_buf + pos, sizeof(row_buf) - pos, "%s  \033[0m", bg);
            }
        }

        tui_render_row(f, row_buf, width, true);
        rows_drawn++;
    }

    /* X-axis border row */
    if (rows_drawn < max_rows) {
        char axis_buf[4096];
        int pos = snprintf(axis_buf, sizeof(axis_buf), "        └");
        uint32_t x_cells = (nx + x_step - 1) / x_step;
        for (uint32_t ix = 0; ix < x_cells && pos + 8 < (int)sizeof(axis_buf); ++ix) {
            pos += snprintf(axis_buf + pos, sizeof(axis_buf) - pos, "──");
        }
        pos += snprintf(axis_buf + pos, sizeof(axis_buf) - pos, "─┘");
        tui_render_row(f, axis_buf, width, true);
        rows_drawn++;
    }

    /* X-axis labels row */
    if (rows_drawn < max_rows) {
        char label_buf[256];
        snprintf(label_buf, sizeof(label_buf), "         %-7.1f                              %7.1f", ax.min, ax.max);
        tui_render_row(f, label_buf, width, true);
        rows_drawn++;
    }

    while (rows_drawn < max_rows) {
        tui_render_row(f, "", width, true);
        rows_drawn++;
    }
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

    double total_w = histo_total_weight(h);
    uint64_t num_entries = histo_num_entries(h);

    histo_kde_t *kde = NULL;
    if (st->show_kde && num_entries >= 5) {
        kde = histo_kde_create_from_histo(h, NULL);
    }

    histo_fit_result_t *fit_res = NULL;
    if (st->show_fit && num_entries >= 5) {
        histo_fit_model(h, HISTO_FIT_MODEL_GAUSSIAN, NULL, NULL, &fit_res);
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

        char row_buf[4096];
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

        double x_center = 0.5 * (lower + upper);
        double bin_w = upper - lower;

        int kde_col = -1;
        if (kde && total_w > 0.0) {
            double pdf = histo_kde_eval(kde, x_center);
            double kde_c = pdf * total_w * bin_w;
            double kde_scaled = (st->scale_mode == SCALE_LOG_Y || st->scale_mode == SCALE_LOG_LOG) ?
                                log10(kde_c + 1.0) : kde_c;
            double kde_frac = kde_scaled / max_scaled;
            if (kde_frac > 1.0) kde_frac = 1.0;
            if (kde_frac > 0.005) {
                kde_col = (int)(kde_frac * (double)bar_max + 0.5);
                if (kde_col >= bar_max) kde_col = bar_max - 1;
            }
        }

        int fit_col = -1;
        if (fit_res && fit_res->converged) {
            double fit_c = histo_fit_eval(HISTO_FIT_MODEL_GAUSSIAN, fit_res->params, fit_res->num_params, x_center);
            if (fit_c > 0.0) {
                double fit_scaled = (st->scale_mode == SCALE_LOG_Y || st->scale_mode == SCALE_LOG_LOG) ?
                                    log10(fit_c + 1.0) : fit_c;
                double fit_frac = fit_scaled / max_scaled;
                if (fit_frac > 1.0) fit_frac = 1.0;
                if (fit_frac > 0.005) {
                    fit_col = (int)(fit_frac * (double)bar_max + 0.5);
                    if (fit_col >= bar_max) fit_col = bar_max - 1;
                }
            }
        }

        char color_ansi[32] = "";
        tui_term_get_color(frac, st->monochrome, color_ansi, sizeof(color_ansi));

        int pos = snprintf(row_buf, sizeof(row_buf), "%-16s │ %6s │ ", bounds_str, count_str);

        int max_col = full_chars + (rem_eighths > 0 ? 1 : 0);
        if (kde_col >= max_col) max_col = kde_col + 1;
        if (fit_col >= max_col) max_col = fit_col + 1;
        if (max_col > bar_max) max_col = bar_max;

        bool in_bar_color = false;
        for (int k = 0; k < max_col && pos + 32 < (int)sizeof(row_buf); ++k) {
            if (k == kde_col && k == fit_col) {
                if (in_bar_color) { pos += snprintf(row_buf + pos, sizeof(row_buf) - pos, "\033[0m"); in_bar_color = false; }
                pos += snprintf(row_buf + pos, sizeof(row_buf) - pos, "\033[1;33m✦\033[0m");
            } else if (k == kde_col) {
                if (in_bar_color) { pos += snprintf(row_buf + pos, sizeof(row_buf) - pos, "\033[0m"); in_bar_color = false; }
                pos += snprintf(row_buf + pos, sizeof(row_buf) - pos, "\033[1;36m◆\033[0m");
            } else if (k == fit_col) {
                if (in_bar_color) { pos += snprintf(row_buf + pos, sizeof(row_buf) - pos, "\033[0m"); in_bar_color = false; }
                pos += snprintf(row_buf + pos, sizeof(row_buf) - pos, "\033[1;35m✖\033[0m");
            } else if (k < full_chars) {
                if (!in_bar_color) {
                    if (color_ansi[0]) pos += snprintf(row_buf + pos, sizeof(row_buf) - pos, "%s", color_ansi);
                    in_bar_color = true;
                }
                pos += snprintf(row_buf + pos, sizeof(row_buf) - pos, "█");
            } else if (k == full_chars && rem_eighths > 0) {
                if (!in_bar_color) {
                    if (color_ansi[0]) pos += snprintf(row_buf + pos, sizeof(row_buf) - pos, "%s", color_ansi);
                    in_bar_color = true;
                }
                pos += snprintf(row_buf + pos, sizeof(row_buf) - pos, "%s", BLOCKS_UTF8[rem_eighths]);
            } else {
                if (in_bar_color) { pos += snprintf(row_buf + pos, sizeof(row_buf) - pos, "\033[0m"); in_bar_color = false; }
                pos += snprintf(row_buf + pos, sizeof(row_buf) - pos, " ");
            }
        }
        if (in_bar_color) {
            pos += snprintf(row_buf + pos, sizeof(row_buf) - pos, "\033[0m");
        }

        if (st->show_errors) {
            double err = 0.0;
            histo_bin_error(h, i, &err);
            if (err > 0.0) {
                if (st->monochrome) {
                    pos += snprintf(row_buf + pos, sizeof(row_buf) - pos, " +-%.2g", err);
                } else {
                    pos += snprintf(row_buf + pos, sizeof(row_buf) - pos, " \033[90m╎±%.2g╎\033[0m", err);
                }
            }
        }

        tui_render_row(f, row_buf, width, true);
        rows_drawn++;
    }

    if (kde) histo_kde_destroy(kde);
    if (fit_res) histo_fit_result_destroy(fit_res);

    // Fill remaining budget rows so height is strictly invariant
    while (rows_drawn < max_rows) {
        tui_render_row(f, "", width, true);
        rows_drawn++;
    }
}

static void render_help_viewport(tui_frame_t *f, const tui_state_t *st, int width, int max_rows) {
    if (st && st->view_mode == VIEW_2D_HEATMAP) {
        const char *help_lines_2d[] = {
            "── Interactive Help Cheatsheet (2D Heatmap Mode) ───────────────────────────",
            "",
            "DISPLAY & INTENSITY SCALING",
            "  l           Toggle Log-Z intensity color scale (LIN <-> LOG-Z)",
            "  C           Toggle TrueColor Viridis gradient / Monochrome ASCII density",
            "  [Space]     Freeze / Unfreeze live streaming display",
            "  c           Clear all accumulators and reset live count",
            "",
            "COMMANDS & PROMPT (:)",
            "  :clear      Clear accumulators",
            "  :quit / :q  Quit application cleanly",
            "  :help       Open this help modal",
            "",
            "Press [?], [Esc], or [Space] to dismiss this help window."
        };

        size_t n_lines = sizeof(help_lines_2d) / sizeof(help_lines_2d[0]);
        int rows_drawn = 0;

        for (size_t i = 0; i < n_lines && rows_drawn < max_rows; ++i) {
            tui_render_row(f, help_lines_2d[i], width, true);
            rows_drawn++;
        }
        while (rows_drawn < max_rows) {
            tui_render_row(f, "", width, true);
            rows_drawn++;
        }
        return;
    }

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
    bool has_weights = false;
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
                   "  -w, --weights      Input stream contains 'value weight' pairs\n"
                   "  -M, --mono         Monochrome mode (disable ANSI colors)\n"
                   "  -h, --help         Show this help message\n");
            return 0;
        } else if (strncmp(arg, "--bins=", 7) == 0) {
            nbins = (uint32_t)atoi(arg + 7);
        } else if (strcmp(arg, "--2d") == 0) {
            is_2d = true;
        } else if (strcmp(arg, "-w") == 0 || strcmp(arg, "--weights") == 0) {
            has_weights = true;
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
    if (!tui_engine_init(&eng, in_fp, is_2d, nbins, rmin, rmax, flags, has_weights)) {
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
                                if (eng.is_2d) {
                                    if (st.frozen_snapshot_2d) histo2d_destroy(st.frozen_snapshot_2d);
                                    st.frozen_snapshot_2d = tui_engine_get_snapshot_2d(&eng);
                                } else {
                                    if (st.frozen_snapshot) histo_destroy(st.frozen_snapshot);
                                    st.frozen_snapshot = tui_engine_get_snapshot_1d(&eng);
                                }
                            } else {
                                if (st.frozen_snapshot) {
                                    histo_destroy(st.frozen_snapshot);
                                    st.frozen_snapshot = NULL;
                                }
                                if (st.frozen_snapshot_2d) {
                                    histo2d_destroy(st.frozen_snapshot_2d);
                                    st.frozen_snapshot_2d = NULL;
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
                            if (eng.is_2d) {
                                st.log_z = !st.log_z;
                            } else {
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
                            }
                            break;
                        case 'k':
                            if (!eng.is_2d) st.show_kde = !st.show_kde;
                            break;
                        case 'f':
                            if (!eng.is_2d) st.show_fit = !st.show_fit;
                            break;
                        case 'a':
                        case 'A':
                            if (!eng.is_2d) tui_engine_set_autorange(&eng, !eng.auto_range, eng.auto_range_threshold);
                            break;
                        case 'e':
                            if (!eng.is_2d) st.show_errors = !st.show_errors;
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
                            if (st.frozen_snapshot_2d) {
                                histo2d_destroy(st.frozen_snapshot_2d);
                                st.frozen_snapshot_2d = NULL;
                            }
                            break;
                        case 'r':
                            if (!eng.is_2d) tui_engine_rebuild_1d(&eng, (nbins > 10) ? nbins / 2 : 5, 0, 0, NULL);
                            break;
                        case 'R':
                            if (!eng.is_2d) tui_engine_rebuild_1d(&eng, nbins * 2, 0, 0, NULL);
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

        histo_t *snap = NULL;
        histo2d_t *snap_2d = NULL;
        uint64_t n_entries = 0;
        double tot_w = 0.0;

        if (eng.is_2d) {
            snap_2d = tui_engine_get_snapshot_2d(&eng);
            n_entries = snap_2d ? histo2d_num_entries(snap_2d) : 0;
            tot_w = snap_2d ? histo2d_total_weight(snap_2d) : 0.0;
        } else {
            snap = st.paused ? st.frozen_snapshot : tui_engine_get_snapshot_1d(&eng);
            n_entries = snap ? histo_num_entries(snap) : 0;
            tot_w = snap ? histo_total_weight(snap) : 0.0;
        }

        tui_frame_clear(&frame);
        tui_frame_puts(&frame, "\033[H"); // Cursor home

        // Row 1: Top border with badge
        char badge[96] = "";
        if (st.paused) {
            if (eng.has_weights) {
                snprintf(badge, sizeof(badge), "[PAUSED | N=%lu | W=%.2f]", (unsigned long)n_entries, tot_w);
            } else {
                snprintf(badge, sizeof(badge), "[PAUSED | N=%lu]", (unsigned long)n_entries);
            }
        } else if (tui_engine_is_finished(&eng)) {
            if (eng.has_weights) {
                snprintf(badge, sizeof(badge), "[EOF | N=%lu | W=%.2f]", (unsigned long)n_entries, tot_w);
            } else {
                snprintf(badge, sizeof(badge), "[EOF | N=%lu]", (unsigned long)n_entries);
            }
        } else {
            double rate = eng.current_rate_ops;
            char rate_str[32];
            if (rate >= 1e6) snprintf(rate_str, sizeof(rate_str), "%.1fM/s", rate / 1e6);
            else if (rate >= 1e3) snprintf(rate_str, sizeof(rate_str), "%.1fk/s", rate / 1e3);
            else snprintf(rate_str, sizeof(rate_str), "%.0f/s", rate);

            if (eng.has_weights) {
                snprintf(badge, sizeof(badge), "[LIVE: %s | N=%lu | W=%.2f]", rate_str, (unsigned long)n_entries, tot_w);
            } else {
                snprintf(badge, sizeof(badge), "[LIVE: %s | N=%lu]", rate_str, (unsigned long)n_entries);
            }
        }
        render_top_border(&frame, eng.is_2d ? "libhisto top (2D)" : "libhisto top", badge, cols);

        // Row 2: Stats summary
        char stats_buf[256];
        if (eng.is_2d) {
            if (snap_2d && n_entries > 0) {
                histo2d_axis_t ax, ay;
                histo2d_axis_x(snap_2d, &ax);
                histo2d_axis_y(snap_2d, &ay);
                snprintf(stats_buf, sizeof(stats_buf),
                         "X: [%.2f, %.2f] %ux │ Y: [%.2f, %.2f] %uy │ Entries: %lu │ Weight: %.2f",
                         ax.min, ax.max, histo2d_nbins_x(snap_2d),
                         ay.min, ay.max, histo2d_nbins_y(snap_2d),
                         (unsigned long)n_entries, tot_w);
            } else {
                snprintf(stats_buf, sizeof(stats_buf), "Waiting for streaming 'x y' samples...");
            }
        } else {
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
        }
        tui_render_row(&frame, stats_buf, cols, true);

        // Row 3: Divider
        render_divider(&frame, cols);

        // Row 4: Subheader
        char subhdr[384];
        if (eng.is_2d) {
            if (snap_2d) {
                histo2d_axis_t ax, ay;
                histo2d_axis_x(snap_2d, &ax);
                histo2d_axis_y(snap_2d, &ay);
                snprintf(subhdr, sizeof(subhdr), "2D Heatmap │ X: %u bins [%.1f, %.1f] │ Y: %u bins [%.1f, %.1f] │ Max bin: %.0f",
                         histo2d_nbins_x(snap_2d), ax.min, ax.max,
                         histo2d_nbins_y(snap_2d), ay.min, ay.max,
                         tot_w);
            } else {
                snprintf(subhdr, sizeof(subhdr), "Initializing 2D...");
            }
        } else if (snap) {
            double r_min = 0, r_max = 0;
            histo_range(snap, &r_min, &r_max);
            uint32_t nb = histo_nbins(snap);
            const char *sc = (st.scale_mode == SCALE_LOG_Y) ? "LOG-Y" :
                             (st.scale_mode == SCALE_LOG_X) ? "LOG-X" :
                             (st.scale_mode == SCALE_LOG_LOG) ? "LOG-LOG" : "LIN";
            char kde_tag[64] = "";
            char fit_tag[64] = "";
            if (st.show_kde && n_entries >= 5) {
                histo_kde_t *sub_kde = histo_kde_create_from_histo(snap, NULL);
                if (sub_kde) {
                    double bw = histo_kde_get_bandwidth(sub_kde);
                    snprintf(kde_tag, sizeof(kde_tag), "│ KDE: Gauss(h=%.2g) ", bw);
                    histo_kde_destroy(sub_kde);
                } else {
                    snprintf(kde_tag, sizeof(kde_tag), "│ KDE ");
                }
            }
            if (st.show_fit && n_entries >= 5) {
                histo_fit_result_t *sub_fit = NULL;
                histo_fit_model(snap, HISTO_FIT_MODEL_GAUSSIAN, NULL, NULL, &sub_fit);
                if (sub_fit && sub_fit->converged) {
                    snprintf(fit_tag, sizeof(fit_tag), "│ Fit: μ=%.2f σ=%.2f ", sub_fit->params[1], sub_fit->params[2]);
                    histo_fit_result_destroy(sub_fit);
                } else {
                    if (sub_fit) histo_fit_result_destroy(sub_fit);
                    snprintf(fit_tag, sizeof(fit_tag), "│ Fit: Gauss ");
                }
            }
            char err_tag[32] = "";
            if (st.show_errors) snprintf(err_tag, sizeof(err_tag), "│ Err: ON ");
            snprintf(subhdr, sizeof(subhdr), "Range: [%.2f, %.2f] │ Bins: %u (Δ=%.2f) │ Scale: %s │ Auto: %s %s%s%s",
                     r_min, r_max, nb, (r_max - r_min) / (nb ? (double)nb : 1.0), sc,
                     eng.auto_range ? "ON" : "OFF",
                     kde_tag, fit_tag, err_tag);
        } else {
            snprintf(subhdr, sizeof(subhdr), "Initializing...");
        }
        tui_render_row(&frame, subhdr, cols, true);

        // Viewport rows budget: rows - 7 lines (Top, Stats, Div, Subhdr, Div, Footer, Bottom)
        int viewport_rows = rows - 7;
        if (viewport_rows < 3) viewport_rows = 3;

        if (st.modal == MODAL_HELP) {
            render_help_viewport(&frame, &st, cols, viewport_rows);
        } else if (eng.is_2d) {
            render_2d_heatmap_viewport(&frame, &st, snap_2d, cols, viewport_rows);
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
            const char *hints = eng.is_2d ?
                "[Space] Freeze  [l] Log-Z Intensity  [C] Mono  [:] Cmd  [?] Help  [q] Quit" :
                "[Space] Freeze  [a] Auto  [l] Log  [k] KDE  [f] Fit  [r/R] Rebin  [:] Cmd  [?] Help  [q] Quit";
            tui_render_row(&frame, hints, cols, true);
        }

        // Row rows: Bottom border (no trailing newline to avoid scrolling on final cell)
        render_bottom_border(&frame, cols, false);
        tui_frame_puts(&frame, "\033[J"); // Erase below in case window grew or shrank

        tui_frame_flush(&frame, stdout);

        if (!st.paused && snap) {
            histo_destroy(snap);
        }
        if (!st.paused && snap_2d) {
            histo2d_destroy(snap_2d);
        }
    }

    if (st.frozen_snapshot) {
        histo_destroy(st.frozen_snapshot);
    }
    if (st.frozen_snapshot_2d) {
        histo2d_destroy(st.frozen_snapshot_2d);
    }

    tui_frame_free(&frame);
    tui_term_restore();
    tui_engine_free(&eng);
    if (in_fp != stdin) fclose(in_fp);
    return 0;
}
