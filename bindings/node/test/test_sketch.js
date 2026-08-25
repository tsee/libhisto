'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const { Sketch } = require('../index.js');

test('Sketch - Creation, Ingestion & Quantile accuracy', () => {
  const s = new Sketch(0.01, 2048); // 1% relative error
  assert.equal(s.totalWeight, 0);
  assert.equal(s.numEntries, 0);

  // Single insert
  s.insert(10.0);
  s.insert(20.0, 2.0);
  assert.equal(s.totalWeight, 3.0);
  assert.equal(s.numEntries, 2);

  // Batch insert with Float64Array
  const data = new Float64Array(1000);
  for (let i = 0; i < 1000; i++) {
    data[i] = i + 1.0;
  }
  s.insertMany(data);

  assert.equal(s.min, 1.0);
  assert.equal(s.max, 1000.0);
  assert.equal(s.totalWeight, 1003.0);
  assert.equal(s.numEntries, 1002);

  // Quantiles
  const q50 = s.quantile(0.5);
  assert.ok(Math.abs(q50 - 500.0) / 500.0 < 0.05);

  const q90 = s.quantile(0.9);
  assert.ok(Math.abs(q90 - 900.0) / 900.0 < 0.05);

  const q99 = s.quantile(0.99);
  assert.ok(Math.abs(q99 - 990.0) / 990.0 < 0.05);
});

test('Sketch - Merge & Reset', () => {
  const s1 = new Sketch(0.01, 1024);
  const s2 = new Sketch(0.01, 1024);

  for (let i = 1; i <= 50; i++) s1.insert(i);
  for (let i = 51; i <= 100; i++) s2.insert(i);

  s1.merge(s2);
  assert.equal(s1.numEntries, 100);
  assert.equal(s1.min, 1.0);
  assert.equal(s1.max, 1000 ? 100.0 : 100.0);

  const median = s1.quantile(0.5);
  assert.ok(Math.abs(median - 50.0) < 3.0);

  s1.reset();
  assert.equal(s1.numEntries, 0);
  assert.equal(s1.totalWeight, 0);
});

test('Sketch - Binary Serialization Roundtrip', () => {
  const s = new Sketch(0.01, 1024);
  for (let i = 1; i <= 100; i++) {
    s.insert(i * 1.5, i);
  }

  const buf = s.serializeBinary();
  assert.ok(Buffer.isBuffer(buf));
  assert.ok(buf.length > 0);

  const sRestored = Sketch.deserializeBinary(buf);
  assert.equal(sRestored.numEntries, s.numEntries);
  assert.equal(sRestored.totalWeight, s.totalWeight);
  assert.equal(sRestored.min, s.min);
  assert.equal(sRestored.max, s.max);

  const q95Orig = s.quantile(0.95);
  const q95Rest = sRestored.quantile(0.95);
  assert.ok(Math.abs(q95Orig - q95Rest) < 1e-9);
});
