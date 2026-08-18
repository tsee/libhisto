# CLI Toolkit Extensions: User-Centric & Progressive Disclosure Design

## 1. Design Philosophy: Progressive Disclosure

The `libhisto` CLI toolkit follows the Unix philosophy of small, composable tools that communicate via standard streams (`stdin` / `stdout`). A user should be able to get instant value without reading a manual, while advanced scientific and systems users can access fine-grained controls on demand.

```
       [ Level 1: Zero-Config Ad-Hoc Shell Exploration ]
            cat data.txt | histo plot
            cat data.txt | histo stats
            cat data.txt | histo fit
                         │
                         ▼
       [ Level 2: Structured Scripting & Filtering ]
            histo fill --bins 50 --min 0 --max 100 < data.txt > h.json
            histo plot --sparkline < h.json
            histo fit --model gaussian < h.json
                         │
                         ▼
       [ Level 3: Advanced 2D Geometry & Constrained Optimization ]
            histo fill --2d --xbins 50 --ybins 50 < coords.csv > h2d.bin
            histo plot --2d --heatmap < h2d.bin
            histo fit --model gaussian --mle --fix-param 0=100.0 < h.bin
```

---

## 2. Multi-Call Command Structure

All capabilities are accessible either via the unified multi-call dispatcher (`histo <subcommand>`) or dedicated symlinks (`histo-<subcommand>`):

| Standalone Binary | Sub-command Equivalent | Purpose |
| :--- | :--- | :--- |
| `histo-fill` | `histo fill` | Stream ingestion (1D & 2D), bin accumulation, JSON/binary export |
| `histo-plot` | `histo plot` | Terminal visualization: vertical/horizontal bars, sparklines, 2D ANSI heatmaps |
| `histo-stats` | `histo stats` | Summary statistics, moments, quantiles, robust dispersion, 2D correlations |
| `histo-fit` *(New)* | `histo fit` | Binned parametric curve fitting (Gaussian, Exp, Poly, Breit-Wigner) |
| `histo-cmp` | `histo cmp` | Two-distribution statistical distance metrics ($\chi^2$, KS, Wasserstein, KL) |

---

## 3. Extension 1: `histo-fit` (Binned Curve Fitting)

### User Experience & Learning Curve

#### Step 1: Zero-Config Ingestion & Auto-Model Fit
```bash
# Pipes raw samples directly, auto-bins, auto-detects Gaussian peak, outputs ASCII fitted curve + parameters
cat measurements.txt | histo fit
```

#### Step 2: Selecting Model & JSON Output
```bash
# Fit exponential decay to pre-computed histogram, output structured JSON for CI/scripting
histo fit --model exponential < histogram.json --json
```

#### Step 3: Advanced Optimization & Box Constraints
```bash
# Low-statistics binned Poisson MLE fit with parameter constraints and frozen parameters
histo fit --model gaussian \
          --mle \
          --bounds 2=0.5:5.0 \
          --fix-param 1=0.0 \
          --confidence 0.95 \
          < low_counts.bin
```

### Proposed CLI Options for `histo-fit`

- `--model, -m <type>`: `gaussian` (default), `exponential`, `polynomial`, `breit-wigner`, `power-law`.
- `--degree, -d <N>`: Degree for polynomial fit (default: 1).
- `--mle`: Use Binned Poisson Maximum Likelihood Estimation instead of $\chi^2$ Least Squares.
- `--bounds, -b <idx=min:max>`: Box constraints for parameter index.
- `--fix-param, -f <idx=val>`: Hold parameter index constant at value.
- `--json, -j`: Emit structured JSON fit results (`chi2`, `ndf`, `p_value`, `params`, `param_errors`, `aic`, `bic`).
- `--quiet, -q`: Output only optimal parameter values (tab-separated) for shell scripts.
- `--plot, -p`: Render ASCII overlay of histogram bars and fitted model curve.

---

## 4. Extension 2: 2D Stream Ingestion (`histo-fill --2d`)

### User Experience & Learning Curve

#### Step 1: Ad-Hoc 2D Point Stream Ingestion
```bash
# Ingests space/tab/comma-separated (x, y) coordinates, auto-ranges grid, emits JSON
cat telemetry_xy.csv | histo fill --2d > grid.json
```

#### Step 2: Explicit 2D Grid Specification
```bash
# 100x100 uniform grid with weighted samples
histo fill --2d \
           --xbins 100 --xmin -10.0 --xmax 10.0 \
           --ybins 100 --ymin -10.0 --ymax 10.0 \
           --weights-col 3 \
           < simulation_xyw.dat > grid.bin
```

#### Step 3: 2D Terminal Heatmap Visualization (`histo-plot --2d`)
```bash
# View TrueColor ANSI density grid directly in terminal
histo plot --2d < grid.bin
```

### Proposed CLI Options for 2D Ingestion

- `--2d`: Enable 2D bivariate ingestion mode (expects 2 or 3 columns per row).
- `--xbins <N>`, `--xmin <val>`, `--xmax <val>`: X axis grid configuration.
- `--ybins <N>`, `--ymin <val>`, `--ymax <val>`: Y axis grid configuration.
- `--delimiter, -d <char>`: Column delimiter (auto-detects whitespace, commas, tabs).
- `--xcol <N>`, `--ycol <N>`, `--wcol <N>`: Column index mapping (1-based).

