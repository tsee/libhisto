#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#if !defined(_WIN32)
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#endif

#if defined(_MSC_VER) && !defined(__cplusplus) && !defined(inline)
#define inline __inline
#endif

/* Fast xorshift64* pseudo-random number generator (~1.2 ns per float) */
static inline uint64_t xorshift64star(uint64_t *state) {
    uint64_t x = *state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *state = x;
    return x * 0x2545F4914F6CDD1DULL;
}

int main(void) {
    /* Ignore SIGPIPE so writer exits cleanly on pipe close */
#ifdef SIGPIPE
    signal(SIGPIPE, SIG_DFL);
#endif

    const size_t BATCH_SIZE = 4096;
    double buf[BATCH_SIZE];
    uint64_t rng_state = 12345678901234567ULL;

    while (1) {
        /* Generate a batch of uniform (0, 1) floats using fast PRNG */
        for (size_t i = 0; i < BATCH_SIZE; ++i) {
            uint64_t r = xorshift64star(&rng_state);
            /* Scale 53-bit mantissa to [0.0, 1.0) */
            buf[i] = (double)(r >> 11) * (1.0 / 9007199254740992.0);
        }

        ssize_t written = write(STDOUT_FILENO, buf, sizeof(buf));
        if (written <= 0) break;
    }

    return 0;
}
