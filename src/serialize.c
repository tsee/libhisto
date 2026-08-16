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

/* ========================================================================= */
/* JSON Serialization & Deserialization                                      */
/* ========================================================================= */

size_t histo_serialize_json_size(const histo_t *h) {
    if (!h) return 0;
    /* Estimate: ~1024 bytes header metadata + ~40 bytes per double formatted as %.17g */
    size_t num_doubles = (size_t)h->nbins * (h->sum_w2 ? 2 : 1);
    if (h->bin_type == HISTO_BIN_VARIABLE) {
        num_doubles += (size_t)(h->nbins + 1);
    }
    return 1024 + num_doubles * 48;
}

histo_status_t histo_serialize_json_into(const histo_t *h, char *buf, size_t capacity) {
    if (!h || !buf || capacity == 0) {
        return HISTO_ERR_INVALID_ARG;
    }

    int written = snprintf(
        buf, capacity,
        "{\n"
        "  \"schema\": \"libhisto-v1\",\n"
        "  \"bin_type\": \"%s\",\n"
        "  \"nbins\": %u,\n"
        "  \"min\": %.17g,\n"
        "  \"max\": %.17g,\n"
        "  \"flags\": %u,\n"
        "  \"total_weight\": %.17g,\n"
        "  \"total_sum_w2\": %.17g,\n"
        "  \"n_fills\": %llu,\n"
        "  \"underflow_weight\": %.17g,\n"
        "  \"overflow_weight\": %.17g,\n"
        "  \"underflow_sum_w2\": %.17g,\n"
        "  \"overflow_sum_w2\": %.17g,\n"
        "  \"n_underflow\": %llu,\n"
        "  \"n_overflow\": %llu,\n"
        "  \"n_nan\": %llu,\n"
        "  \"stats_mean\": %.17g,\n"
        "  \"stats_M2\": %.17g,\n"
        "  \"stats_min\": %.17g,\n"
        "  \"stats_max\": %.17g",
        (h->bin_type == HISTO_BIN_UNIFORM ? "uniform" : "variable"),
        h->nbins,
        h->min,
        h->max,
        h->flags,
        h->total_weight,
        h->total_sum_w2,
        (unsigned long long)h->n_fills,
        h->underflow_weight,
        h->overflow_weight,
        h->underflow_sum_w2,
        h->overflow_sum_w2,
        (unsigned long long)h->n_underflow,
        (unsigned long long)h->n_overflow,
        (unsigned long long)h->n_nan,
        h->stats_mean,
        h->stats_M2,
        h->stats_min,
        h->stats_max
    );

    if (written < 0 || (size_t)written >= capacity) {
        return HISTO_ERR_SERIALIZATION;
    }

    size_t offset = (size_t)written;

    /* Variable bin edges */
    if (h->bin_type == HISTO_BIN_VARIABLE) {
        written = snprintf(buf + offset, capacity - offset, ",\n  \"bin_edges\": [");
        if (written < 0 || (size_t)written >= capacity - offset) return HISTO_ERR_SERIALIZATION;
        offset += (size_t)written;

        for (uint32_t i = 0; i <= h->nbins; ++i) {
            written = snprintf(buf + offset, capacity - offset, "%s%.17g", (i > 0 ? ", " : ""), h->bin_edges[i]);
            if (written < 0 || (size_t)written >= capacity - offset) return HISTO_ERR_SERIALIZATION;
            offset += (size_t)written;
        }
        written = snprintf(buf + offset, capacity - offset, "]");
        if (written < 0 || (size_t)written >= capacity - offset) return HISTO_ERR_SERIALIZATION;
        offset += (size_t)written;
    }

    /* Bins */
    written = snprintf(buf + offset, capacity - offset, ",\n  \"bins\": [");
    if (written < 0 || (size_t)written >= capacity - offset) return HISTO_ERR_SERIALIZATION;
    offset += (size_t)written;

    for (uint32_t i = 0; i < h->nbins; ++i) {
        written = snprintf(buf + offset, capacity - offset, "%s%.17g", (i > 0 ? ", " : ""), h->bins[i]);
        if (written < 0 || (size_t)written >= capacity - offset) return HISTO_ERR_SERIALIZATION;
        offset += (size_t)written;
    }
    written = snprintf(buf + offset, capacity - offset, "]");
    if (written < 0 || (size_t)written >= capacity - offset) return HISTO_ERR_SERIALIZATION;
    offset += (size_t)written;

    /* sum_w2 */
    if (h->sum_w2) {
        written = snprintf(buf + offset, capacity - offset, ",\n  \"sum_w2\": [");
        if (written < 0 || (size_t)written >= capacity - offset) return HISTO_ERR_SERIALIZATION;
        offset += (size_t)written;

        for (uint32_t i = 0; i < h->nbins; ++i) {
            written = snprintf(buf + offset, capacity - offset, "%s%.17g", (i > 0 ? ", " : ""), h->sum_w2[i]);
            if (written < 0 || (size_t)written >= capacity - offset) return HISTO_ERR_SERIALIZATION;
            offset += (size_t)written;
        }
        written = snprintf(buf + offset, capacity - offset, "]");
        if (written < 0 || (size_t)written >= capacity - offset) return HISTO_ERR_SERIALIZATION;
        offset += (size_t)written;
    }

    written = snprintf(buf + offset, capacity - offset, "\n}\n");
    if (written < 0 || (size_t)written >= capacity - offset) return HISTO_ERR_SERIALIZATION;

    return HISTO_OK;
}

histo_status_t histo_serialize_json(const histo_t *h, char **out_json_str) {
    if (!h || !out_json_str) {
        return HISTO_ERR_INVALID_ARG;
    }

    size_t req_size = histo_serialize_json_size(h);
    char *buf = (char *)malloc(req_size);
    if (!buf) {
        return HISTO_ERR_NOMEM;
    }

    histo_status_t st = histo_serialize_json_into(h, buf, req_size);
    if (st != HISTO_OK) {
        free(buf);
        return st;
    }

    *out_json_str = buf;
    return HISTO_OK;
}

/* Helper to parse a double array from JSON string */
static const char* parse_json_double_array(const char *p, double **out_array, uint32_t expected_count) {
    p = strchr(p, '[');
    if (!p) return NULL;
    p++;

    double *arr = (double *)malloc(expected_count * sizeof(double));
    if (!arr) return NULL;

    for (uint32_t i = 0; i < expected_count; ++i) {
        while (*p && (isspace((unsigned char)*p) || *p == ',')) p++;
        if (*p == ']' || *p == '\0') {
            free(arr);
            return NULL;
        }
        char *endptr = NULL;
        arr[i] = strtod(p, &endptr);
        if (endptr == p) {
            free(arr);
            return NULL;
        }
        p = endptr;
    }

    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == ']') p++;

    *out_array = arr;
    return p;
}

histo_status_t histo_deserialize_json(const char *json_str, histo_t **out_h) {
    if (!json_str || !out_h) {
        return HISTO_ERR_INVALID_ARG;
    }

    if (!strstr(json_str, "libhisto-v1")) {
        return HISTO_ERR_DESERIALIZATION;
    }

    bool is_variable = (strstr(json_str, "\"variable\"") != NULL);
    uint32_t nbins = 0;
    double min_val = 0.0, max_val = 0.0;
    uint32_t flags = 0;

    const char *p = strstr(json_str, "\"nbins\":");
    if (!p || sscanf(p, "\"nbins\": %u", &nbins) != 1 || nbins == 0 || nbins > HISTO_MAX_NBINS) {
        return HISTO_ERR_DESERIALIZATION;
    }

    p = strstr(json_str, "\"min\":");
    if (!p || sscanf(p, "\"min\": %lf", &min_val) != 1) return HISTO_ERR_DESERIALIZATION;

    p = strstr(json_str, "\"max\":");
    if (!p || sscanf(p, "\"max\": %lf", &max_val) != 1) return HISTO_ERR_DESERIALIZATION;

    p = strstr(json_str, "\"flags\":");
    if (p) sscanf(p, "\"flags\": %u", &flags);

    histo_t *h = NULL;
    if (is_variable) {
        const char *edges_str = strstr(json_str, "\"bin_edges\":");
        if (!edges_str) return HISTO_ERR_DESERIALIZATION;
        double *edges = NULL;
        if (!parse_json_double_array(edges_str, &edges, nbins + 1)) {
            return HISTO_ERR_DESERIALIZATION;
        }
        h = histo_create_variable(nbins, edges, flags);
        free(edges);
    } else {
        h = histo_create_uniform(nbins, min_val, max_val, flags);
    }

    if (!h) {
        return HISTO_ERR_DESERIALIZATION;
    }

    /* Parse bins array */
    const char *bins_str = strstr(json_str, "\"bins\":");
    if (!bins_str) {
        histo_destroy(h);
        return HISTO_ERR_DESERIALIZATION;
    }
    double *parsed_bins = NULL;
    if (!parse_json_double_array(bins_str, &parsed_bins, nbins)) {
        histo_destroy(h);
        return HISTO_ERR_DESERIALIZATION;
    }
    memcpy(h->bins, parsed_bins, nbins * sizeof(double));
    free(parsed_bins);

    /* Parse sum_w2 if present */
    const char *w2_str = strstr(json_str, "\"sum_w2\":");
    if (w2_str && h->sum_w2) {
        double *parsed_w2 = NULL;
        if (parse_json_double_array(w2_str, &parsed_w2, nbins)) {
            memcpy(h->sum_w2, parsed_w2, nbins * sizeof(double));
            free(parsed_w2);
        }
    }

    /* Parse metadata */
    p = strstr(json_str, "\"total_weight\":"); if (p) sscanf(p, "\"total_weight\": %lf", &h->total_weight);
    p = strstr(json_str, "\"total_sum_w2\":"); if (p) sscanf(p, "\"total_sum_w2\": %lf", &h->total_sum_w2);
    p = strstr(json_str, "\"n_fills\":"); if (p) sscanf(p, "\"n_fills\": %llu", (unsigned long long *)&h->n_fills);
    p = strstr(json_str, "\"underflow_weight\":"); if (p) sscanf(p, "\"underflow_weight\": %lf", &h->underflow_weight);
    p = strstr(json_str, "\"overflow_weight\":"); if (p) sscanf(p, "\"overflow_weight\": %lf", &h->overflow_weight);
    p = strstr(json_str, "\"underflow_sum_w2\":"); if (p) sscanf(p, "\"underflow_sum_w2\": %lf", &h->underflow_sum_w2);
    p = strstr(json_str, "\"overflow_sum_w2\":"); if (p) sscanf(p, "\"overflow_sum_w2\": %lf", &h->overflow_sum_w2);
    p = strstr(json_str, "\"n_underflow\":"); if (p) sscanf(p, "\"n_underflow\": %llu", (unsigned long long *)&h->n_underflow);
    p = strstr(json_str, "\"n_overflow\":"); if (p) sscanf(p, "\"n_overflow\": %llu", (unsigned long long *)&h->n_overflow);
    p = strstr(json_str, "\"n_nan\":"); if (p) sscanf(p, "\"n_nan\": %llu", (unsigned long long *)&h->n_nan);
    p = strstr(json_str, "\"stats_mean\":"); if (p) sscanf(p, "\"stats_mean\": %lf", &h->stats_mean);
    p = strstr(json_str, "\"stats_M2\":"); if (p) sscanf(p, "\"stats_M2\": %lf", &h->stats_M2);
    p = strstr(json_str, "\"stats_min\":"); if (p) sscanf(p, "\"stats_min\": %lf", &h->stats_min);
    p = strstr(json_str, "\"stats_max\":"); if (p) sscanf(p, "\"stats_max\": %lf", &h->stats_max);

    *out_h = h;
    return HISTO_OK;
}
