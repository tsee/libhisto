/*
 * Standalone regression test runner for fuzz targets without LLVM libFuzzer.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif

#include "standalone_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

/* Deterministic 64-bit PRNG (SplitMix64) for regression test reproducibility */
static uint64_t prng_state = 0x853c49e6748fea9bULL;

static inline uint64_t next_prng(void) {
    uint64_t z = (prng_state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static inline uint32_t next_prng_u32(void) {
    return (uint32_t)(next_prng() >> 32);
}

/* Maximum corpus entries to store for mutation pool */
#define MAX_SEEDS 128
#define MAX_SEED_SIZE (64 * 1024)

typedef struct {
    uint8_t *data;
    size_t size;
} seed_entry_t;

static seed_entry_t seeds[MAX_SEEDS];
static size_t num_seeds = 0;

static void add_seed(const uint8_t *data, size_t size) {
    if (num_seeds >= MAX_SEEDS || size > MAX_SEED_SIZE) return;
    uint8_t *copy = (uint8_t *)malloc(size > 0 ? size : 1);
    if (!copy) return;
    if (size > 0 && data) {
        memcpy(copy, data, size);
    }
    seeds[num_seeds].data = copy;
    seeds[num_seeds].size = size;
    num_seeds++;
}

static void free_seeds(void) {
    for (size_t i = 0; i < num_seeds; i++) {
        free(seeds[i].data);
    }
    num_seeds = 0;
}

/* Reads an entire file into memory */
static uint8_t *read_file_bytes(const char *filepath, size_t *out_size) {
    *out_size = 0;
    FILE *f = fopen(filepath, "rb");
    if (!f) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    size_t file_len = (size_t)sz;
    uint8_t *buf = (uint8_t *)malloc(file_len > 0 ? file_len : 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    if (file_len > 0) {
        size_t nread = fread(buf, 1, file_len, f);
        if (nread != file_len) {
            free(buf);
            fclose(f);
            return NULL;
        }
    }
    fclose(f);
    *out_size = file_len;
    return buf;
}

/* Test a single file */
static size_t test_single_file(const char *filepath) {
    size_t sz = 0;
    uint8_t *buf = read_file_bytes(filepath, &sz);
    if (!buf) {
        return 0;
    }
    LLVMFuzzerTestOneInput(buf, sz);
    add_seed(buf, sz);
    free(buf);
    return 1;
}

/* Test all files in a directory */
static size_t test_directory(const char *dirpath) {
    size_t count = 0;
#if defined(_WIN32)
    char search_pattern[MAX_PATH];
    snprintf(search_pattern, sizeof(search_pattern), "%s\\*.*", dirpath);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_pattern, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            char fullpath[MAX_PATH];
            snprintf(fullpath, sizeof(fullpath), "%s\\%s", dirpath, fd.cFileName);
            count += test_single_file(fullpath);
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
#else
    DIR *d = opendir(dirpath);
    if (!d) {
        /* If opendir fails, check if dirpath was actually a file */
        return test_single_file(dirpath);
    }
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);
        struct stat st;
        if (stat(fullpath, &st) == 0 && S_ISREG(st.st_mode)) {
            count += test_single_file(fullpath);
        }
    }
    closedir(d);
#endif
    return count;
}

/* Dictionary of interesting numerical values for mutations */
static const uint8_t INTERESTING_VALUES[][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 0.0 */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80}, /* -0.0 */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x7F}, /* +Inf */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0xFF}, /* -Inf */
    {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x7F}, /* qNaN */
    {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF4, 0x7F}, /* sNaN */
    {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* Subnormal 1 */
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x00}, /* Subnormal 2 */
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEF, 0x7F}, /* DBL_MAX */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00}, /* DBL_MIN */
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, /* All 1s */
    {0x89, 'L', 'H', 'I', 'S', 'T', 0x0A, 0x00}         /* Magic */
};
#define NUM_INTERESTING_VALUES (sizeof(INTERESTING_VALUES) / sizeof(INTERESTING_VALUES[0]))

/* Apply random mutation to a buffer */
static uint8_t *mutate_buffer(const uint8_t *src, size_t src_size, size_t *out_size) {
    size_t target_size = src_size;
    if (target_size == 0) {
        target_size = (next_prng_u32() % 64) + 1;
        uint8_t *buf = (uint8_t *)malloc(target_size);
        if (!buf) return NULL;
        for (size_t i = 0; i < target_size; i++) {
            buf[i] = (uint8_t)next_prng_u32();
        }
        *out_size = target_size;
        return buf;
    }

    uint8_t *buf = (uint8_t *)malloc(src_size + 64);
    if (!buf) return NULL;
    memcpy(buf, src, src_size);

    uint32_t num_ops = (next_prng_u32() % 3) + 1;
    for (uint32_t op = 0; op < num_ops; op++) {
        uint32_t type = next_prng_u32() % 6;
        switch (type) {
            case 0: /* Bit flip */
                if (target_size > 0) {
                    size_t pos = next_prng_u32() % target_size;
                    buf[pos] ^= (uint8_t)(1u << (next_prng_u32() % 8));
                }
                break;
            case 1: /* Byte flip / random byte */
                if (target_size > 0) {
                    size_t pos = next_prng_u32() % target_size;
                    buf[pos] = (uint8_t)next_prng_u32();
                }
                break;
            case 2: /* Insert interesting value */
                if (target_size >= 8) {
                    size_t pos = next_prng_u32() % (target_size - 7);
                    uint32_t val_idx = next_prng_u32() % NUM_INTERESTING_VALUES;
                    memcpy(buf + pos, INTERESTING_VALUES[val_idx], 8);
                }
                break;
            case 3: /* Truncate buffer */
                if (target_size > 4) {
                    target_size = (next_prng_u32() % (target_size - 1)) + 1;
                }
                break;
            case 4: /* Extend buffer */
                if (target_size + 8 <= src_size + 64) {
                    uint32_t val_idx = next_prng_u32() % NUM_INTERESTING_VALUES;
                    memcpy(buf + target_size, INTERESTING_VALUES[val_idx], 8);
                    target_size += 8;
                }
                break;
            case 5: /* Arithmetic perturbation on 16/32 bit word */
                if (target_size >= 4) {
                    size_t pos = next_prng_u32() % (target_size - 3);
                    uint32_t delta = (next_prng_u32() % 31) - 15;
                    uint32_t val;
                    memcpy(&val, buf + pos, 4);
                    val += delta;
                    memcpy(buf + pos, &val, 4);
                }
                break;
        }
    }

    *out_size = target_size;
    return buf;
}

int main(int argc, char **argv) {
    size_t corpus_files_run = 0;
    int max_mutations = 1000;

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--runs=", 7) == 0 || strncmp(argv[i], "-runs=", 6) == 0) {
            const char *val_str = strchr(argv[i], '=') + 1;
            max_mutations = atoi(val_str);
            if (max_mutations < 0) max_mutations = 0;
        } else if (strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("libhisto Standalone Fuzz Runner\n");
            printf("Usage: %s [--runs=N] [corpus_dir | corpus_file ...]\n", argv[0]);
            return 0;
        } else {
            corpus_files_run += test_directory(argv[i]);
        }
    }

    /* Test null and zero-size inputs */
    LLVMFuzzerTestOneInput(NULL, 0);
    uint8_t zero_byte = 0;
    LLVMFuzzerTestOneInput(&zero_byte, 0);

    /* If no seeds loaded from directories/files, add synthetic default seeds */
    if (num_seeds == 0) {
        for (uint32_t v = 0; v < NUM_INTERESTING_VALUES; v++) {
            add_seed(INTERESTING_VALUES[v], 8);
        }
        /* Add a 256-byte zero buffer */
        uint8_t zero_header[256];
        memset(zero_header, 0, sizeof(zero_header));
        add_seed(zero_header, sizeof(zero_header));
    }

    /* Perform randomized mutation iterations */
    for (int iter = 0; iter < max_mutations; iter++) {
        /* Pick random seed */
        size_t seed_idx = next_prng_u32() % num_seeds;
        size_t mutated_size = 0;
        uint8_t *mutated = mutate_buffer(seeds[seed_idx].data, seeds[seed_idx].size, &mutated_size);
        if (mutated) {
            LLVMFuzzerTestOneInput(mutated, mutated_size);
            free(mutated);
        }
    }

    /* Clean up */
    free_seeds();

    printf("[STANDALONE-FUZZ] PASS: Executed %zu corpus files + %d mutations without crash/error.\n",
           corpus_files_run, max_mutations);
    return 0;
}
