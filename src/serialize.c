#include "internal.h"
#include <stdio.h>
#include <ctype.h>

#define HISTO_MAGIC_LEN 8
static const uint8_t HISTO_MAGIC[HISTO_MAGIC_LEN] = {0x89, 'L', 'H', 'I', 'S', 'T', 0x0A, 0x00};
#define HISTO_FORMAT_VERSION 1
#define HISTO_HEADER_SIZE 256

/* ========================================================================= */
/* Binary Serialization & Deserialization                                    */
/* ========================================================================= */

size_t histo_serialize_binary_size(const histo_t *h) {
    if (!h) return 0;

    size_t size = HISTO_HEADER_SIZE;
    if (h->bin_type == HISTO_BIN_VARIABLE) {
        size += (size_t)(h->nbins + 1) * sizeof(double);
    }
    size += (size_t)h->nbins * sizeof(double); /* bins */
    if (h->sum_w2) {
        size += (size_t)h->nbins * sizeof(double); /* sum_w2 */
    }
    return size;
}

histo_status_t histo_serialize_binary_into(const histo_t *h, void *buf, size_t capacity) {
    if (!h || !buf) {
        return HISTO_ERR_INVALID_ARG;
    }

    size_t req_size = histo_serialize_binary_size(h);
    if (capacity < req_size) {
        return HISTO_ERR_SERIALIZATION;
    }

    uint8_t *p = (uint8_t *)buf;
    memset(p, 0, HISTO_HEADER_SIZE);

    /* 0x00 - 0x07: Magic */
    memcpy(p + 0x00, HISTO_MAGIC, HISTO_MAGIC_LEN);

    /* 0x08 - 0x09: Version */
    uint16_t version_le = histo_htole16(HISTO_FORMAT_VERSION);
    memcpy(p + 0x08, &version_le, sizeof(uint16_t));

    /* 0x0A - 0x0B: Header Size */
    uint16_t header_size_le = histo_htole16(HISTO_HEADER_SIZE);
    memcpy(p + 0x0A, &header_size_le, sizeof(uint16_t));

    /* 0x0C - 0x0F: Flags (0x01=Variable, 0x02=sum_w2, 0x04=exact_moments) */
    uint32_t wire_flags = 0;
    if (h->bin_type == HISTO_BIN_VARIABLE) {
        wire_flags |= (1u << 0);
    }
    if (h->sum_w2) {
        wire_flags |= (1u << 1);
    }
    if (h->flags & HISTO_FLAG_EXACT_MOMENTS) {
        wire_flags |= (1u << 2);
    }
    uint32_t flags_le = histo_htole32(wire_flags);
    memcpy(p + 0x0C, &flags_le, sizeof(uint32_t));


    /* 0x10 - 0x13: nbins */
    uint32_t nbins_le = histo_htole32(h->nbins);
    memcpy(p + 0x10, &nbins_le, sizeof(uint32_t));

    /* 0x14 - 0x17: reserved padding (already 0) */

    /* 0x18 - 0x1F: min */
    double min_le = histo_dtole(h->min);
    memcpy(p + 0x18, &min_le, sizeof(double));

    /* 0x20 - 0x27: max */
    double max_le = histo_dtole(h->max);
    memcpy(p + 0x20, &max_le, sizeof(double));

    /* 0x28 - 0x2F: total_weight */
    double total_weight_le = histo_dtole(h->total_weight);
    memcpy(p + 0x28, &total_weight_le, sizeof(double));

    /* 0x30 - 0x37: total_sum_w2 */
    double total_sum_w2_le = histo_dtole(h->total_sum_w2);
    memcpy(p + 0x30, &total_sum_w2_le, sizeof(double));

    /* 0x38 - 0x3F: n_fills */
    uint64_t n_fills_le = histo_htole64(h->n_fills);
    memcpy(p + 0x38, &n_fills_le, sizeof(uint64_t));

    /* 0x40 - 0x47: underflow_w */
    double underflow_w_le = histo_dtole(h->underflow_weight);
    memcpy(p + 0x40, &underflow_w_le, sizeof(double));

    /* 0x48 - 0x4F: overflow_w */
    double overflow_w_le = histo_dtole(h->overflow_weight);
    memcpy(p + 0x48, &overflow_w_le, sizeof(double));

    /* 0x50 - 0x57: underflow_w2 */
    double underflow_w2_le = histo_dtole(h->underflow_sum_w2);
    memcpy(p + 0x50, &underflow_w2_le, sizeof(double));

    /* 0x58 - 0x5F: overflow_w2 */
    double overflow_w2_le = histo_dtole(h->overflow_sum_w2);
    memcpy(p + 0x58, &overflow_w2_le, sizeof(double));

    /* 0x60 - 0x67: n_underflow */
    uint64_t n_underflow_le = histo_htole64(h->n_underflow);
    memcpy(p + 0x60, &n_underflow_le, sizeof(uint64_t));

    /* 0x68 - 0x6F: n_overflow */
    uint64_t n_overflow_le = histo_htole64(h->n_overflow);
    memcpy(p + 0x68, &n_overflow_le, sizeof(uint64_t));

    /* 0x70 - 0x77: n_nan */
    uint64_t n_nan_le = histo_htole64(h->n_nan);
    memcpy(p + 0x70, &n_nan_le, sizeof(uint64_t));

    /* 0x78 - 0x7F: stats_mean */
    double stats_mean_le = histo_dtole(h->stats_mean);
    memcpy(p + 0x78, &stats_mean_le, sizeof(double));

    /* 0x80 - 0x87: stats_M2 */
    double stats_M2_le = histo_dtole(h->stats_M2);
    memcpy(p + 0x80, &stats_M2_le, sizeof(double));

    /* 0x88 - 0x8F: stats_min */
    double stats_min_le = histo_dtole(h->stats_min);
    memcpy(p + 0x88, &stats_min_le, sizeof(double));

    /* 0x90 - 0x97: stats_max */
    double stats_max_le = histo_dtole(h->stats_max);
    memcpy(p + 0x90, &stats_max_le, sizeof(double));

    /* Payloads begin at offset 256 */
    uint8_t *payload_ptr = p + HISTO_HEADER_SIZE;

    if (h->bin_type == HISTO_BIN_VARIABLE) {
        for (uint32_t i = 0; i <= h->nbins; ++i) {
            double edge_le = histo_dtole(h->bin_edges[i]);
            memcpy(payload_ptr, &edge_le, sizeof(double));
            payload_ptr += sizeof(double);
        }
    }

    for (uint32_t i = 0; i < h->nbins; ++i) {
        double bin_le = histo_dtole(h->bins[i]);
        memcpy(payload_ptr, &bin_le, sizeof(double));
        payload_ptr += sizeof(double);
    }

    if (h->sum_w2) {
        for (uint32_t i = 0; i < h->nbins; ++i) {
            double w2_le = histo_dtole(h->sum_w2[i]);
            memcpy(payload_ptr, &w2_le, sizeof(double));
            payload_ptr += sizeof(double);
        }
    }

    return HISTO_OK;
}

histo_status_t histo_serialize_binary(const histo_t *h, void **out_buf, size_t *out_size) {
    if (!h || !out_buf || !out_size) {
        return HISTO_ERR_INVALID_ARG;
    }

    size_t req_size = histo_serialize_binary_size(h);
    void *buf = malloc(req_size);
    if (!buf) {
        return HISTO_ERR_NOMEM;
    }

    histo_status_t st = histo_serialize_binary_into(h, buf, req_size);
    if (st != HISTO_OK) {
        free(buf);
        return st;
    }

    *out_buf = buf;
    *out_size = req_size;
    return HISTO_OK;
}

histo_status_t histo_deserialize_binary(const void *buf, size_t size, histo_t **out_h) {
    if (!buf || !out_h) {
        return HISTO_ERR_INVALID_ARG;
    }
    if (size < HISTO_HEADER_SIZE) {
        return HISTO_ERR_DESERIALIZATION;
    }

    const uint8_t *p = (const uint8_t *)buf;

    /* 1. Magic check */
    if (memcmp(p, HISTO_MAGIC, HISTO_MAGIC_LEN) != 0) {
        return HISTO_ERR_DESERIALIZATION;
    }

    /* 2. Version check */
    uint16_t version_raw;
    memcpy(&version_raw, p + 0x08, sizeof(uint16_t));
    uint16_t version = histo_le16toh(version_raw);
    if (version != HISTO_FORMAT_VERSION) {
        return HISTO_ERR_DESERIALIZATION;
    }

    /* 3. Header size check */
    uint16_t header_size_raw;
    memcpy(&header_size_raw, p + 0x0A, sizeof(uint16_t));
    uint16_t header_size = histo_le16toh(header_size_raw);
    if (header_size < HISTO_HEADER_SIZE || (size_t)header_size > size) {
        return HISTO_ERR_DESERIALIZATION;
    }

    /* 4. Flags & nbins */
    uint32_t flags_raw, nbins_raw;
    memcpy(&flags_raw, p + 0x0C, sizeof(uint32_t));
    memcpy(&nbins_raw, p + 0x10, sizeof(uint32_t));
    uint32_t flags = histo_le32toh(flags_raw);
    uint32_t nbins = histo_le32toh(nbins_raw);

    if (nbins == 0 || nbins > HISTO_MAX_NBINS) {
        return HISTO_ERR_DESERIALIZATION;
    }

    bool is_variable = (flags & (1u << 0)) != 0;
    bool has_sumw2 = (flags & (1u << 1)) != 0;
    bool has_moments = (flags & (1u << 2)) != 0;

    /* 5. Payload size calculation (nbins is already checked against HISTO_MAX_NBINS) */
    size_t payload_bytes = (size_t)nbins * sizeof(double); /* bins */
    if (is_variable) {
        payload_bytes += ((size_t)nbins + 1) * sizeof(double); /* bin_edges */
    }
    if (has_sumw2) {
        payload_bytes += (size_t)nbins * sizeof(double); /* sum_w2 */
    }



    if (size < (size_t)header_size + payload_bytes) {
        return HISTO_ERR_DESERIALIZATION;
    }

    /* 6. Read geometry */
    double min_raw, max_raw;
    memcpy(&min_raw, p + 0x18, sizeof(double));
    memcpy(&max_raw, p + 0x20, sizeof(double));
    double min_val = histo_letoh_d(min_raw);
    double max_val = histo_letoh_d(max_raw);

    if (!isfinite(min_val) || !isfinite(max_val) || min_val >= max_val) {
        return HISTO_ERR_DESERIALIZATION;
    }

    const uint8_t *payload_ptr = p + header_size;
    histo_t *h = NULL;

    uint32_t creation_flags = HISTO_FLAG_NONE;
    if (has_sumw2) creation_flags |= HISTO_FLAG_TRACK_SUMW2;
    if (has_moments) creation_flags |= HISTO_FLAG_EXACT_MOMENTS;

    if (is_variable) {
        double *edges = (double *)malloc((nbins + 1) * sizeof(double));
        if (!edges) return HISTO_ERR_NOMEM;

        for (uint32_t i = 0; i <= nbins; ++i) {
            double edge_raw;
            memcpy(&edge_raw, payload_ptr, sizeof(double));
            edges[i] = histo_letoh_d(edge_raw);
            payload_ptr += sizeof(double);
        }

        h = histo_create_variable(nbins, edges, creation_flags);
        free(edges);
    } else {
        h = histo_create_uniform(nbins, min_val, max_val, creation_flags);
    }

    if (!h) {
        return HISTO_ERR_DESERIALIZATION;
    }

    /* Read bins */
    for (uint32_t i = 0; i < nbins; ++i) {
        double bin_raw;
        memcpy(&bin_raw, payload_ptr, sizeof(double));
        h->bins[i] = histo_letoh_d(bin_raw);
        payload_ptr += sizeof(double);
    }

    /* Read sum_w2 if present */
    if (has_sumw2 && h->sum_w2) {
        for (uint32_t i = 0; i < nbins; ++i) {
            double w2_raw;
            memcpy(&w2_raw, payload_ptr, sizeof(double));
            h->sum_w2[i] = histo_letoh_d(w2_raw);
            payload_ptr += sizeof(double);
        }
    }

    /* Read aggregates from header */
    double d_val;
    uint64_t u64_val;

    memcpy(&d_val, p + 0x28, sizeof(double)); h->total_weight = histo_letoh_d(d_val);
    memcpy(&d_val, p + 0x30, sizeof(double)); h->total_sum_w2 = histo_letoh_d(d_val);
    memcpy(&u64_val, p + 0x38, sizeof(uint64_t)); h->n_fills = histo_le64toh(u64_val);
    memcpy(&d_val, p + 0x40, sizeof(double)); h->underflow_weight = histo_letoh_d(d_val);
    memcpy(&d_val, p + 0x48, sizeof(double)); h->overflow_weight = histo_letoh_d(d_val);
    memcpy(&d_val, p + 0x50, sizeof(double)); h->underflow_sum_w2 = histo_letoh_d(d_val);
    memcpy(&d_val, p + 0x58, sizeof(double)); h->overflow_sum_w2 = histo_letoh_d(d_val);
    memcpy(&u64_val, p + 0x60, sizeof(uint64_t)); h->n_underflow = histo_le64toh(u64_val);
    memcpy(&u64_val, p + 0x68, sizeof(uint64_t)); h->n_overflow = histo_le64toh(u64_val);
    memcpy(&u64_val, p + 0x70, sizeof(uint64_t)); h->n_nan = histo_le64toh(u64_val);
    memcpy(&d_val, p + 0x78, sizeof(double)); h->stats_mean = histo_letoh_d(d_val);
    memcpy(&d_val, p + 0x80, sizeof(double)); h->stats_M2 = histo_letoh_d(d_val);
    memcpy(&d_val, p + 0x88, sizeof(double)); h->stats_min = histo_letoh_d(d_val);
    memcpy(&d_val, p + 0x90, sizeof(double)); h->stats_max = histo_letoh_d(d_val);

    *out_h = h;
    return HISTO_OK;
}

