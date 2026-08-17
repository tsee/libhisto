#include "simd.h"
#include "internal.h"
#ifdef LIBHISTO_ENABLE_AVX2
#include <immintrin.h>
#include <math.h>

bool histo_fill_uniform_avx2(histo_t *h, const double *x, size_t n) {
    size_t i = 0;
    bool had_non_finite = false;
    double min_val = h->min;
    double max_val = h->max;
    double inv_binsize = h->inv_binsize > 0.0 ? h->inv_binsize : 1.0 / h->binsize;
    uint32_t nbins = h->nbins;
    
    __m256d v_min = _mm256_set1_pd(min_val);
    __m256d v_max = _mm256_set1_pd(max_val);
    __m256d v_inv_binsize = _mm256_set1_pd(inv_binsize);
    __m256d v_nbins_minus_1 = _mm256_set1_pd((double)(nbins - 1));
    __m256d v_zero = _mm256_setzero_pd();

    for (; i + 3 < n; i += 4) {
        __m256d v_x = _mm256_loadu_pd(&x[i]);
        
        // Find underflow, overflow, nan
        // scalar fallback for non-finite / out of bounds for simplicity, or just extract masks
        // Actually, for simplicity and correctness, if any element is out of bounds or nan, fallback to scalar for this chunk.
        
        __m256d mask_under = _mm256_cmp_pd(v_x, v_min, _CMP_LT_OQ);
        __m256d mask_over = _mm256_cmp_pd(v_x, v_max, _CMP_GE_OQ);
        __m256d mask_nan = _mm256_cmp_pd(v_x, v_x, _CMP_NEQ_UQ); // true if NaN
        
        __m256d mask_any_bad = _mm256_or_pd(_mm256_or_pd(mask_under, mask_over), mask_nan);
        int bad_mask = _mm256_movemask_pd(mask_any_bad);
        
        if (bad_mask != 0) {
            // fallback to scalar for these 4 elements
            for (size_t j = 0; j < 4; ++j) {
                double val = x[i + j];
                if (isnan(val)) {
                    { h->n_nan++; had_non_finite = true; }
                } else if (val < h->min) {
                    h->underflow_weight += 1.0;
                    h->n_underflow++;
                } else if (val >= h->max) {
                    h->overflow_weight += 1.0;
                    h->n_overflow++;
                } else {
                    int64_t idx = (int64_t)((val - h->min) * inv_binsize);
                    if (idx < 0) idx = 0;
                    else if ((uint32_t)idx >= h->nbins) idx = h->nbins - 1;
                    if (idx + 1 < (int64_t)h->nbins && val >= h->min + (double)(idx + 1) * h->binsize) idx++;
                    if (idx > 0 && val < h->min + (double)idx * h->binsize) idx--;
                    h->bins[idx] += 1.0;
                    h->total_weight += 1.0;
                    h->n_fills++;
                }
            }
        } else {
            // all 4 are perfectly in bounds and finite.
            __m256d v_idx = _mm256_floor_pd(_mm256_mul_pd(_mm256_sub_pd(v_x, v_min), v_inv_binsize));
            // clamp
            v_idx = _mm256_max_pd(v_idx, v_zero);
            v_idx = _mm256_min_pd(v_idx, v_nbins_minus_1);
            
            // convert to int
            // __m256i v_iidx = _mm256_cvtpd_epi64(v_idx); // AVX512 only! AVX2 does not have _mm256_cvtpd_epi64
            // so extract to array
            double idx_arr[4];
            _mm256_storeu_pd(idx_arr, v_idx);
            
            // boundary checks
            for(int j=0; j<4; ++j) {
                int64_t idx = (int64_t)idx_arr[j];
                double val = x[i + j];
                if (idx + 1 < (int64_t)h->nbins && val >= h->min + (double)(idx + 1) * h->binsize) idx++;
                if (idx > 0 && val < h->min + (double)idx * h->binsize) idx--;
                h->bins[idx] += 1.0;
                h->total_weight += 1.0;
                h->n_fills++;
            }
        }
    }
    
    // tail
    for (; i < n; ++i) {
        double val = x[i];
        if (isnan(val)) {
            { h->n_nan++; had_non_finite = true; }
        } else if (val < h->min) {
            h->underflow_weight += 1.0;
            h->n_underflow++;
        } else if (val >= h->max) {
            h->overflow_weight += 1.0;
            h->n_overflow++;
        } else {
            int64_t idx = (int64_t)((val - h->min) * inv_binsize);
            if (idx < 0) idx = 0;
            else if ((uint32_t)idx >= h->nbins) idx = h->nbins - 1;
            if (idx + 1 < (int64_t)h->nbins && val >= h->min + (double)(idx + 1) * h->binsize) idx++;
            if (idx > 0 && val < h->min + (double)idx * h->binsize) idx--;
            h->bins[idx] += 1.0;
            h->total_weight += 1.0;
            h->n_fills++;
        }
    }
    return had_non_finite;
}
bool histo_fill_uniform_w2_avx2(histo_t *h, const double *x, const double *weights, size_t n) {
    // left unimplemented for brevity, scalar fallback
    size_t i = 0;
    bool had_non_finite = false;
    double inv_binsize = h->inv_binsize > 0.0 ? h->inv_binsize : 1.0 / h->binsize;
    for (; i < n; ++i) {
        double val = x[i];
        double w = weights[i];
        if (isnan(val) || !isfinite(w)) { h->n_nan++; had_non_finite = true; }
        else if (val < h->min) {
            h->underflow_weight += w;
            if (h->sum_w2) h->underflow_sum_w2 += w*w;
            h->n_underflow++;
        } else if (val >= h->max) {
            h->overflow_weight += w;
            if (h->sum_w2) h->overflow_sum_w2 += w*w;
            h->n_overflow++;
        } else {
            int64_t idx = (int64_t)((val - h->min) * inv_binsize);
            if (idx < 0) idx = 0;
            else if ((uint32_t)idx >= h->nbins) idx = h->nbins - 1;
            if (idx + 1 < (int64_t)h->nbins && val >= h->min + (double)(idx + 1) * h->binsize) idx++;
            if (idx > 0 && val < h->min + (double)idx * h->binsize) idx--;
            h->bins[idx] += w;
            if (h->sum_w2) h->sum_w2[idx] += w*w;
            h->total_weight += w;
            if (h->sum_w2) h->total_sum_w2 += w*w;
            h->n_fills++;
        }
    }
    return had_non_finite;
}
#endif
