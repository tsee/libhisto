'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const { Histogram, Histogram2D, KDE, Sketch } = require('../index.js');

test('Lifecycle - Explicit destroy and double-free safety', () => {
  // 1D Histogram
  const h1 = Histogram.uniform(10, 0, 10);
  h1.fill(5.0);
  assert.equal(h1.binContent(5), 1.0);

  h1.destroy();
  h1.destroy(); // Multiple destroys must be safe no-op
  assert.throws(() => h1.fill(5.0), /destroyed|Invalid Histogram/);
  assert.throws(() => h1.mean(), /destroyed|Invalid Histogram/);

  // 2D Histogram
  const h2 = Histogram2D.uniform(5, 0, 5, 5, 0, 5);
  h2.fill(1, 1);
  h2.destroy();
  h2.free();
  assert.throws(() => h2.fill(1, 1), /destroyed|Invalid Histogram2D/);

  // KDE
  const kde = new KDE([1, 2, 3, 4, 5]);
  assert.ok(kde.eval(3) > 0);
  kde.destroy();
  kde.destroy();
  assert.throws(() => kde.eval(3), /destroyed|Invalid KDE/);

  // Sketch
  const sk = new Sketch(0.01, 1024);
  sk.insert(10);
  sk.destroy();
  sk.free();
  assert.throws(() => sk.insert(10), /destroyed|Invalid Sketch/);
});

test('Lifecycle - High-frequency allocation GC finalization stress test', () => {
  // Allocate 10,000 objects in a loop without explicit destroy to verify
  // Node-API finalizers run without segfaults or memory crashes.
  for (let i = 0; i < 2000; i++) {
    const h = Histogram.uniform(20, 0, 20);
    h.fill(i % 20);
    const mean = h.mean();
    assert.equal(mean, i % 20 + 0.5);

    const h2 = Histogram2D.uniform(10, 0, 10, 10, 0, 10);
    h2.fill(5, 5);

    const sk = new Sketch(0.05, 128);
    sk.insert(i + 1);

    if (i % 50 === 0) {
      const kde = new KDE([1.0, 2.0, 3.0, 4.0]);
      kde.eval(2.5);
    }
  }
});
