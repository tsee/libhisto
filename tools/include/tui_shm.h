/*
 * Shared memory binary ring buffer protocol and portable IPC ingestion.
 */

#ifndef HISTO_TUI_SHM_H
#define HISTO_TUI_SHM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HISTO_SHM_MAGIC 0x484953544F53484DULL /* "HISTOSHM" */
#define HISTO_SHM_VERSION 1
#define HISTO_SHM_DEFAULT_CAPACITY 65536 /* Must be power of two */

/*
 * Portable binary shared memory ring buffer header.
 * Uses atomic/volatile 64-bit indexes (head and tail) and power-of-two mask.
 * Total capacity of data buffer = (mask + 1) * entry_size bytes.
 */
typedef struct {
    uint64_t magic;         /* HISTO_SHM_MAGIC */
    uint32_t version;       /* Protocol version (1) */
    uint32_t entry_size;    /* Size in bytes of each entry (e.g. sizeof(double) or 2*sizeof(double)) */
    volatile uint64_t head; /* Producer write index (monotonically increasing) */
    volatile uint64_t tail; /* Consumer read index (monotonically increasing) */
    uint64_t mask;          /* Capacity mask: capacity must be a power of 2, mask = capacity - 1 */
    uint64_t dropped;       /* Number of dropped/overwritten events */
    uint64_t flags;         /* Feature flags: bit 0 = is_2d, bit 1 = has_weight */
    uint8_t reserved[16];   /* Reserved padding to align data payload */
    uint8_t data[];         /* Flexible array member for ring buffer payload */
} histo_shm_ring_t;

/* Shm handle container for portable cleanup */
typedef struct {
    histo_shm_ring_t *ring;
    size_t size;
    int fd;
    void *os_handle;
    char path[256];
    bool is_creator;
} histo_shm_t;

/* Lifecycle and IPC helpers */
bool histo_shm_create(histo_shm_t *shm, const char *path, size_t capacity, uint32_t entry_size, uint64_t flags);
bool histo_shm_open(histo_shm_t *shm, const char *path);
void histo_shm_close(histo_shm_t *shm);

/* High-speed lock-free producer / consumer operations */
bool histo_shm_push(histo_shm_ring_t *ring, const void *entry);
size_t histo_shm_pop_batch(histo_shm_ring_t *ring, void *out_entries, size_t max_entries);

#ifdef __cplusplus
}
#endif

#endif /* HISTO_TUI_SHM_H */
