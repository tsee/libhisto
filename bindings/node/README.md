# histo — Node.js & TypeScript Bindings for libhisto

High-performance, memory-safe ISO C99 1D and 2D histogramming, Kernel Density Estimation (KDE), DDSketch dynamic quantile sketching, and non-linear curve fitting for Node.js.

Built with **Node-API (N-API)** for native C performance and guaranteed ABI stability across Node.js versions without recompilation.

---

## Features

- ⚡ **Zero-Copy Float64Array Ingestion**: Batch fill millions of samples directly from typed arrays (`Float64Array`, `Float32Array`, `Int32Array`) without heap allocations or data copies.
- 📊 **1D & 2D Histograms**: Fixed uniform and variable binning, 9-region guard boundary classification, statistical moments, quantiles (median, IQR, MAD, trimmed/winsorized mean), rebinning, slicing, projections, and profiles.
- 📈 **Non-Linear Curve Fitting**: Levenberg-Marquardt optimizer with built-in models (Gaussian, Exponential, Polynomial, Breit-Wigner, Power Law, Log-Normal, Weibull, Gamma, Poisson, Laplace) and **custom JavaScript callback functions** `(x, params) => number`.
- 🌊 **Kernel Density Estimation (KDE)**: Fast evaluation with Gaussian, Epanechnikov, Uniform, Triangular, Biweight, and Cosine kernels, automated Silverman/Scott bandwidth selection, and seeded PRNG sampling.
- 📐 **Dynamic Quantile Sketches (DDSketch)**: Relative-error guaranteed online quantile tracking and sketch merging.
- 💾 **Serialization**: Compact binary wire format and JSON export/import.
- 🔒 **Safe Lifecycle Management**: Automatic V8 GC finalization and explicit `.destroy()` / `.free()` with double-free protection.
- 📘 **TypeScript Ready**: Full type definitions (`index.d.ts`) included out-of-the-box.

---

## Installation & Building

```bash
# Build native addon using node-gyp
npm install
npm run build

# Run automated test suite
npm test
```

---

## Quickstart (JavaScript / TypeScript)

### 1. 1D Histogram & Zero-Copy Ingestion

```javascript
const { Histogram, Flag } = require('histo');

// Create uniform 1D histogram with sum_w2 error tracking
const h = Histogram.uniform(100, 0.0, 100.0, Flag.TRACK_SUMW2);

// Single fills
h.fill(42.5, 1.0);

// Batch fill millions of points with zero-copy Float64Array
const samples = new Float64Array([10.5, 20.3, 42.5, 88.1]);
const weights = new Float64Array([1.0, 2.0, 1.5, 0.5]);
h.fillMany(samples, weights);

// Statistical moments & robust metrics
console.log('Mean:', h.mean());
console.log('StdDev:', h.stdDev());
console.log('Median:', h.median());
console.log('IQR:', h.iqr());
console.log('P95 Quantile:', h.quantile(0.95));

// Binary & JSON Serialization
const buffer = h.serializeBinary();
const hRestored = Histogram.deserializeBinary(buffer);
```

### 2. 2D Bivariate Histogram & Projections

```javascript
const { Histogram2D } = require('histo');

// Uniform 2D histogram: 50x50 bins in [0, 10] x [0, 10]
const h2 = Histogram2D.uniform(50, 0, 10, 50, 0, 10);

const xData = new Float64Array([1.2, 3.4, 5.6]);
const yData = new Float64Array([2.1, 4.3, 6.5]);
h2.fillMany(xData, yData);

console.log('Correlation:', h2.correlation());
console.log('Covariance:', h2.covariance());

// Extract 1D projections and slices
const projX = h2.projectX();
const sliceY = h2.sliceY(10, 20);
```

### 3. Non-Linear Curve Fitting (Built-in & Custom JS Callbacks)

```javascript
const { Histogram, Fit, FitModel, FitLoss } = require('histo');

const h = Histogram.uniform(100, -5, 5);
// ... fill data ...

// Built-in Gaussian peak fitting
const initialParams = h.estimateFitParams(FitModel.GAUSSIAN);
const result = h.fit(FitModel.GAUSSIAN, initialParams, {
  lossType: FitLoss.CHI2,
  maxIterations: 300,
});

console.log('Fitted Amplitude:', result.params[0]);
console.log('Fitted Mean:', result.params[1]);
console.log('Fitted Sigma:', result.params[2]);
console.log('Chi2 / NDF:', result.reducedChi2, 'p-value:', result.pValue);

// Custom JavaScript callback fitting: f(x; p)
const customModel = (x, params) => {
  const [amp, center, width] = params;
  return amp * Math.exp(-0.5 * Math.pow((x - center) / width, 2));
};

const customResult = h.fitCustom(customModel, 3, [50.0, 0.0, 1.0]);
```

### 4. Kernel Density Estimation (KDE)

```javascript
const { KDE, KDEKernel } = require('histo');

const kde = new KDE([1.2, 2.3, 2.5, 3.1, 4.8], null, {
  kernel: KDEKernel.GAUSSIAN,
});

console.log('Bandwidth:', kde.bandwidth);
console.log('PDF at 2.5:', kde.eval(2.5));
console.log('CDF at 2.5:', kde.cdf(2.5));

// Batch evaluation returning Float64Array
const grid = new Float64Array([1.0, 2.0, 3.0, 4.0]);
const pdfValues = kde.evalMany(grid);
```

### 5. DDSketch Dynamic Quantile Sketch

```javascript
const { Sketch } = require('histo');

// Relative error alpha = 0.01 (1%)
const sketch = new Sketch(0.01);
sketch.insert(42.0);
sketch.insertMany(new Float64Array([10, 20, 30, 40, 50]));

console.log('p50:', sketch.quantile(0.50));
console.log('p99:', sketch.quantile(0.99));
```

### 6. In-Process CLI Execution

```javascript
const { cli } = require('histo');

const { exitCode, stdout, stderr } = cli.plot('--bins=20', 'samples.txt');
console.log(stdout);
```

---

## License

MIT License. Copyright (c) Steffen Mueller.
