'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const { cli } = require('../index.js');

test('CLI - Help and Version', () => {
  const resHelp = cli.run('--help');
  assert.equal(resHelp.exitCode, 0);
  assert.ok(resHelp.stdout.includes('histo') || resHelp.stderr.includes('histo'));

  const resVer = cli.run('--version');
  assert.equal(resVer.exitCode, 0);
  assert.ok(resVer.stdout.includes('0.2.0') || resVer.stderr.includes('0.2.0'));
});

test('CLI - Fill, Plot, Stats and Fit subcommands', () => {
  const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'histo-cli-test-'));
  const dataFile = path.join(tmpDir, 'data.txt');
  const histoFile = path.join(tmpDir, 'out.histo');

  try {
    // Write sample data
    fs.writeFileSync(dataFile, '1.0\n2.0\n2.5\n3.0\n3.5\n4.0\n4.5\n5.0\n', 'utf8');

    // 1. Fill
    const fillRes = cli.fill('--bins=10', '--min=0', '--max=10', '-f', histoFile, dataFile);
    assert.equal(fillRes.exitCode, 0);
    assert.ok(fs.existsSync(histoFile));

    // 2. Stats
    const statsRes = cli.stats(histoFile);
    assert.equal(statsRes.exitCode, 0);
    assert.ok(statsRes.stdout.includes('Entries') || statsRes.stdout.includes('Mean'));

    // 3. Plot
    const plotRes = cli.plot(histoFile);
    assert.equal(plotRes.exitCode, 0);
    assert.ok(plotRes.stdout.length > 0);

    // 4. Fit
    const fitRes = cli.fit('--model=gaussian', histoFile);
    assert.equal(fitRes.exitCode, 0);
    assert.ok(fitRes.stdout.includes('Converged') || fitRes.stdout.includes('Chi2') || fitRes.stdout.includes('Parameters'));
  } finally {
    fs.rmSync(tmpDir, { recursive: true, force: true });
  }
});
