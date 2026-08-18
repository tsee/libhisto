# CLI Toolkit Extensions: Design Specification

## 1. Overview & Incremental Learning Philosophy

The `libhisto` CLI toolkit extensions adhere strictly to Unix pipeline composability and progressive disclosure:
- **Level 1 (Zero-Config Pipes)**: Run `cat data.txt | histo fit` or `cat data.txt | histo fill --2d > out.json` without configuring options.
- **Level 2 (Structured Formats & Models)**: Select fitting models (`--model exponential`), output formats (`--json`), and bin geometry (`--bins 100`).
- **Level 3 (Constrained Optimization & Diagnostics)**: Box constraints (`--bounds`), frozen parameters (`--fix-param`), Poisson MLE (`--mle`), and residual analysis.

---

## 2. Command Architecture & Sub-Commands

| Binary Symlink | Sub-command | Purpose |
| :--- | :--- | :--- |
| `histo-fit` | `histo fit` | Binned parametric curve fitting with formula and parameter table |
| `histo-fill` | `histo fill` | Stream ingestion for 1D and 2D (`--2d`), JSON/binary export |
| `histo-plot` | `histo plot` | Terminal histogram visualization (vertical/horizontal bars, 2D heatmaps) |
| `histo-stats` | `histo stats` | Moments, quantiles, robust dispersion, 2D bivariate statistics |
| `histo-cmp` | `histo cmp` | Two-distribution statistical distance metrics ($\chi^2$, KS, Wasserstein, KL) |

---

## 3. `histo fit` Specification

### Default Output Format
`histo fit` outputs the fitted mathematical formula, a formatted parameter table, and convergence diagnostics:

```text
================================================================================
 MODEL: Gaussian Resonance [ f(x) = A · exp(-(x - μ)² / (2σ²)) ]
================================================================================
  Param  Name               Estimate      Std. Error       95% Confidence Interval
  [0]    Amplitude (A)      1248.520     ±  14.281        [ 1220.528, 1276.512 ]
  [1]    Mean (μ)             50.012     ±   0.011        [   49.990,   50.034 ]
  [2]    Std Dev (σ)           1.488     ±   0.010        [    1.468,    1.508 ]
--------------------------------------------------------------------------------
 GOODNESS OF FIT:
  χ² / NDF       = 37.42 / 37 (1.011)
  p-value        = 0.4497 (Consistent with model)
  Log-Likelihood = -142.81  |  AIC = 291.62  |  BIC = 297.05
  Convergence    = Converged in 6 iterations (Δχ² < 1e-6)
================================================================================
```

### Supported Parametric Models
1. `gaussian` (default): $f(x) = A \cdot \exp\left(-\frac{(x - \mu)^2}{2\sigma^2}\right)$
2. `exponential`: $f(x) = A \cdot \exp(-\lambda x) + C$
3. `polynomial`: $f(x) = \sum_{k=0}^d c_k x^k$ (specify degree with `--degree <d>`)
4. `breit-wigner`: $f(x) = \frac{A}{\pi} \frac{\Gamma / 2}{(x - M)^2 + (\Gamma / 2)^2}$
5. `power-law`: $f(x) = A \cdot (x - x_0)^k$

### Command-Line Arguments for `histo fit`
- `--model, -m <model>`: Model type (`gaussian`, `exponential`, `polynomial`, `breit-wigner`, `power-law`). Default: `gaussian`.
- `--degree, -d <N>`: Degree for polynomial models (1 to 10). Default: 1.
- `--mle`: Use Binned Poisson Maximum Likelihood Estimation (-2 ln L) instead of $\chi^2$ Least Squares.
- `--bounds, -b <idx=min:max>`: Box constraints for parameter index (e.g. `--bounds 2=0.1:10.0`).
- `--fix-param, -f <idx=val>`: Hold parameter constant during optimization.
- `--confidence, -c <level>`: Confidence level for parameter error intervals (default: 0.95).
- `--plot, -p`: Render optional ASCII curve overlay above parameter table.
- `--json, -j`: Emit structured JSON report for programmatic ingestion.
- `--quiet, -q`: Output only optimal parameter values (tab-separated) for shell scripts.
- `--input, -i <file>`: Input histogram file (JSON or Format V1/V2 binary) or raw samples (default: `stdin`).

---

## 4. `histo fill --2d` Specification

### Usage Examples
```bash
# Ingest 2D coordinate pairs from CSV/text and emit JSON
cat telemetry_xy.csv | histo fill --2d > grid.json

# Explicit 100x100 grid with custom column indices
histo fill --2d --xbins 100 --xmin -10 --xmax 10 --ybins 100 --ymin -10 --ymax 10 \
           --xcol 1 --ycol 2 --wcol 3 < simulation.dat > grid.bin
```

### Command-Line Arguments for `histo fill --2d`
- `--2d`: Enable 2D bivariate ingestion mode.
- `--xbins <N>`, `--xmin <val>`, `--xmax <val>`: X axis grid configuration.
- `--ybins <N>`, `--ymin <val>`, `--ymax <val>`: Y axis grid configuration.
- `--delimiter, -d <char>`: Column delimiter (auto-detects whitespace, commas, tabs by default).
- `--xcol <N>`, `--ycol <N>`, `--wcol <N>`: 1-based column indices for $X$, $Y$, and optional weight $W$.
- `--binary, -b`: Emit Format V3 Little-Endian binary blob instead of JSON.

