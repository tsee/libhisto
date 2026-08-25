'use strict';

/**
 * libhisto Node.js & TypeScript binding.
 * High-performance 1D & 2D histogramming, KDE, DDSketch, curve fitting, and statistical metrics.
 */

let nativeAddon;
try {
  nativeAddon = require('./build/Release/histo_native.node');
} catch (e1) {
  try {
    nativeAddon = require('./build/Debug/histo_native.node');
  } catch (e2) {
    throw new Error(`Failed to load libhisto native addon: ${e1.message}`);
  }
}

// Enums & Constants
const BinType = Object.freeze({
  UNIFORM: nativeAddon.BIN_UNIFORM,
  VARIABLE: nativeAddon.BIN_VARIABLE,
});

const Flag = Object.freeze({
  NONE: nativeAddon.FLAG_NONE,
  TRACK_SUMW2: nativeAddon.FLAG_TRACK_SUMW2,
  EXACT_MOMENTS: nativeAddon.FLAG_EXACT_MOMENTS,
});

const BinRule = Object.freeze({
  AUTO: nativeAddon.BIN_RULE_AUTO,
  FD: nativeAddon.BIN_RULE_FD,
  SCOTT: nativeAddon.BIN_RULE_SCOTT,
  STURGES: nativeAddon.BIN_RULE_STURGES,
  DOANE: nativeAddon.BIN_RULE_DOANE,
  KNUTH: nativeAddon.BIN_RULE_KNUTH,
});

const Region2D = Object.freeze({
  CENTER: nativeAddon.REGION_CENTER,
  EAST: nativeAddon.REGION_EAST,
  NORTH: nativeAddon.REGION_NORTH,
  SOUTH: nativeAddon.REGION_SOUTH,
  WEST: nativeAddon.REGION_WEST,
  SOUTH_WEST: nativeAddon.REGION_SOUTH_WEST,
  SOUTH_EAST: nativeAddon.REGION_SOUTH_EAST,
  NORTH_WEST: nativeAddon.REGION_NORTH_WEST,
  NORTH_EAST: nativeAddon.REGION_NORTH_EAST,
});

const KDEKernel = Object.freeze({
  GAUSSIAN: nativeAddon.KDE_KERNEL_GAUSSIAN,
  EPANECHNIKOV: nativeAddon.KDE_KERNEL_EPANECHNIKOV,
  UNIFORM: nativeAddon.KDE_KERNEL_UNIFORM,
  TRIANGULAR: nativeAddon.KDE_KERNEL_TRIANGULAR,
  BIWEIGHT: nativeAddon.KDE_KERNEL_BIWEIGHT,
  COSINE: nativeAddon.KDE_KERNEL_COSINE,
});

const KDEBandwidthMethod = Object.freeze({
  SILVERMAN: nativeAddon.KDE_BANDWIDTH_SILVERMAN,
  SCOTT: nativeAddon.KDE_BANDWIDTH_SCOTT,
  MANUAL: nativeAddon.KDE_BANDWIDTH_MANUAL,
});

const FitModel = Object.freeze({
  GAUSSIAN: nativeAddon.FIT_MODEL_GAUSSIAN,
  EXPONENTIAL: nativeAddon.FIT_MODEL_EXPONENTIAL,
  POLYNOMIAL: nativeAddon.FIT_MODEL_POLYNOMIAL,
  BREIT_WIGNER: nativeAddon.FIT_MODEL_BREIT_WIGNER,
  POWER_LAW: nativeAddon.FIT_MODEL_POWER_LAW,
  LOG_NORMAL: nativeAddon.FIT_MODEL_LOG_NORMAL,
  GAUSSIAN_PLUS_LINEAR: nativeAddon.FIT_MODEL_GAUSSIAN_PLUS_LINEAR,
  WEIBULL: nativeAddon.FIT_MODEL_WEIBULL,
  GAMMA: nativeAddon.FIT_MODEL_GAMMA,
  POISSON: nativeAddon.FIT_MODEL_POISSON,
  LAPLACE: nativeAddon.FIT_MODEL_LAPLACE,
  CUSTOM: nativeAddon.FIT_MODEL_CUSTOM,
});

const FitLoss = Object.freeze({
  CHI2: nativeAddon.FIT_LOSS_CHI2,
  POISSON_MLE: nativeAddon.FIT_LOSS_POISSON_MLE,
  UNWEIGHTED_LS: nativeAddon.FIT_LOSS_UNWEIGHTED_LS,
});

const FitAlgo = Object.freeze({
  AUTO: nativeAddon.FIT_ALGO_AUTO,
  LEVENBERG_MARQUARDT: nativeAddon.FIT_ALGO_LEVENBERG_MARQUARDT,
  LINEAR_LS: nativeAddon.FIT_ALGO_LINEAR_LS,
});

/**
 * 1-Dimensional Histogram.
 */
class Histogram {
  /**
   * Internal constructor. Use Histogram.uniform, Histogram.variable, or Histogram.auto.
   * @param {object|number} handleOrNbins
   * @param {number} [min]
   * @param {number} [max]
   * @param {number} [flags=0]
   */
  constructor(handleOrNbins, min, max, flags = 0) {
    if (typeof handleOrNbins === 'object' && handleOrNbins !== null) {
      this._native = handleOrNbins;
    } else if (typeof handleOrNbins === 'number' && typeof min === 'number' && typeof max === 'number') {
      this._native = nativeAddon.histo_create_uniform(handleOrNbins, min, max, flags);
    } else {
      throw new TypeError('Invalid arguments to Histogram constructor. Use Histogram.uniform() or Histogram.variable()');
    }
  }

  /**
   * Creates a new uniform (fixed-width) 1D histogram.
   * @param {number} nbins Number of bins (>= 1).
   * @param {number} min Lower coordinate limit.
   * @param {number} max Upper coordinate limit.
   * @param {number} [flags=0] Feature flags (e.g. Flag.TRACK_SUMW2).
   * @returns {Histogram}
   */
  static uniform(nbins, min, max, flags = 0) {
    const handle = nativeAddon.histo_create_uniform(nbins, min, max, flags);
    return new Histogram(handle);
  }

  /**
   * Creates a new variable-bin 1D histogram from strictly monotonic bin edges.
   * @param {Array<number>|Float64Array} edges Array of (nbins + 1) bin edges.
   * @param {number} [flags=0] Feature flags.
   * @returns {Histogram}
   */
  static variable(edges, flags = 0) {
    const handle = nativeAddon.histo_create_variable(edges, flags);
    return new Histogram(handle);
  }

  /**
   * Automatically creates and fills a uniform histogram configured using bin estimation heuristics.
   * @param {Array<number>|Float64Array} samples Sample coordinates.
   * @param {number} [rule=BinRule.AUTO] Heuristic rule.
   * @param {number} [flags=0] Feature flags.
   * @returns {Histogram}
   */
  static auto(samples, rule = BinRule.AUTO, flags = 0) {
    const handle = nativeAddon.histo_create_auto(samples, rule, flags);
    return new Histogram(handle);
  }

  /**
   * Estimates optimal uniform bin parameters (nbins, min, max) for given samples.
   * @param {Array<number>|Float64Array} samples
   * @param {number} [rule=BinRule.AUTO]
   * @returns {{ nbins: number, min: number, max: number }}
   */
  static estimateBins(samples, rule = BinRule.AUTO) {
    return nativeAddon.histo_estimate_bins(samples, rule);
  }

  /**
   * Deserializes a histogram from a binary buffer.
   * @param {Buffer|Uint8Array} buffer
   * @returns {Histogram}
   */
  static deserializeBinary(buffer) {
    const handle = nativeAddon.histo_deserialize_binary(buffer);
    return new Histogram(handle);
  }

  /**
   * Deserializes a histogram from a JSON string.
   * @param {string} jsonString
   * @returns {Histogram}
   */
  static deserializeJson(jsonString) {
    const handle = nativeAddon.histo_deserialize_json(jsonString);
    return new Histogram(handle);
  }

  /**
   * Migrates a binary serialized histogram payload to the current format version.
   * @param {Buffer|Uint8Array} buffer
   * @returns {Buffer}
   */
  static migrateBinary(buffer) {
    return nativeAddon.histo_migrate_binary(buffer);
  }

  /**
   * Explicitly frees native histogram memory. Subsequent calls will throw.
   */
  destroy() {
    if (this._native) {
      nativeAddon.histo_destroy(this._native);
      this._native = null;
    }
  }

  /**
   * Alias for destroy().
   */
  free() {
    this.destroy();
  }

  /**
   * Creates an exact clone or empty schema copy.
   * @param {boolean} [empty=false] If true, create identical geometry with cleared bins.
   * @returns {Histogram}
   */
  clone(empty = false) {
    const handle = nativeAddon.histo_clone(this._native, empty);
    return new Histogram(handle);
  }

  /**
   * Resets all bin contents, moments, and guard counters to zero.
   * @returns {this}
   */
  reset() {
    nativeAddon.histo_reset(this._native);
    return this;
  }

  /**
   * Fills a single value with optional weight.
   * @param {number} x Sample coordinate.
   * @param {number} [weight=1.0] Sample weight.
   * @returns {this}
   */
  fill(x, weight = 1.0) {
    nativeAddon.histo_fill(this._native, x, weight);
    return this;
  }

  /**
   * Batch fills multiple values with optional weights (zero-copy for Float64Array).
   * @param {Array<number>|Float64Array} values Sample coordinates.
   * @param {Array<number>|Float64Array|null} [weights=null] Optional weights array.
   * @returns {this}
   */
  fillMany(values, weights = null) {
    nativeAddon.histo_fill_n(this._native, values, weights);
    return this;
  }

  /**
   * Accumulates weight directly into a specific bin index.
   * @param {number} binIndex Target bin index [0, nbins - 1].
   * @param {number} weight Weight to accumulate.
   * @returns {this}
   */
  fillBin(binIndex, weight) {
    nativeAddon.histo_fill_bin(this._native, binIndex, weight);
    return this;
  }

  get nbins() {
    return nativeAddon.histo_nbins(this._native);
  }

  get binType() {
    return nativeAddon.histo_bin_type(this._native);
  }

  get range() {
    return nativeAddon.histo_range(this._native);
  }

  get min() {
    return this.range[0];
  }

  get max() {
    return this.range[1];
  }

  get totalWeight() {
    return nativeAddon.histo_total_weight(this._native);
  }

  get numEntries() {
    return nativeAddon.histo_num_entries(this._native);
  }

  get underflow() {
    return nativeAddon.histo_underflow(this._native);
  }

  get overflow() {
    return nativeAddon.histo_overflow(this._native);
  }

  get nanCount() {
    return nativeAddon.histo_nan_count(this._native);
  }

  findBin(x) {
    return nativeAddon.histo_find_bin(this._native, x);
  }

  binBounds(binIndex) {
    return nativeAddon.histo_bin_bounds(this._native, binIndex);
  }

  binCenter(binIndex) {
    return nativeAddon.histo_bin_center(this._native, binIndex);
  }

  binContent(binIndex) {
    return nativeAddon.histo_bin_content(this._native, binIndex);
  }

  binError(binIndex) {
    return nativeAddon.histo_bin_error(this._native, binIndex);
  }

  binSumW2(binIndex) {
    return nativeAddon.histo_bin_sum_w2(this._native, binIndex);
  }

  mean() {
    return nativeAddon.histo_mean(this._native);
  }

  variance() {
    return nativeAddon.histo_variance(this._native);
  }

  stdDev() {
    return nativeAddon.histo_std_dev(this._native);
  }

  centralMoment(k) {
    return nativeAddon.histo_central_moment(this._native, k);
  }

  skewness() {
    return nativeAddon.histo_skewness(this._native);
  }

  kurtosis() {
    return nativeAddon.histo_kurtosis(this._native);
  }

  excessKurtosis() {
    return nativeAddon.histo_excess_kurtosis(this._native);
  }

  modeBin() {
    return nativeAddon.histo_mode_bin(this._native);
  }

  modeContinuous() {
    return nativeAddon.histo_mode_continuous(this._native);
  }

  fwhm() {
    return nativeAddon.histo_fwhm(this._native);
  }

  rms() {
    return nativeAddon.histo_rms(this._native);
  }

  quantile(p) {
    return nativeAddon.histo_quantile(this._native, p);
  }

  median() {
    return nativeAddon.histo_median(this._native);
  }

  iqr() {
    return nativeAddon.histo_iqr(this._native);
  }

  mad() {
    return nativeAddon.histo_mad(this._native);
  }

  trimmedMean(lowerP, upperP) {
    return nativeAddon.histo_trimmed_mean(this._native, lowerP, upperP);
  }

  winsorizedMean(lowerP, upperP) {
    return nativeAddon.histo_winsorized_mean(this._native, lowerP, upperP);
  }

  integral(startBin, endBin) {
    if (startBin !== undefined && endBin !== undefined) {
      return nativeAddon.histo_integral(this._native, startBin, endBin);
    }
    return nativeAddon.histo_integral(this._native);
  }

  stats() {
    return nativeAddon.histo_get_stats(this._native);
  }

  compareChi2(other) {
    if (!(other instanceof Histogram)) throw new TypeError('Expected Histogram instance');
    return nativeAddon.histo_cmp_chi2(this._native, other._native);
  }

  compareKs(other) {
    if (!(other instanceof Histogram)) throw new TypeError('Expected Histogram instance');
    return nativeAddon.histo_cmp_ks(this._native, other._native);
  }

  compareWasserstein(other) {
    if (!(other instanceof Histogram)) throw new TypeError('Expected Histogram instance');
    return nativeAddon.histo_cmp_wasserstein_1d(this._native, other._native);
  }

  compareKl(other) {
    if (!(other instanceof Histogram)) throw new TypeError('Expected Histogram instance');
    return nativeAddon.histo_cmp_kl_divergence(this._native, other._native);
  }

  compareBhattacharyya(other) {
    if (!(other instanceof Histogram)) throw new TypeError('Expected Histogram instance');
    return nativeAddon.histo_cmp_bhattacharyya(this._native, other._native);
  }

  add(other) {
    if (!(other instanceof Histogram)) throw new TypeError('Expected Histogram instance');
    nativeAddon.histo_add(this._native, other._native);
    return this;
  }

  subtract(other) {
    if (!(other instanceof Histogram)) throw new TypeError('Expected Histogram instance');
    nativeAddon.histo_subtract(this._native, other._native);
    return this;
  }

  multiply(other) {
    if (!(other instanceof Histogram)) throw new TypeError('Expected Histogram instance');
    nativeAddon.histo_multiply(this._native, other._native);
    return this;
  }

  divide(other) {
    if (!(other instanceof Histogram)) throw new TypeError('Expected Histogram instance');
    nativeAddon.histo_divide(this._native, other._native);
    return this;
  }

  scale(factor) {
    nativeAddon.histo_scale(this._native, factor);
    return this;
  }

  normalize(targetArea = 1.0) {
    nativeAddon.histo_normalize(this._native, targetArea);
    return this;
  }

  rebin(factor) {
    const handle = nativeAddon.histo_rebin(this._native, factor);
    return new Histogram(handle);
  }

  slice(startBin, endBin, empty = false) {
    const handle = nativeAddon.histo_slice(this._native, startBin, endBin, empty);
    return new Histogram(handle);
  }

  cdf(prenormalization = 1.0) {
    const handle = nativeAddon.histo_cdf(this._native, prenormalization);
    return new Histogram(handle);
  }

  serializeBinary() {
    return nativeAddon.histo_serialize_binary(this._native);
  }

  serializeJson() {
    return nativeAddon.histo_serialize_json(this._native);
  }

  binContents() {
    const nb = this.nbins;
    const out = new Float64Array(nb);
    for (let i = 0; i < nb; i++) {
      out[i] = this.binContent(i);
    }
    return out;
  }

  binErrors() {
    const nb = this.nbins;
    const out = new Float64Array(nb);
    for (let i = 0; i < nb; i++) {
      out[i] = this.binError(i);
    }
    return out;
  }

  binCenters() {
    const nb = this.nbins;
    const out = new Float64Array(nb);
    for (let i = 0; i < nb; i++) {
      out[i] = this.binCenter(i);
    }
    return out;
  }

  fit(model, initialParams = null, options = {}) {
    return nativeAddon.fit_model(this._native, model, initialParams, options);
  }

  fitCustom(fn, numParams, initialParams, options = {}) {
    return nativeAddon.fit_custom(this._native, fn, numParams, initialParams, options);
  }

  estimateFitParams(model, options = {}) {
    return nativeAddon.fit_estimate_initial_params(this._native, model, options);
  }

  kde(options = {}) {
    const handle = nativeAddon.kde_create_from_histo(this._native, options);
    return new KDE(handle);
  }
}

/**
 * 2-Dimensional Bivariate Histogram.
 */
class Histogram2D {
  /**
   * Internal constructor. Use Histogram2D.uniform, variable, uniformVariable, or variableUniform.
   * @param {object} handle
   */
  constructor(handle) {
    if (typeof handle !== 'object' || handle === null) {
      throw new TypeError('Invalid arguments to Histogram2D constructor. Use Histogram2D.uniform()');
    }
    this._native = handle;
  }

  static uniform(nx, xmin, xmax, ny, ymin, ymax, flags = 0) {
    const handle = nativeAddon.histo2d_create_uniform(nx, xmin, xmax, ny, ymin, ymax, flags);
    return new Histogram2D(handle);
  }

  static variable(xedges, yedges, flags = 0) {
    const handle = nativeAddon.histo2d_create_variable(xedges, yedges, flags);
    return new Histogram2D(handle);
  }

  static uniformVariable(nx, xmin, xmax, yedges, flags = 0) {
    const handle = nativeAddon.histo2d_create_uniform_variable(nx, xmin, xmax, yedges, flags);
    return new Histogram2D(handle);
  }

  static variableUniform(xedges, ny, ymin, ymax, flags = 0) {
    const handle = nativeAddon.histo2d_create_variable_uniform(xedges, ny, ymin, ymax, flags);
    return new Histogram2D(handle);
  }

  static deserializeBinary(buffer) {
    const handle = nativeAddon.histo2d_deserialize_binary(buffer);
    return new Histogram2D(handle);
  }

  static deserializeJson(jsonString) {
    const handle = nativeAddon.histo2d_deserialize_json(jsonString);
    return new Histogram2D(handle);
  }

  destroy() {
    if (this._native) {
      nativeAddon.histo2d_destroy(this._native);
      this._native = null;
    }
  }

  free() {
    this.destroy();
  }

  clone(empty = false) {
    const handle = nativeAddon.histo2d_clone(this._native, empty);
    return new Histogram2D(handle);
  }

  reset() {
    nativeAddon.histo2d_reset(this._native);
    return this;
  }

  fill(x, y, weight = 1.0) {
    nativeAddon.histo2d_fill(this._native, x, y, weight);
    return this;
  }

  fillMany(xValues, yValues, weights = null) {
    nativeAddon.histo2d_fill_n(this._native, xValues, yValues, weights);
    return this;
  }

  fillBin(ix, iy, weight) {
    nativeAddon.histo2d_fill_bin(this._native, ix, iy, weight);
    return this;
  }

  get nbinsX() {
    return nativeAddon.histo2d_nbins_x(this._native);
  }

  get nbinsY() {
    return nativeAddon.histo2d_nbins_y(this._native);
  }

  get totalWeight() {
    return nativeAddon.histo2d_total_weight(this._native);
  }

  get numEntries() {
    return nativeAddon.histo2d_num_entries(this._native);
  }

  get nanCount() {
    return nativeAddon.histo2d_nan_count(this._native);
  }

  findBin(x, y) {
    return nativeAddon.histo2d_find_bin(this._native, x, y);
  }

  findRegion(x, y) {
    return nativeAddon.histo2d_find_region(this._native, x, y);
  }

  binContent(ix, iy) {
    return nativeAddon.histo2d_bin_content(this._native, ix, iy);
  }

  binError(ix, iy) {
    return nativeAddon.histo2d_bin_error(this._native, ix, iy);
  }

  binSumW2(ix, iy) {
    return nativeAddon.histo2d_bin_sum_w2(this._native, ix, iy);
  }

  binBounds(ix, iy) {
    return nativeAddon.histo2d_bin_bounds(this._native, ix, iy);
  }

  binCenter(ix, iy) {
    return nativeAddon.histo2d_bin_center(this._native, ix, iy);
  }

  regionContent(region) {
    return nativeAddon.histo2d_region_content(this._native, region);
  }

  meanX() {
    return nativeAddon.histo2d_mean_x(this._native);
  }

  meanY() {
    return nativeAddon.histo2d_mean_y(this._native);
  }

  varianceX() {
    return nativeAddon.histo2d_variance_x(this._native);
  }

  varianceY() {
    return nativeAddon.histo2d_variance_y(this._native);
  }

  stdDevX() {
    return nativeAddon.histo2d_std_dev_x(this._native);
  }

  stdDevY() {
    return nativeAddon.histo2d_std_dev_y(this._native);
  }

  covariance() {
    return nativeAddon.histo2d_covariance(this._native);
  }

  correlation() {
    return nativeAddon.histo2d_correlation(this._native);
  }

  integral(ixMin, ixMax, iyMin, iyMax) {
    if (ixMin !== undefined && ixMax !== undefined && iyMin !== undefined && iyMax !== undefined) {
      return nativeAddon.histo2d_integral(this._native, ixMin, ixMax, iyMin, iyMax);
    }
    return nativeAddon.histo2d_integral(this._native);
  }

  stats() {
    return nativeAddon.histo2d_get_stats(this._native);
  }

  projectX() {
    const handle = nativeAddon.histo2d_project_x(this._native);
    return new Histogram(handle);
  }

  projectY() {
    const handle = nativeAddon.histo2d_project_y(this._native);
    return new Histogram(handle);
  }

  sliceX(iyMin, iyMax) {
    const handle = nativeAddon.histo2d_slice_x(this._native, iyMin, iyMax);
    return new Histogram(handle);
  }

  sliceY(ixMin, ixMax) {
    const handle = nativeAddon.histo2d_slice_y(this._native, ixMin, ixMax);
    return new Histogram(handle);
  }

  profileX() {
    const handle = nativeAddon.histo2d_profile_x(this._native);
    return new Histogram(handle);
  }

  profileY() {
    const handle = nativeAddon.histo2d_profile_y(this._native);
    return new Histogram(handle);
  }

  scale(factor) {
    nativeAddon.histo2d_scale(this._native, factor);
    return this;
  }

  normalize(targetIntegral = 1.0) {
    nativeAddon.histo2d_normalize(this._native, targetIntegral);
    return this;
  }

  rebin(factorX, factorY) {
    const handle = nativeAddon.histo2d_rebin(this._native, factorX, factorY);
    return new Histogram2D(handle);
  }

  add(other, scale = 1.0) {
    if (!(other instanceof Histogram2D)) throw new TypeError('Expected Histogram2D instance');
    nativeAddon.histo2d_add(this._native, other._native, scale);
    return this;
  }

  subtract(other) {
    if (!(other instanceof Histogram2D)) throw new TypeError('Expected Histogram2D instance');
    nativeAddon.histo2d_subtract(this._native, other._native);
    return this;
  }

  multiply(other) {
    if (!(other instanceof Histogram2D)) throw new TypeError('Expected Histogram2D instance');
    nativeAddon.histo2d_multiply(this._native, other._native);
    return this;
  }

  divide(other) {
    if (!(other instanceof Histogram2D)) throw new TypeError('Expected Histogram2D instance');
    nativeAddon.histo2d_divide(this._native, other._native);
    return this;
  }

  serializeBinary() {
    return nativeAddon.histo2d_serialize_binary(this._native);
  }

  serializeJson() {
    return nativeAddon.histo2d_serialize_json(this._native);
  }
}

/**
 * Kernel Density Estimation (KDE) Engine.
 */
class KDE {
  constructor(handleOrSamples, weights = null, options = {}) {
    if (typeof handleOrSamples === 'object' && handleOrSamples !== null && !Array.isArray(handleOrSamples) && !ArrayBuffer.isView(handleOrSamples)) {
      this._native = handleOrSamples;
    } else {
      this._native = nativeAddon.kde_create(handleOrSamples, weights, options);
    }
  }

  static create(samples, weights = null, options = {}) {
    const handle = nativeAddon.kde_create(samples, weights, options);
    return new KDE(handle);
  }

  static fromHistogram(histogram, options = {}) {
    if (!(histogram instanceof Histogram)) throw new TypeError('Expected Histogram instance');
    const handle = nativeAddon.kde_create_from_histo(histogram._native, options);
    return new KDE(handle);
  }

  destroy() {
    if (this._native) {
      nativeAddon.kde_destroy(this._native);
      this._native = null;
    }
  }

  free() {
    this.destroy();
  }

  eval(x) {
    return nativeAddon.kde_eval(this._native, x);
  }

  evalMany(xArray) {
    return nativeAddon.kde_eval_n(this._native, xArray);
  }

  cdf(x) {
    return nativeAddon.kde_cdf(this._native, x);
  }

  quantile(q) {
    return nativeAddon.kde_quantile(this._native, q);
  }

  sample(n, seed = 0) {
    return nativeAddon.kde_sample(this._native, n, seed);
  }

  get bandwidth() {
    return nativeAddon.kde_get_bandwidth(this._native);
  }

  get kernel() {
    return nativeAddon.kde_get_kernel(this._native);
  }

  get numPoints() {
    return nativeAddon.kde_num_points(this._native);
  }
}

/**
 * Dynamic Online Quantile Sketch (DDSketch).
 */
class Sketch {
  constructor(handleOrAlpha = 0.01, maxBins = 2048) {
    if (typeof handleOrAlpha === 'object' && handleOrAlpha !== null) {
      this._native = handleOrAlpha;
    } else {
      this._native = nativeAddon.sketch_create(handleOrAlpha, maxBins);
    }
  }

  static deserializeBinary(buffer) {
    const handle = nativeAddon.sketch_deserialize_binary(buffer);
    return new Sketch(handle);
  }

  destroy() {
    if (this._native) {
      nativeAddon.sketch_destroy(this._native);
      this._native = null;
    }
  }

  free() {
    this.destroy();
  }

  insert(value, weight = 1.0) {
    nativeAddon.sketch_insert(this._native, value, weight);
    return this;
  }

  insertMany(values, weights = null) {
    nativeAddon.sketch_insert_n(this._native, values, weights);
    return this;
  }

  quantile(q) {
    return nativeAddon.sketch_quantile(this._native, q);
  }

  merge(other) {
    if (!(other instanceof Sketch)) throw new TypeError('Expected Sketch instance');
    nativeAddon.sketch_merge(this._native, other._native);
    return this;
  }

  reset() {
    nativeAddon.sketch_reset(this._native);
    return this;
  }

  get min() {
    return nativeAddon.sketch_min(this._native);
  }

  get max() {
    return nativeAddon.sketch_max(this._native);
  }

  get totalWeight() {
    return nativeAddon.sketch_total_weight(this._native);
  }

  get numEntries() {
    return nativeAddon.sketch_num_entries(this._native);
  }

  serializeBinary() {
    return nativeAddon.sketch_serialize_binary(this._native);
  }
}

/**
 * Non-linear Curve Fitting and Regression Utilities.
 */
const Fit = Object.freeze({
  modelNumParams(model, polyDegree = 1) {
    return nativeAddon.fit_model_num_params(model, polyDegree);
  },

  estimateInitialParams(histogram, model, options = {}) {
    if (!(histogram instanceof Histogram)) throw new TypeError('Expected Histogram instance');
    return nativeAddon.fit_estimate_initial_params(histogram._native, model, options);
  },

  fitModel(histogram, model, initialParams = null, options = {}) {
    if (!(histogram instanceof Histogram)) throw new TypeError('Expected Histogram instance');
    return nativeAddon.fit_model(histogram._native, model, initialParams, options);
  },

  fitCustom(histogram, fn, numParams, initialParams, options = {}) {
    if (!(histogram instanceof Histogram)) throw new TypeError('Expected Histogram instance');
    return nativeAddon.fit_custom(histogram._native, fn, numParams, initialParams, options);
  },

  eval(model, params, x) {
    return nativeAddon.fit_eval(model, params, x);
  },

  evalGradient(model, params, x) {
    return nativeAddon.fit_eval_gradient(model, params, x);
  },

  chi2PValue(chi2, ndf) {
    return nativeAddon.fit_chi2_p_value(chi2, ndf);
  },
});

/**
 * In-process CLI execution helper.
 */
const cli = Object.freeze({
  /**
   * Runs libhistocli in-process with captured stdout and stderr.
   * @param  {...string} args CLI arguments (e.g. "plot", "stats", "--bins=20")
   * @returns {{ exitCode: number, stdout: string, stderr: string }}
   */
  run(...args) {
    const flatArgs = args.flat().map(String);
    return nativeAddon.cli_run(flatArgs);
  },

  main(...args) {
    return this.run(...args);
  },

  fill(...args) {
    return this.run('fill', ...args);
  },

  plot(...args) {
    return this.run('plot', ...args);
  },

  stats(...args) {
    return this.run('stats', ...args);
  },

  fit(...args) {
    return this.run('fit', ...args);
  },

  cmp(...args) {
    return this.run('cmp', ...args);
  },
});

module.exports = {
  VERSION: nativeAddon.VERSION,
  VERSION_MAJOR: nativeAddon.VERSION_MAJOR,
  VERSION_MINOR: nativeAddon.VERSION_MINOR,
  VERSION_PATCH: nativeAddon.VERSION_PATCH,

  BinType,
  Flag,
  BinRule,
  Region2D,
  KDEKernel,
  KDEBandwidthMethod,
  FitModel,
  FitLoss,
  FitAlgo,

  Histogram,
  Histogram2D,
  KDE,
  Sketch,
  Fit,
  cli,

  _native: nativeAddon,
};
