/**
 * TypeScript Type Definitions for libhisto Node.js bindings.
 */

export const VERSION: string;
export const VERSION_MAJOR: number;
export const VERSION_MINOR: number;
export const VERSION_PATCH: number;

export enum BinType {
  UNIFORM = 0,
  VARIABLE = 1,
}

export enum Flag {
  NONE = 0,
  TRACK_SUMW2 = 1,
  EXACT_MOMENTS = 2,
}

export enum BinRule {
  AUTO = 0,
  FD = 1,
  SCOTT = 2,
  STURGES = 3,
  DOANE = 4,
  KNUTH = 5,
}

export enum Region2D {
  CENTER = 0,
  EAST = 1,
  NORTH = 2,
  SOUTH = 3,
  WEST = 4,
  SOUTH_WEST = 5,
  SOUTH_EAST = 6,
  NORTH_WEST = 7,
  NORTH_EAST = 8,
}

export enum KDEKernel {
  GAUSSIAN = 0,
  EPANECHNIKOV = 1,
  UNIFORM = 2,
  TRIANGULAR = 3,
  BIWEIGHT = 4,
  COSINE = 5,
}

export enum KDEBandwidthMethod {
  SILVERMAN = 0,
  SCOTT = 1,
  MANUAL = 2,
}

export enum FitModel {
  GAUSSIAN = 0,
  EXPONENTIAL = 1,
  POLYNOMIAL = 2,
  BREIT_WIGNER = 3,
  POWER_LAW = 4,
  LOG_NORMAL = 5,
  GAUSSIAN_PLUS_LINEAR = 6,
  WEIBULL = 7,
  GAMMA = 8,
  POISSON = 9,
  LAPLACE = 10,
  CUSTOM = 11,
}

export enum FitLoss {
  CHI2 = 0,
  POISSON_MLE = 1,
  UNWEIGHTED_LS = 2,
}

export enum FitAlgo {
  AUTO = 0,
  LEVENBERG_MARQUARDT = 1,
  LINEAR_LS = 2,
}

export interface HistogramStats {
  numEntries: bigint | number;
  totalWeight: number;
  mean: number;
  variance: number;
  stdDev: number;
  min: number;
  max: number;
  median: number;
}

export interface Histogram2DStats {
  numEntries: bigint | number;
  totalWeight: number;
  meanX: number;
  meanY: number;
  varianceX: number;
  varianceY: number;
  stdDevX: number;
  stdDevY: number;
  covariance: number;
  correlation: number;
  minX: number;
  maxX: number;
  minY: number;
  maxY: number;
}

export interface Chi2Comparison {
  chi2: number;
  ndf: number;
}

export interface BoundingBox2D {
  xmin: number;
  xmax: number;
  ymin: number;
  ymax: number;
}

export interface Point2D {
  x: number;
  y: number;
}

export interface RegionContent2D {
  weight: number;
  count: bigint | number;
}

export interface KDEOptions {
  kernel?: KDEKernel;
  bwMethod?: KDEBandwidthMethod;
  bandwidth?: number;
  bwAdjust?: number;
}

export interface FitOptions {
  maxIterations?: number;
  ftol?: number;
  xtol?: number;
  gtol?: number;
  lossType?: FitLoss;
  algo?: FitAlgo;
  polyDegree?: number;
  rangeMin?: number;
  rangeMax?: number;
  lowerBounds?: Array<number> | Float64Array;
  upperBounds?: Array<number> | Float64Array;
  fixedParams?: Array<boolean>;
}

export interface FitResult {
  numParams: number;
  params: Float64Array;
  paramErrors: Float64Array;
  covMatrix: Float64Array;
  corMatrix: Float64Array;
  chi2: number;
  ndf: number;
  reducedChi2: number;
  pValue: number;
  logLikelihood: number;
  aic: number;
  bic: number;
  iterations: number;
  converged: boolean;
  status: number;
  stopReason: string;
}

export interface CLIResult {
  exitCode: number;
  stdout: string;
  stderr: string;
}

export type NumericArray = Array<number> | Float64Array | Float32Array | Int32Array | Uint32Array;

/**
 * 1-Dimensional Histogram.
 */
export class Histogram {
  constructor(nbins: number, min: number, max: number, flags?: number);

  static uniform(nbins: number, min: number, max: number, flags?: number): Histogram;
  static variable(edges: NumericArray, flags?: number): Histogram;
  static auto(samples: NumericArray, rule?: BinRule, flags?: number): Histogram;
  static estimateBins(samples: NumericArray, rule?: BinRule): { nbins: number; min: number; max: number };
  static deserializeBinary(buffer: Buffer | Uint8Array): Histogram;
  static deserializeJson(jsonString: string): Histogram;
  static migrateBinary(buffer: Buffer | Uint8Array): Buffer;

  destroy(): void;
  free(): void;
  clone(empty?: boolean): Histogram;
  reset(): this;

  fill(x: number, weight?: number): this;
  fillMany(values: NumericArray, weights?: NumericArray | null): this;
  fillBin(binIndex: number, weight: number): this;

  readonly nbins: number;
  readonly binType: BinType;
  readonly range: [number, number];
  readonly min: number;
  readonly max: number;
  readonly totalWeight: number;
  readonly numEntries: bigint | number;
  readonly underflow: number;
  readonly overflow: number;
  readonly nanCount: bigint | number;

  findBin(x: number): number;
  binBounds(binIndex: number): [number, number];
  binCenter(binIndex: number): number;
  binContent(binIndex: number): number;
  binError(binIndex: number): number;
  binSumW2(binIndex: number): number;

  mean(): number;
  variance(): number;
  stdDev(): number;
  centralMoment(k: number): number;
  skewness(): number;
  kurtosis(): number;
  excessKurtosis(): number;
  modeBin(): number;
  modeContinuous(): number;
  fwhm(): number;
  rms(): number;
  quantile(p: number): number;
  median(): number;
  iqr(): number;
  mad(): number;
  trimmedMean(lowerP: number, upperP: number): number;
  winsorizedMean(lowerP: number, upperP: number): number;
  integral(startBin?: number, endBin?: number): number;
  stats(): HistogramStats;

  compareChi2(other: Histogram): Chi2Comparison;
  compareKs(other: Histogram): number;
  compareWasserstein(other: Histogram): number;
  compareKl(other: Histogram): number;
  compareBhattacharyya(other: Histogram): number;

  add(other: Histogram): this;
  subtract(other: Histogram): this;
  multiply(other: Histogram): this;
  divide(other: Histogram): this;
  scale(factor: number): this;
  normalize(targetArea?: number): this;
  rebin(factor: number): Histogram;
  slice(startBin: number, endBin: number, empty?: boolean): Histogram;
  cdf(prenormalization?: number): Histogram;

  serializeBinary(): Buffer;
  serializeJson(): string;

  binContents(): Float64Array;
  binErrors(): Float64Array;
  binCenters(): Float64Array;

  fit(model: FitModel, initialParams?: NumericArray | null, options?: FitOptions): FitResult;
  fitCustom(
    fn: (x: number, params: Float64Array) => number,
    numParams: number,
    initialParams: NumericArray,
    options?: FitOptions
  ): FitResult;
  estimateFitParams(model: FitModel, options?: FitOptions): Float64Array;

  kde(options?: KDEOptions): KDE;
}

/**
 * 2-Dimensional Bivariate Histogram.
 */
export class Histogram2D {
  static uniform(nx: number, xmin: number, xmax: number, ny: number, ymin: number, ymax: number, flags?: number): Histogram2D;
  static variable(xedges: NumericArray, yedges: NumericArray, flags?: number): Histogram2D;
  static uniformVariable(nx: number, xmin: number, xmax: number, yedges: NumericArray, flags?: number): Histogram2D;
  static variableUniform(xedges: NumericArray, ny: number, ymin: number, ymax: number, flags?: number): Histogram2D;
  static deserializeBinary(buffer: Buffer | Uint8Array): Histogram2D;
  static deserializeJson(jsonString: string): Histogram2D;

  destroy(): void;
  free(): void;
  clone(empty?: boolean): Histogram2D;
  reset(): this;

  fill(x: number, y: number, weight?: number): this;
  fillMany(xValues: NumericArray, yValues: NumericArray, weights?: NumericArray | null): this;
  fillBin(ix: number, iy: number, weight: number): this;

  readonly nbinsX: number;
  readonly nbinsY: number;
  readonly totalWeight: number;
  readonly numEntries: bigint | number;
  readonly nanCount: bigint | number;

  findBin(x: number, y: number): { ix: number; iy: number };
  findRegion(x: number, y: number): Region2D;

  binContent(ix: number, iy: number): number;
  binError(ix: number, iy: number): number;
  binSumW2(ix: number, iy: number): number;
  binBounds(ix: number, iy: number): BoundingBox2D;
  binCenter(ix: number, iy: number): Point2D;
  regionContent(region: Region2D): RegionContent2D;

  meanX(): number;
  meanY(): number;
  varianceX(): number;
  varianceY(): number;
  stdDevX(): number;
  stdDevY(): number;
  covariance(): number;
  correlation(): number;
  integral(ixMin?: number, ixMax?: number, iyMin?: number, iyMax?: number): number;
  stats(): Histogram2DStats;

  projectX(): Histogram;
  projectY(): Histogram;
  sliceX(iyMin: number, iyMax: number): Histogram;
  sliceY(ixMin: number, ixMax: number): Histogram;
  profileX(): Histogram;
  profileY(): Histogram;

  scale(factor: number): this;
  normalize(targetIntegral?: number): this;
  rebin(factorX: number, factorY: number): Histogram2D;
  add(other: Histogram2D, scale?: number): this;
  subtract(other: Histogram2D): this;
  multiply(other: Histogram2D): this;
  divide(other: Histogram2D): this;

  serializeBinary(): Buffer;
  serializeJson(): string;
}

/**
 * Kernel Density Estimation.
 */
export class KDE {
  constructor(samples: NumericArray, weights?: NumericArray | null, options?: KDEOptions);

  static create(samples: NumericArray, weights?: NumericArray | null, options?: KDEOptions): KDE;
  static fromHistogram(histogram: Histogram, options?: KDEOptions): KDE;

  destroy(): void;
  free(): void;

  eval(x: number): number;
  evalMany(xArray: NumericArray): Float64Array;
  cdf(x: number): number;
  quantile(q: number): number;
  sample(n: number, seed?: number | bigint): Float64Array;

  readonly bandwidth: number;
  readonly kernel: KDEKernel;
  readonly numPoints: bigint | number;
}

/**
 * Dynamic Online Quantile Sketch (DDSketch).
 */
export class Sketch {
  constructor(alpha?: number, maxBins?: number);

  static deserializeBinary(buffer: Buffer | Uint8Array): Sketch;

  destroy(): void;
  free(): void;

  insert(value: number, weight?: number): this;
  insertMany(values: NumericArray, weights?: NumericArray | null): this;
  quantile(q: number): number;
  merge(other: Sketch): this;
  reset(): this;

  readonly min: number;
  readonly max: number;
  readonly totalWeight: number;
  readonly numEntries: bigint | number;

  serializeBinary(): Buffer;
}

/**
 * Curve Fitting and Regression Utilities.
 */
export namespace Fit {
  function modelNumParams(model: FitModel, polyDegree?: number): number;
  function estimateInitialParams(histogram: Histogram, model: FitModel, options?: FitOptions): Float64Array;
  function fitModel(
    histogram: Histogram,
    model: FitModel,
    initialParams?: NumericArray | null,
    options?: FitOptions
  ): FitResult;
  function fitCustom(
    histogram: Histogram,
    fn: (x: number, params: Float64Array) => number,
    numParams: number,
    initialParams: NumericArray,
    options?: FitOptions
  ): FitResult;
  function eval(model: FitModel, params: NumericArray, x: number): number;
  function evalGradient(model: FitModel, params: NumericArray, x: number): Float64Array;
  function chi2PValue(chi2: number, ndf: number): number;
}

/**
 * CLI runner.
 */
export namespace cli {
  function run(...args: Array<string | number | boolean | Array<string>>): CLIResult;
  function main(...args: Array<string | number | boolean | Array<string>>): CLIResult;
  function fill(...args: Array<string | number | boolean | Array<string>>): CLIResult;
  function plot(...args: Array<string | number | boolean | Array<string>>): CLIResult;
  function stats(...args: Array<string | number | boolean | Array<string>>): CLIResult;
  function fit(...args: Array<string | number | boolean | Array<string>>): CLIResult;
  function cmp(...args: Array<string | number | boolean | Array<string>>): CLIResult;
}
