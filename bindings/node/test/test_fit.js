'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const { Histogram, Fit, FitModel, FitLoss, FitAlgo } = require('../index.js');

test('Fit - Built-in Gaussian Peak Model', () => {
  const h = Histogram.uniform(100, -5, 5);
  // Generate Gaussian distribution: A=100, mu=0, sigma=1
  for (let i = 0; i < 100; i++) {
    const x = h.binCenter(i);
    const expectedCount = 100.0 * Math.exp(-(x * x) / (2.0 * 1.0 * 1.0));
    h.fill(x, expectedCount);
  }

  // Model num params
  assert.equal(Fit.modelNumParams(FitModel.GAUSSIAN), 3);

  // Estimate initial parameters
  const initialParams = h.estimateFitParams(FitModel.GAUSSIAN);
  assert.equal(initialParams.length, 3);
  assert.ok(initialParams[0] > 50); // Amplitude
  assert.ok(Math.abs(initialParams[1]) < 1.0); // Mean ~ 0
  assert.ok(initialParams[2] > 0.5 && initialParams[2] < 2.0); // Sigma ~ 1

  // Perform fit
  const res = h.fit(FitModel.GAUSSIAN, initialParams, {
    maxIterations: 200,
    lossType: FitLoss.CHI2,
  });

  assert.equal(res.converged, true);
  assert.equal(res.numParams, 3);
  assert.ok(res.params instanceof Float64Array);
  assert.ok(res.paramErrors instanceof Float64Array);
  assert.ok(res.covMatrix instanceof Float64Array);
  assert.ok(res.corMatrix instanceof Float64Array);

  // Check fitted parameters: A ~ 100, mu ~ 0, sigma ~ 1
  assert.ok(Math.abs(res.params[0] - 100.0) < 1.0);
  assert.ok(Math.abs(res.params[1] - 0.0) < 0.05);
  assert.ok(Math.abs(res.params[2] - 1.0) < 0.05);

  assert.ok(res.chi2 >= 0);
  assert.ok(res.reducedChi2 >= 0);
  assert.ok(res.pValue >= 0 && res.pValue <= 1.0);

  // Model Evaluation & Gradient
  const evalVal = Fit.eval(FitModel.GAUSSIAN, res.params, 0.0);
  assert.ok(Math.abs(evalVal - res.params[0]) < 1e-6);

  const grad = Fit.evalGradient(FitModel.GAUSSIAN, res.params, 0.0);
  assert.equal(grad.length, 3);
  assert.ok(Math.abs(grad[1] - 0.0) < 1e-6); // df/dmu at center is 0
});

test('Fit - Built-in Exponential Decay Model', () => {
  const h = Histogram.uniform(50, 0, 10);
  // f(x) = 50 * exp(-0.5 * x) + 2
  for (let i = 0; i < 50; i++) {
    const x = h.binCenter(i);
    const count = 50.0 * Math.exp(-0.5 * x) + 2.0;
    h.fill(x, count);
  }

  const res = h.fit(FitModel.EXPONENTIAL, [50, 0.5, 2]);
  assert.equal(res.converged, true);
  assert.ok(Math.abs(res.params[0] - 50.0) < 2.0);
  assert.ok(Math.abs(res.params[1] - 0.5) < 0.1);
  assert.ok(Math.abs(res.params[2] - 2.0) < 1.0);
});

test('Fit - Built-in Polynomial Model', () => {
  const h = Histogram.uniform(50, -5, 5);
  // f(x) = 2 + 3*x + 1*x^2
  for (let i = 0; i < 50; i++) {
    const x = h.binCenter(i);
    const count = 2.0 + 3.0 * x + 1.0 * x * x + 50.0;
    h.fill(x, count);
  }

  const res = h.fit(FitModel.POLYNOMIAL, null, {
    polyDegree: 2,
    algo: FitAlgo.LINEAR_LS,
  });

  assert.equal(res.converged, true);
  assert.equal(res.numParams, 3);
  assert.ok(Math.abs(res.params[0] - 52.0) < 1e-3);
  assert.ok(Math.abs(res.params[1] - 3.0) < 1e-3);
  assert.ok(Math.abs(res.params[2] - 1.0) < 1e-3);
});

test('Fit - Custom JavaScript Callback Model', () => {
  const h = Histogram.uniform(40, -4, 4);
  for (let i = 0; i < 40; i++) {
    const x = h.binCenter(i);
    // Gaussian: p0 * exp(-0.5 * ((x - p1) / p2)^2)
    const count = 80.0 * Math.exp(-0.5 * Math.pow((x - 0.5) / 1.2, 2));
    h.fill(x, count);
  }

  // Define custom JavaScript model callback
  const customModel = (x, params) => {
    const [amp, mean, sigma] = params;
    return amp * Math.exp(-0.5 * Math.pow((x - mean) / sigma, 2));
  };

  const initial = [50.0, 0.0, 1.0];
  const res = h.fitCustom(customModel, 3, initial, {
    maxIterations: 300,
  });

  assert.equal(res.converged, true);
  assert.equal(res.numParams, 3);
  assert.ok(Math.abs(res.params[0] - 80.0) < 1.0);
  assert.ok(Math.abs(res.params[1] - 0.5) < 0.05);
  assert.ok(Math.abs(res.params[2] - 1.2) < 0.05);
});

test('Fit - Chi2 p-value computation', () => {
  const pval0 = Fit.chi2PValue(0.0, 10);
  assert.ok(Math.abs(pval0 - 1.0) < 1e-6);

  const pvalLarge = Fit.chi2PValue(100.0, 5);
  assert.ok(pvalLarge < 1e-5);
});
