#include "simd.h"
#include "internal.h"
#ifdef LIBHISTO_ENABLE_AVX512
#include <immintrin.h>
#include <math.h>

bool histo_fill_uniform_avx512(histo_t *h, const double *x, size_t n) {
    size_t i = 0;
    bool had_non_finite = false;
    double min_val = h->min;
    double max_val = h->max;
    double inv_binsize = h->inv_binsize > 0.0 ? h->inv_binsize : 1.0 / h->binsize;
    uint32_t nbins = h->nbins;
    
    __m512d v_min = _mm512_set1_pd(min_val);
    __m512d v_max = _mm512_set1_pd(max_val);
    __m512d v_inv_binsize = _mm512_set1_pd(inv_binsize);
    __m512d v_nbins_minus_1 = _mm512_set1_pd((double)(nbins - 1));
    __m512d v_zero = _mm512_setzero_pd();

    for (; i + 7 < n; i += 8) {
        __m512d v_x = _mm512_loadu_pd(&x[i]);
        
        __mmask8 mask_under = _mm512_cmp_pd_mask(v_x, v_min, _CMP_LT_OQ);
        __mmask8 mask_over = _mm512_cmp_pd_mask(v_x, v_max, _CMP_GE_OQ);
        __mmask8 mask_nan = _mm512_cmp_pd_mask(v_x, v_x, _CMP_NEQ_UQ); // true if NaN
        
        __mmask8 bad_mask = mask_under | mask_over | mask_nan;
        
        if (bad_mask != 0) {
            for (size_t j = 0; j < 8; ++j) {
                double val = x[i + j];
                if (isnan(val)) { h->n_nan++; had_non_finite = true; }
                else if (val < h->min) {
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
            __m512d v_idx = _mm512_floor_pd(_mm512_mul_pd(_mm512_sub_pd(v_x, v_min), v_inv_binsize));
            v_idx = _mm512_max_pd(v_idx, v_zero);
            v_idx = _mm512_min_pd(v_idx, v_nbins_minus_1);
            
            double idx_arr[8];
            _mm512_storeu_pd(idx_arr, v_idx);
            
            for(int j=0; j<8; ++j) {
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
        if (isnan(val)) { h->n_nan++; had_non_finite = true; }
        else if (val < h->min) {
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
bool histo_fill_uniform_w2_avx512(histo_t *h, const double *x, const double *weights, size_t n) {
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
