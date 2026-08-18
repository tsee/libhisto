#include "cli_common.h"
#include "histo/version.h"
#include <stdio.h>
#include <string.h>

static void print_usage(const char *prog) {
    printf("libhisto CLI toolkit v%s\n\n", HISTO_VERSION_STRING);
    printf("Usage:\n");
    printf("  %s <command> [options] [arguments...]\n", prog);
    printf("  histo-fill [options] [arguments...]\n");
    printf("  histo-plot [options] [arguments...]\n");
    printf("  histo-stats [options] [arguments...]\n");
    printf("  histo-fit [options] [arguments...]\n");
    printf("  histo-cmp [options] [arguments...]\n\n");
    printf("Commands:\n");
    printf("  fill     Stream data in, aggregate into histogram, and emit binary/JSON/text\n");
    printf("  plot     Render histogram as ASCII / Unicode terminal bar chart\n");
    printf("  stats    Display detailed statistical metrics and moment analysis\n");
    printf("  fit      Fit parametric models (Gaussian, Exponential, Polynomial, Breit-Wigner)\n");
    printf("  cmp      Compare two histograms and compute statistical distance metrics\n\n");

    printf("Flags:\n");
    printf("  -h, --help       Show this help message\n");
    printf("  -v, --version    Show version information\n\n");
    printf("For command-specific help, run: %s <command> --help\n", prog);
}

int main(int argc, char **argv) {
    if (argc < 1) return 1;

    /* Check if invoked directly via symlink (e.g. histo-fill, histo_plot) */
    const char *prog_name = argv[0];
    const char *slash = strrchr(prog_name, '/');
    if (slash) prog_name = slash + 1;

    if (strcmp(prog_name, "histo-fill") == 0 || strcmp(prog_name, "histo_fill") == 0) {
        return cmd_fill_main(argc, argv);
    }
    if (strcmp(prog_name, "histo-plot") == 0 || strcmp(prog_name, "histo_plot") == 0) {
        return cmd_plot_main(argc, argv);
    }
    if (strcmp(prog_name, "histo-stats") == 0 || strcmp(prog_name, "histo_stats") == 0) {
        return cmd_stats_main(argc, argv);
    }
    if (strcmp(prog_name, "histo-cmp") == 0 || strcmp(prog_name, "histo_cmp") == 0 ||
        strcmp(prog_name, "histo-compare") == 0) {
        return cmd_cmp_main(argc, argv);
    }
    if (strcmp(prog_name, "histo-fit") == 0 || strcmp(prog_name, "histo_fit") == 0) {
        return cmd_fit_main(argc, argv);
    }

    /* Multi-call dispatcher */
    if (argc < 2) {
        print_usage(prog_name);
        return 1;
    }

    const char *cmd = argv[1];
    if (strcmp(cmd, "-h") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "help") == 0) {
        print_usage(prog_name);
        return 0;
    }
    if (strcmp(cmd, "-v") == 0 || strcmp(cmd, "--version") == 0 || strcmp(cmd, "version") == 0) {
        printf("libhisto %s\n", HISTO_VERSION_STRING);
        return 0;
    }

    /* Shift argv for sub-command */
    int sub_argc = argc - 1;
    char **sub_argv = argv + 1;

    if (strcmp(cmd, "fill") == 0) {
        return cmd_fill_main(sub_argc, sub_argv);
    }
    if (strcmp(cmd, "plot") == 0 || strcmp(cmd, "draw") == 0) {
        return cmd_plot_main(sub_argc, sub_argv);
    }
    if (strcmp(cmd, "stats") == 0 || strcmp(cmd, "stat") == 0 || strcmp(cmd, "inspect") == 0) {
        return cmd_stats_main(sub_argc, sub_argv);
    }
    if (strcmp(cmd, "fit") == 0) {
        return cmd_fit_main(sub_argc, sub_argv);
    }
    if (strcmp(cmd, "cmp") == 0 || strcmp(cmd, "compare") == 0 || strcmp(cmd, "diff") == 0) {
        return cmd_cmp_main(sub_argc, sub_argv);
    }

    fprintf(stderr, "Error: Unknown command '%s'. Run '%s --help' for usage.\n", cmd, prog_name);
    return 1;
}

