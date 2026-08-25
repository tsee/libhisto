'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const { Histogram, KDE, KDEKernel, KDEBandwidthMethod } = require('../index.js');

test('KDE - Creation from raw samples', () => {
  const samples = new Float64Array([-2.0, -1.0, 0.0, 1.0, 2.0]);
  const kde = KDE.create(samples, null, {
    kernel: KDEKernel.GAUSSIAN,
    bwMethod: KDEBandwidthMethod.SILVERMAN,
  });

  assert.equal(kde.kernel, KDEKernel.GAUSSIAN);
  assert.equal(kde.numPoints, 5);
  assert.ok(kde.bandwidth > 0.0);

  // PDF evaluation
  const pdfCenter = kde.eval(0.0);
  const pdfTail = kde.eval(10.0);
  assert.ok(pdfCenter > 0.0);
  assert.ok(pdfTail >= 0.0);
  assert.ok(pdfCenter > pdfTail);

  // evalMany
  const xVals = new Float64Array([-1.0, 0.0, 1.0]);
  const pdfs = kde.evalMany(xVals);
  assert.ok(pdfs instanceof Float64Array);
  assert.equal(pdfs.length, 3);
  assert.ok(Math.abs(pdfs[1] - pdfCenter) < 1e-9);

  // CDF
  const cdf0 = kde.cdf(0.0);
  assert.ok(Math.abs(cdf0 - 0.5) < 0.1); // symmetric around 0
  assert.ok(kde.cdf(-100.0) < 1e-4);
  assert.ok(kde.cdf(100.0) > 0.999);

  // Quantile
  const q50 = kde.quantile(0.5);
  assert.ok(Math.abs(q50 - 0.0) < 0.2);

  // Sampling
  const synthetic = kde.sample(50, 42);
  assert.ok(synthetic instanceof Float64Array);
  assert.equal(synthetic.length, 50);
});

test('KDE - Creation from Histogram', () => {
  const h = Histogram.uniform(50, -5, 5);
  for (let i = 0; i < 500; i++) {
    h.fill((Math.random() - 0.5) * 4.0);
  }

  const kde = KDE.fromHistogram(h, {
    kernel: KDEKernel.EPANECHNIKOV,
    bandwidth: 0.5,
  });

  assert.equal(kde.kernel, KDEKernel.EPANECHNIKOV);
  assert.equal(kde.bandwidth, 0.5);
  assert.ok(kde.numPoints > 0);

  const pdf = kde.eval(0.0);
  assert.ok(pdf > 0.0);
});

test('KDE - Kernels and Bandwidth options', () => {
  const samples = [1.0, 2.0, 3.0, 4.0, 5.0];
  const kernels = [
    KDEKernel.GAUSSIAN,
    KDEKernel.EPANECHNIKOV,
    KDEKernel.UNIFORM,
    KDEKernel.TRIANGULAR,
    KDEKernel.BIWEIGHT,
    KDEKernel.COSINE,
  ];

  for (const k of kernels) {
    const kde = new KDE(samples, null, { kernel: k, bwAdjust: 1.2 });
    assert.equal(kde.kernel, k);
    const pdf = kde.eval(3.0);
    assert.ok(pdf > 0.0);
  }
});
