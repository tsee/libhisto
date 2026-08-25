'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const { Histogram, Histogram2D, BinRule, Flag } = require('../index.js');

test('Edge Cases - Invalid parameters error throwing', () => {
  // Invalid bounds: min >= max
  assert.throws(() => Histogram.uniform(10, 10, 10), /bounds|invalid/i);
  assert.throws(() => Histogram.uniform(10, 20, 10), /bounds|invalid/i);

  // Invalid nbins: nbins == 0
  assert.throws(() => Histogram.uniform(0, 0, 10), /nbins|invalid/i);

  // Invalid variable bin edges: non-monotonic
  assert.throws(() => Histogram.variable([0, 5, 2, 10]), /monotonic|invalid/i);
  assert.throws(() => Histogram.variable([0]), /at least 2/i);

  // 2D Invalid creation
  assert.throws(() => Histogram2D.uniform(0, 0, 10, 10, 0, 10), /Failed to create/i);
  assert.throws(() => Histogram2D.variable([0], [0, 1]), /at least 2/i);
});

test('Edge Cases - Non-finite values handling', () => {
  const h = Histogram.uniform(10, 0, 10);

  // NaN fill throws non-finite error and increments nanCount
  assert.throws(() => h.fill(NaN), /non-finite/i);
  assert.equal(h.nanCount, 1);

  // +Infinity is overflow, -Infinity is underflow
  h.fill(Infinity);
  h.fill(-Infinity);
  assert.equal(h.overflow, 1.0);
  assert.equal(h.underflow, 1.0);
});

test('Edge Cases - Empty Histogram statistics throw error', () => {
  const h = Histogram.uniform(10, 0, 10);
  assert.throws(() => h.mean(), /empty/i);
  assert.throws(() => h.variance(), /empty/i);
  assert.throws(() => h.stdDev(), /empty/i);
  assert.throws(() => h.median(), /empty/i);

  const h2 = Histogram2D.uniform(5, 0, 5, 5, 0, 5);
  assert.throws(() => h2.meanX(), /empty/i);
  assert.throws(() => h2.covariance(), /empty/i);
});

test('Edge Cases - Incompatible arithmetic and dimension mismatch throw error', () => {
  const h1 = Histogram.uniform(10, 0, 10);
  const h2 = Histogram.uniform(20, 0, 10); // different nbins

  assert.throws(() => h1.add(h2), /incompatible/i);
  assert.throws(() => h1.subtract(h2), /incompatible/i);
  assert.throws(() => h1.multiply(h2), /incompatible/i);
  assert.throws(() => h1.divide(h2), /incompatible/i);

  const h3 = Histogram.uniform(10, 0, 20); // different range
  assert.throws(() => h1.add(h3), /incompatible/i);
});

test('Edge Cases - Exact boundary coordinates', () => {
  const h = Histogram.uniform(10, 0, 10);

  // Exact lower bound is in bin 0
  assert.equal(h.findBin(0.0), 0);
  h.fill(0.0);
  assert.equal(h.binContent(0), 1.0);

  // Interior boundaries: 1.0 is in bin 1 ([1.0, 2.0))
  assert.equal(h.findBin(1.0), 1);
  h.fill(1.0);
  assert.equal(h.binContent(1), 1.0);

  // Exact upper bound 10.0 is overflow
  assert.equal(h.findBin(10.0), 10);
  h.fill(10.0);
  assert.equal(h.overflow, 1.0);
});
