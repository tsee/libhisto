'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const { Histogram, BinType, Flag, BinRule } = require('../index.js');

test('1D Histogram - Uniform Creation & Geometry', () => {
  const h = Histogram.uniform(10, 0, 10);
  assert.equal(h.nbins, 10);
  assert.equal(h.binType, BinType.UNIFORM);
  assert.deepEqual(h.range, [0, 10]);
  assert.equal(h.min, 0);
  assert.equal(h.max, 10);
  assert.equal(h.totalWeight, 0);
  assert.equal(h.numEntries, 0);
  assert.equal(h.underflow, 0);
  assert.equal(h.overflow, 0);
  assert.equal(h.nanCount, 0);

  for (let i = 0; i < 10; i++) {
    const bounds = h.binBounds(i);
    assert.equal(bounds[0], i);
    assert.equal(bounds[1], i + 1);
    assert.equal(h.binCenter(i), i + 0.5);
    assert.equal(h.binContent(i), 0);
    assert.equal(h.binError(i), 0);
  }

  assert.equal(h.findBin(0.0), 0);
  assert.equal(h.findBin(5.5), 5);
  assert.equal(h.findBin(9.99), 9);
  assert.equal(h.findBin(-1.0), -1); // Underflow
  assert.equal(h.findBin(10.0), 10); // Overflow (upper edge is exclusive/overflow)
});

test('1D Histogram - Variable Bins Creation', () => {
  const edges = [0.0, 1.0, 3.0, 7.0, 15.0];
  const h = Histogram.variable(edges, Flag.TRACK_SUMW2);
  assert.equal(h.nbins, 4);
  assert.equal(h.binType, BinType.VARIABLE);
  assert.deepEqual(h.range, [0, 15]);

  assert.deepEqual(h.binBounds(0), [0, 1]);
  assert.deepEqual(h.binBounds(1), [1, 3]);
  assert.deepEqual(h.binBounds(2), [3, 7]);
  assert.deepEqual(h.binBounds(3), [7, 15]);

  assert.equal(h.binCenter(0), 0.5);
  assert.equal(h.binCenter(1), 2.0);
  assert.equal(h.binCenter(2), 5.0);
  assert.equal(h.binCenter(3), 11.0);

  h.fill(2.0, 2.5);
  assert.equal(h.binContent(1), 2.5);
  assert.equal(h.binSumW2(1), 6.25);
  assert.equal(h.binError(1), 2.5);
});

test('1D Histogram - Auto Binning', () => {
  const samples = new Float64Array([1, 2, 2, 3, 3, 3, 4, 4, 5, 10]);
  const estimated = Histogram.estimateBins(samples, BinRule.AUTO);
  assert(estimated.nbins > 0);
  assert(estimated.min <= 1);
  assert(estimated.max >= 10);

  const h = Histogram.auto(samples, BinRule.FD);
  assert(h.nbins > 0);
  assert.equal(h.totalWeight, 10);
  assert.equal(h.numEntries, 10);
});

test('1D Histogram - Ingestion: fill, fillMany, fillBin', () => {
  const h = Histogram.uniform(10, 0, 10, Flag.TRACK_SUMW2);

  // Single fills
  h.fill(2.5);
  h.fill(2.5, 3.0);
  assert.equal(h.binContent(2), 4.0);
  assert.equal(h.numEntries, 2);

  // Batch fill with Float64Array (Zero-copy)
  const data = new Float64Array([0.5, 1.5, 2.5, 3.5, 4.5]);
  const weights = new Float64Array([1.0, 2.0, 3.0, 4.0, 5.0]);
  h.fillMany(data, weights);

  assert.equal(h.binContent(0), 1.0);
  assert.equal(h.binContent(1), 2.0);
  assert.equal(h.binContent(2), 7.0);
  assert.equal(h.binContent(3), 4.0);
  assert.equal(h.binContent(4), 5.0);

  // fillBin
  h.fillBin(9, 10.0);
  assert.equal(h.binContent(9), 10.0);

  // Out of bounds / Underflow / Overflow
  h.fill(-5.0, 2.0);
  h.fill(15.0, 3.0);
  assert.equal(h.underflow, 2.0);
  assert.equal(h.overflow, 3.0);

  // Reset
  h.reset();
  assert.equal(h.totalWeight, 0);
  assert.equal(h.numEntries, 0);
  assert.equal(h.underflow, 0);
  assert.equal(h.overflow, 0);
});

test('1D Histogram - Statistical Moments & Quantiles', () => {
  const h = Histogram.uniform(100, 0, 100);
  // Fill values 0..99
  const vals = new Float64Array(100);
  for (let i = 0; i < 100; i++) vals[i] = i + 0.5;
  h.fillMany(vals);

  assert.equal(h.totalWeight, 100);
  assert.equal(h.numEntries, 100);

  assert.ok(Math.abs(h.mean() - 50.0) < 1e-6);
  assert.ok(h.variance() > 0);
  assert.ok(Math.abs(h.stdDev() - Math.sqrt(h.variance())) < 1e-9);
  assert.ok(Math.abs(h.median() - 50.0) < 0.5);
  assert.ok(Math.abs(h.quantile(0.5) - 50.0) < 0.5);
  assert.ok(Math.abs(h.quantile(0.25) - 25.0) < 0.5);
  assert.ok(Math.abs(h.quantile(0.75) - 75.0) < 0.5);
  assert.ok(Math.abs(h.iqr() - 50.0) < 1.0);
  assert.ok(h.mad() > 0);
  assert.ok(h.rms() > 0);
  assert.equal(h.centralMoment(0), 1.0);
  assert.ok(Math.abs(h.centralMoment(1)) < 1e-6);

  const stats = h.stats();
  assert.equal(stats.numEntries, 100);
  assert.equal(stats.totalWeight, 100);
  assert.ok(Math.abs(stats.mean - 50.0) < 1e-6);

  const trimmed = h.trimmedMean(0.1, 0.9);
  assert.ok(Math.abs(trimmed - 50.0) < 1.0);
  const winsorized = h.winsorizedMean(0.1, 0.9);
  assert.ok(Math.abs(winsorized - 50.0) < 1.0);

  const integ = h.integral(0, 99);
  assert.equal(integ, 100);
});

test('1D Histogram - Comparisons (Chi2, KS, Wasserstein, KL, Bhattacharyya)', () => {
  const h1 = Histogram.uniform(20, 0, 20);
  const h2 = Histogram.uniform(20, 0, 20);

  for (let i = 0; i < 20; i++) {
    h1.fill(i + 0.5, 10.0);
    h2.fill(i + 0.5, 10.0);
  }

  const chi2 = h1.compareChi2(h2);
  assert.equal(chi2.chi2, 0.0);
  assert.equal(chi2.ndf, 20);

  assert.equal(h1.compareKs(h2), 0.0);
  assert.equal(h1.compareWasserstein(h2), 0.0);
  assert.equal(h1.compareKl(h2), 0.0);
  assert.equal(h1.compareBhattacharyya(h2), 0.0);

  // Perturb h2
  h2.fill(0.5, 50.0);
  assert.ok(h1.compareKs(h2) > 0.0);
  assert.ok(h1.compareWasserstein(h2) > 0.0);
  assert.ok(h1.compareBhattacharyya(h2) > 0.0);
});

test('1D Histogram - Arithmetic & Transformations', () => {
  const h1 = Histogram.uniform(10, 0, 10);
  const h2 = Histogram.uniform(10, 0, 10);

  h1.fill(2.5, 4.0);
  h2.fill(2.5, 2.0);

  // Add
  h1.add(h2);
  assert.equal(h1.binContent(2), 6.0);

  // Subtract
  h1.subtract(h2);
  assert.equal(h1.binContent(2), 4.0);

  // Multiply
  h1.multiply(h2);
  assert.equal(h1.binContent(2), 8.0);

  // Divide
  h1.divide(h2);
  assert.equal(h1.binContent(2), 4.0);

  // Scale
  h1.scale(2.5);
  assert.equal(h1.binContent(2), 10.0);

  // Normalize
  h1.normalize(100.0);
  assert.equal(h1.totalWeight, 100.0);
  assert.equal(h1.binContent(2), 100.0);

  // Rebin
  const rebinned = h1.rebin(2);
  assert.equal(rebinned.nbins, 5);
  assert.equal(rebinned.totalWeight, 100.0);

  // Slice
  const sliced = h1.slice(1, 3);
  assert.equal(sliced.nbins, 3);
  assert.equal(sliced.binContent(1), 100.0); // original bin 2 is now bin 1 in slice [1..3]

  // CDF
  const cdf = h1.cdf(1.0);
  assert.equal(cdf.binContent(2), 1.0);
  assert.equal(cdf.binContent(9), 1.0);

  // Clone
  const cloned = h1.clone();
  assert.equal(cloned.totalWeight, h1.totalWeight);
  const emptyClone = h1.clone(true);
  assert.equal(emptyClone.totalWeight, 0.0);
  assert.equal(emptyClone.nbins, h1.nbins);
});

test('1D Histogram - Binary & JSON Serialization Roundtrip', () => {
  const h = Histogram.uniform(50, -25, 25, Flag.TRACK_SUMW2);
  for (let i = -20; i < 20; i++) {
    h.fill(i + 0.5, Math.abs(i) + 1);
  }

  // Binary roundtrip
  const binBuf = h.serializeBinary();
  assert.ok(Buffer.isBuffer(binBuf));
  assert.ok(binBuf.length >= 256);

  const hFromBin = Histogram.deserializeBinary(binBuf);
  assert.equal(hFromBin.nbins, h.nbins);
  assert.equal(hFromBin.totalWeight, h.totalWeight);
  assert.equal(hFromBin.numEntries, h.numEntries);
  assert.deepEqual(hFromBin.range, h.range);
  assert.equal(hFromBin.compareKs(h), 0.0);

  // JSON roundtrip
  const jsonStr = h.serializeJson();
  assert.equal(typeof jsonStr, 'string');
  assert.ok(jsonStr.includes('"nbins"'));

  const hFromJson = Histogram.deserializeJson(jsonStr);
  assert.equal(hFromJson.nbins, h.nbins);
  assert.equal(hFromJson.totalWeight, h.totalWeight);
  assert.equal(hFromJson.compareKs(h), 0.0);

  // Migration
  const migrated = Histogram.migrateBinary(binBuf);
  assert.ok(Buffer.isBuffer(migrated));
});

test('1D Histogram - TypedArray Buffer Accessors', () => {
  const h = Histogram.uniform(5, 0, 5);
  h.fill(0.5, 10);
  h.fill(2.5, 20);

  const contents = h.binContents();
  assert.ok(contents instanceof Float64Array);
  assert.equal(contents.length, 5);
  assert.equal(contents[0], 10);
  assert.equal(contents[2], 20);

  const centers = h.binCenters();
  assert.ok(centers instanceof Float64Array);
  assert.equal(centers.length, 5);
  assert.equal(centers[0], 0.5);
  assert.equal(centers[4], 4.5);

  const errors = h.binErrors();
  assert.ok(errors instanceof Float64Array);
  assert.equal(errors.length, 5);
  assert.ok(errors[0] > 0);
});
