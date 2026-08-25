'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const { Histogram2D, Region2D, Flag } = require('../index.js');

test('2D Histogram - Creation (all 4 Axis combinations)', () => {
  // 1. Uniform - Uniform
  const hUU = Histogram2D.uniform(10, 0, 10, 20, 0, 20);
  assert.equal(hUU.nbinsX, 10);
  assert.equal(hUU.nbinsY, 20);
  assert.equal(hUU.totalWeight, 0);
  assert.equal(hUU.numEntries, 0);

  // 2. Variable - Variable
  const xedges = [0, 2, 5, 10];
  const yedges = [0, 1, 4, 9, 16];
  const hVV = Histogram2D.variable(xedges, yedges, Flag.TRACK_SUMW2);
  assert.equal(hVV.nbinsX, 3);
  assert.equal(hVV.nbinsY, 4);

  // 3. Uniform - Variable
  const hUV = Histogram2D.uniformVariable(5, 0, 10, yedges);
  assert.equal(hUV.nbinsX, 5);
  assert.equal(hUV.nbinsY, 4);

  // 4. Variable - Uniform
  const hVU = Histogram2D.variableUniform(xedges, 8, 0, 8);
  assert.equal(hVU.nbinsX, 3);
  assert.equal(hVU.nbinsY, 8);
});

test('2D Histogram - Ingestion & 9-Region Guards', () => {
  const h = Histogram2D.uniform(10, 0, 10, 10, 0, 10, Flag.TRACK_SUMW2);

  // In-range fill
  h.fill(2.5, 3.5, 5.0);
  assert.equal(h.binContent(2, 3), 5.0);
  assert.equal(h.binSumW2(2, 3), 25.0);
  assert.equal(h.binError(2, 3), 5.0);
  assert.equal(h.totalWeight, 5.0);
  assert.equal(h.numEntries, 1);

  // Batch fill (Float64Array Zero-copy)
  const x = new Float64Array([0.5, 1.5, 2.5]);
  const y = new Float64Array([0.5, 1.5, 2.5]);
  const w = new Float64Array([1.0, 2.0, 3.0]);
  h.fillMany(x, y, w);

  assert.equal(h.binContent(0, 0), 1.0);
  assert.equal(h.binContent(1, 1), 2.0);
  assert.equal(h.binContent(2, 2), 3.0);
  assert.equal(h.totalWeight, 11.0);

  // fillBin
  h.fillBin(9, 9, 7.0);
  assert.equal(h.binContent(9, 9), 7.0);

  // 9-region out-of-bounds fills
  h.fill(-1.0, 5.0, 2.0); // West
  h.fill(11.0, 5.0, 3.0); // East
  h.fill(5.0, -1.0, 4.0); // South
  h.fill(5.0, 11.0, 5.0); // North
  h.fill(-1.0, -1.0, 6.0); // South-West
  h.fill(11.0, -1.0, 7.0); // South-East
  h.fill(-1.0, 11.0, 8.0); // North-West
  h.fill(11.0, 11.0, 9.0); // North-East

  assert.equal(h.regionContent(Region2D.WEST).weight, 2.0);
  assert.equal(h.regionContent(Region2D.EAST).weight, 3.0);
  assert.equal(h.regionContent(Region2D.SOUTH).weight, 4.0);
  assert.equal(h.regionContent(Region2D.NORTH).weight, 5.0);
  assert.equal(h.regionContent(Region2D.SOUTH_WEST).weight, 6.0);
  assert.equal(h.regionContent(Region2D.SOUTH_EAST).weight, 7.0);
  assert.equal(h.regionContent(Region2D.NORTH_WEST).weight, 8.0);
  assert.equal(h.regionContent(Region2D.NORTH_EAST).weight, 9.0);

  // Geometry
  const bin = h.findBin(2.5, 3.5);
  assert.deepEqual(bin, { ix: 2, iy: 3 });
  assert.equal(h.findRegion(2.5, 3.5), Region2D.CENTER);
  assert.equal(h.findRegion(-1.0, 5.0), Region2D.WEST);

  const bounds = h.binBounds(2, 3);
  assert.deepEqual(bounds, { xmin: 2, xmax: 3, ymin: 3, ymax: 4 });

  const center = h.binCenter(2, 3);
  assert.deepEqual(center, { x: 2.5, y: 3.5 });
});

test('2D Histogram - 2D Statistical Moments & Covariance', () => {
  const h = Histogram2D.uniform(20, 0, 20, 20, 0, 20);
  for (let i = 0; i < 20; i++) {
    h.fill(i + 0.5, i + 0.5, 1.0); // Perfect positive correlation y = x
  }

  assert.equal(h.totalWeight, 20.0);
  assert.equal(h.numEntries, 20);

  assert.ok(Math.abs(h.meanX() - 10.0) < 1e-6);
  assert.ok(Math.abs(h.meanY() - 10.0) < 1e-6);
  assert.ok(h.varianceX() > 0);
  assert.ok(h.varianceY() > 0);
  assert.ok(h.covariance() > 0);
  assert.ok(Math.abs(h.correlation() - 1.0) < 1e-6);

  const st = h.stats();
  assert.equal(st.numEntries, 20);
  assert.ok(Math.abs(st.correlation - 1.0) < 1e-6);

  assert.equal(h.integral(), 20.0);
  assert.equal(h.integral(0, 9, 0, 9), 10.0);
});

test('2D Histogram - Projections, Slices & Profiles', () => {
  const h = Histogram2D.uniform(10, 0, 10, 10, 0, 10);
  for (let x = 0; x < 10; x++) {
    for (let y = 0; y < 10; y++) {
      h.fill(x + 0.5, y + 0.5, 1.0);
    }
  }

  // Projection X
  const projX = h.projectX();
  assert.equal(projX.nbins, 10);
  assert.equal(projX.totalWeight, 100.0);
  for (let i = 0; i < 10; i++) {
    assert.equal(projX.binContent(i), 10.0);
  }

  // Projection Y
  const projY = h.projectY();
  assert.equal(projY.nbins, 10);
  assert.equal(projY.totalWeight, 100.0);

  // Slice X
  const sliceX = h.sliceX(0, 4); // Y bins 0..4 (5 bins total)
  assert.equal(sliceX.nbins, 10);
  assert.equal(sliceX.totalWeight, 50.0);

  // Slice Y
  const sliceY = h.sliceY(0, 2); // X bins 0..2 (3 bins total)
  assert.equal(sliceY.nbins, 10);
  assert.equal(sliceY.totalWeight, 30.0);

  // Profiles
  const profX = h.profileX();
  assert.equal(profX.nbins, 10);
  const profY = h.profileY();
  assert.equal(profY.nbins, 10);
});

test('2D Histogram - Arithmetic & Transformations', () => {
  const h1 = Histogram2D.uniform(10, 0, 10, 10, 0, 10);
  const h2 = Histogram2D.uniform(10, 0, 10, 10, 0, 10);

  h1.fill(2.5, 3.5, 4.0);
  h2.fill(2.5, 3.5, 2.0);

  // Add
  h1.add(h2);
  assert.equal(h1.binContent(2, 3), 6.0);

  // Subtract
  h1.subtract(h2);
  assert.equal(h1.binContent(2, 3), 4.0);

  // Multiply
  h1.multiply(h2);
  assert.equal(h1.binContent(2, 3), 8.0);

  // Divide
  h1.divide(h2);
  assert.equal(h1.binContent(2, 3), 4.0);

  // Scale
  h1.scale(2.5);
  assert.equal(h1.binContent(2, 3), 10.0);

  // Normalize
  h1.normalize(100.0);
  assert.equal(h1.totalWeight, 100.0);

  // Rebin
  const rebinned = h1.rebin(2, 2);
  assert.equal(rebinned.nbinsX, 5);
  assert.equal(rebinned.nbinsY, 5);
  assert.equal(rebinned.totalWeight, 100.0);
});

test('2D Histogram - Binary & JSON Serialization Roundtrip', () => {
  const h = Histogram2D.uniform(10, -5, 5, 10, -5, 5, Flag.TRACK_SUMW2);
  h.fill(1.5, 2.5, 10.0);
  h.fill(-2.5, -1.5, 20.0);

  // Binary roundtrip
  const binBuf = h.serializeBinary();
  assert.ok(Buffer.isBuffer(binBuf));
  const hFromBin = Histogram2D.deserializeBinary(binBuf);
  assert.equal(hFromBin.nbinsX, h.nbinsX);
  assert.equal(hFromBin.nbinsY, h.nbinsY);
  assert.equal(hFromBin.totalWeight, h.totalWeight);
  assert.equal(hFromBin.binContent(6, 7), 10.0);

  // JSON roundtrip
  const jsonStr = h.serializeJson();
  assert.equal(typeof jsonStr, 'string');
  const hFromJson = Histogram2D.deserializeJson(jsonStr);
  assert.equal(hFromJson.nbinsX, h.nbinsX);
  assert.equal(hFromJson.nbinsY, h.nbinsY);
  assert.equal(hFromJson.totalWeight, h.totalWeight);
});
