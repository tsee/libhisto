#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>
#include "../../tools/include/tui_shm.h"

static volatile bool g_running = true;
static void handle_sig(int sig) {
    (void)sig;
    g_running = false;
}

int main(int argc, char **argv) {
    const char *shm_path = (argc > 1) ? argv[1] : "/histo_shm";
    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);

    histo_shm_t shm;
    if (!histo_shm_create(&shm, shm_path, 65536, sizeof(double), 0)) {
        fprintf(stderr, "Error: Failed to create shared memory at '%s'\n", shm_path);
        return 1;
    }

    printf("Shared memory ring buffer created at '%s' (capacity: 65536 entries)\n", shm_path);
    printf("Now open a second terminal and run:\n");
    printf("  histo top --shm=%s --bins 50 --palette magma\n\n", shm_path);
    printf("Streaming 100,000 events/sec... Press Ctrl-C to stop.\n");

    double t = 0.0;
    while (g_running) {
        for (int i = 0; i < 1000; ++i) {
            double u1 = (double)rand() / ((double)RAND_MAX + 1.0);
            double u2 = (double)rand() / ((double)RAND_MAX + 1.0);
            if (u1 <= 1e-12) u1 = 1e-12;
            double z = sqrt(-2.0 * log(u1)) * cos(2.0 * 3.141592653589793 * u2);
            double val = 50.0 + 12.0 * sin(t) + 6.0 * z;
            histo_shm_push(shm.ring, &val);
        }
        t += 0.05;
        usleep(10000); /* 100k events/sec */
    }

    printf("\nCleaning up shared memory '%s'...\n", shm_path);
    histo_shm_close(&shm);
    return 0;
}
