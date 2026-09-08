/*
 * LibFuzzer target for TUI interactive command parsing and engine mutations.
 */

#include "tui_engine.h"
#include "cli_palette.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if !defined(_WIN32)
#include <strings.h>
#endif

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!data || size == 0 || size > 1024) {
        return 0;
    }

    char cmd[1025];
    memcpy(cmd, data, size);
    cmd[size] = '\0';

    tui_engine_t eng;
    if (!tui_engine_init(&eng, NULL, false, 50, 0.0, 100.0, HISTO_FLAG_NONE, false)) {
        return 0;
    }

    /* Seed reservoir and live histogram with synthetic samples */
    for (size_t i = 0; i < 200; i++) {
        double val = (double)i * 0.5;
        eng.reservoir.col_data[0][i] = val;
        histo_fill(eng.live_1d, val);
    }
    eng.reservoir.count = 200;

    const char *p = cmd;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == ':') p++;
    while (*p && isspace((unsigned char)*p)) p++;

    if (strncasecmp(p, "bins ", 5) == 0 || strncasecmp(p, "b ", 2) == 0) {
        const char *arg = strchr(p, ' ');
        if (arg) {
            while (*arg == ' ') arg++;
            if (strcasecmp(arg, "auto") == 0) {
                eng.auto_range = true;
            } else {
                uint32_t n = (uint32_t)atoi(arg);
                if (n > 0 && n <= 100000) {
                    histo_t *snap = NULL;
                    tui_engine_rebuild_1d(&eng, n, 0, 0, &snap);
                    if (snap) histo_destroy(snap);
                }
            }
        }
    } else if (strncasecmp(p, "range ", 6) == 0 || strncasecmp(p, "r ", 2) == 0) {
        double rmin = 0, rmax = 0;
        const char *arg = p + (*p == 'r' ? 2 : 6);
        if (sscanf(arg, "%lf %lf", &rmin, &rmax) == 2 && rmin < rmax) {
            histo_t *snap = NULL;
            tui_engine_rebuild_1d(&eng, 50, rmin, rmax, &snap);
            if (snap) histo_destroy(snap);
        }
    } else if (strncasecmp(p, "col ", 4) == 0 || strncasecmp(p, "c ", 2) == 0) {
        const char *arg = strchr(p, ' ');
        if (arg) {
            int c = atoi(arg + 1);
            if (c >= 1 && c <= 16) {
                tui_engine_set_column(&eng, c, 0, 0, NULL, NULL);
            }
        }
    } else if (strncasecmp(p, "window ", 7) == 0 || strncasecmp(p, "w ", 2) == 0) {
        const char *arg = strchr(p, ' ');
        if (arg) {
            size_t win = (size_t)atol(arg + 1);
            tui_engine_set_window(&eng, win, NULL, NULL);
        }
    } else if (strncasecmp(p, "decay ", 6) == 0) {
        double d = atof(p + 6);
        tui_engine_set_decay(&eng, d);
    } else if (strncasecmp(p, "palette", 7) == 0 || strncasecmp(p, "p ", 2) == 0) {
        const char *arg = strchr(p, ' ');
        if (arg) {
            while (*arg == ' ') arg++;
            (void)histo_palette_from_name(arg);
        }
    }

    /* Acquire snapshot to ensure consistent state */
    histo_t *snap = tui_engine_get_snapshot_1d(&eng);
    if (snap) {
        histo_destroy(snap);
    }

    tui_engine_free(&eng);
    return 0;
}
