# Real-Time Interactive Distribution Monitor (`histo top`) {#histo_top_manual}

**`histo top`** is a high-performance, real-time interactive terminal monitor for live streaming distributions. Similar to `top` or `htop` for system processes, `histo top` continuously ingests numeric streams from standard input or files, computes online statistical moments, and renders an interactive 60 FPS terminal dashboard with 1D bar charts, continuous KDE overlays, Gaussian curve fits, statistical error bars, and 24-bit TrueColor 2D bivariate heatmaps.

---

## 1. Overview & Key Capabilities

```text
 ┌─────────────────────────── libhisto top ───────────────────────────[LIVE: 12.5k/s | N=45000]┐
 │ Mean: 50.02 ± 9.98 │ Med: 50.01 │ IQR: 13.48 │ P95: 66.45 │ P99: 73.21                     │
 ├─────────────────────────────────────────────────────────────────────────────────────────────┤
 │ Range: [10.00, 90.00] │ Bins: 50 (Δ=1.60) │ Scale: LIN │ Auto: ON │ KDE: Gauss │ Fit: Gauss │
 │ [Count Scale]               0                    1125                  2250                 │
 │                             ├─────────────────────┼──────────────────────┤                  │
 │ [ 40.40,  42.00) │    820 │ ████████████████▋                                ╎±28.6╎        │
 │ [ 42.00,  43.60) │   1105 │ ███████████████████████▍                         ╎±33.2╎        │
 │ [ 43.60,  45.20) │   1450 │ ██████████████████████████████▉                  ╎±38.1╎        │
 │ [ 45.20,  46.80) │   1810 │ █████████████████████████████████████▋           ╎±42.5╎        │
 │ [ 46.80,  48.40) │   2105 │ ███████████████████████████████████████████      ╎±45.9╎        │
 │ [ 48.40,  50.00) │   2245 │ ██████████████████████████████████████████████   ╎±47.4╎        │
 ├─────────────────────────────────────────────────────────────────────────────────────────────┤
 │ [Space] Freeze  [a] Auto  [l] Log  [y] Y-Axis  [k] KDE  [f] Fit  [r/R] Rebin  [:] Cmd  [?]  │
 └─────────────────────────────────────────────────────────────────────────────────────────────┘
```

### Key Highlights
- **Two-Thread Lockless Ingestion Architecture**: Producer pipe reads occur on a dedicated background ingestion thread. Pausing the display or resizing your window never blocks upstream data sources.
- **Micro-Batch Low-Latency Draining**: Ingests samples in 64-item batches with an automatic 50ms time-flush for low-rate streaming sources.
- **Dynamic Reservoir Rebinning & Outlier-Resistant Auto-Ranging**: Outlier resistance calculates $[P_1, P_{99}]$ percentile spans to dynamically adjust histogram bounds without skew from rogue extreme values.
- **Rich 1D Analytical Overlays**: Toggle Kernel Density Estimation (`k`), Gaussian parameter fits (`f`), Poisson/SumW2 error whiskers (`e`), and count axis scale rulers (`y`).
- **Bivariate 2D Heatmap Mode (`--2d`)**: Full 2D interactive grid with 24-bit TrueColor Viridis palettes, Log-Z intensity contrast (`l`), and color range reference bars (`g`).
- **Weighted Stream Ingestion (`-w`)**: Native support for `<value> <weight>` streaming input.

---

## 2. Quickstart & Command-Line Syntax

### 2.1 Invocation Synopsis

```bash
histo top [OPTIONS] [FILE]
# Or using the direct alias:
histo-top [OPTIONS] [FILE]
```

If `FILE` is omitted or `-`, `histo top` reads from `stdin`.

### 2.2 CLI Options Reference Table

| Option | Shorthand | Description | Default |
| :--- | :--- | :--- | :--- |
| `--bins=<N>` | `-n <N>` | Initial number of bins | `50` |
| `--min=<X>` | | Initial lower range boundary | `0.0` |
| `--max=<X>` | | Initial upper range boundary | `100.0` |
| `--2d` | | Enable bivariate 2D heatmap mode (expects `x y` input) | Disabled |
| `--weights` | `-w` | Input stream contains `value weight` (or `x y weight`) | Disabled |
| `--palette=<P>` | `-p <P>` | Color palette: `viridis`, `plasma`, `inferno`, `magma`, `turbo`, `cividis`, `grayscale`, `rainbow` (alias: `--colormap`) | `viridis` |
| `--mono` | `-M` | Monochrome mode (disable ANSI colors, use ASCII ramps) | Auto (color) |
| `--help` | `-h` | Display command-line options | |

### 2.3 Ready-to-Run Quickstart Examples

#### 1D Gaussian Stream with Live Fit & KDE Overlays
```bash
python3 -u -c "import random, time; [print(f'{random.gauss(50, 12):.2f}', flush=True) or time.sleep(0.001) for _ in range(100000)]" | \
  histo top --bins=60 --palette=plasma
```

#### 1D Weighted Stream (`-w`)
```bash
python3 -u -c "import random, time; [print(f'{random.gauss(50, 10):.2f} {random.uniform(0.5, 3.0):.2f}', flush=True) or time.sleep(0.002) for _ in range(50000)]" | \
  histo top -w
```

#### 2D Correlated Bivariate Stream (`--2d`)
```bash
python3 -u -c "import random, time; [print(f'{random.gauss(50, 15):.2f} {random.gauss(50, 10) + 0.3 * (random.gauss(50, 15) - 50):.2f}', flush=True) or time.sleep(0.002) for _ in range(50000)]" | \
  histo top --2d --bins=35 --palette=inferno
```

---

## 3. Interactive Keybindings & Controls

When `histo top` is running, control the display using single-key shortcuts:

### 3.1 Global & Viewport Controls

| Key | Mode | Description |
| :--- | :--- | :--- |
| `Space` | 1D / 2D | **Freeze / Resume**: Freezes live UI snapshot for close inspection while background thread continues draining the stream. |
| `p` / `P` | 1D / 2D | **Cycle Colormaps**: Cycles live between `viridis` → `plasma` → `inferno` → `magma` → `turbo` → `cividis` → `grayscale` → `rainbow`. |
| `?` | 1D / 2D | **Help Modal**: Opens the interactive cheat sheet modal. Press `Esc`, `?`, or `Space` to dismiss. |
| `:` | 1D / 2D | **Command Prompt**: Activates the command bar for direct numerical input (e.g. `:bins 80`). |
| `C` | 1D / 2D | **Monochrome Toggle**: Toggles between 24-bit TrueColor gradients and monochrome ASCII density glyphs. |
| `c` | 1D / 2D | **Clear**: Clears all accumulated samples, resetting bin counts and reservoir history. |
| `q` / `Q` | 1D / 2D | **Quit**: Exits cleanly and restores standard terminal modes. |
| `Ctrl+C` | 1D / 2D | **Interrupt / Quit**: Immediately restores terminal and exits. |

### 3.2 1D Analytical Overlays & Scale Controls

| Key | Mode | Description |
| :--- | :--- | :--- |
| `l` | 1D | **Cycle Log Scales**: Cycles between `Linear` → `Log-Y` (log counts) → `Log-X` (logarithmic bin spacing) → `Log-Log`. |
| `k` | 1D | **KDE Overlay**: Toggles continuous Gaussian Kernel Density Estimation curve (cyan `◆` marks). |
| `f` | 1D | **Gaussian Fit Overlay**: Toggles real-time Levenberg-Marquardt Gaussian fit curve (magenta `✖` marks; `✦` for overlap). |
| `e` | 1D | **Error Bars**: Toggles statistical uncertainty whiskers \f$\pm\sqrt{\sum w^2}\f$ (formatted as `╎±err╎`). |
| `y` / `Y` | 1D | **Count Axis Ruler**: Toggles the top count-scale tick ruler (`0 ... MaxCount`). |
| `a` / `A` | 1D | **Auto-Range**: Toggles dynamic outlier-resistant range detection (\f$P_1\f$ to \f$P_{99}\f$ with 5% margin). |
| `r` | 1D | **Halve Bins**: Halves bin count and re-aggregates from reservoir cache. |
| `R` | 1D | **Double Bins**: Doubles bin count and re-aggregates from reservoir cache. |

### 3.3 2D Heatmap Controls (`--2d`)

| Key | Mode | Description |
| :--- | :--- | :--- |
| `l` | 2D | **Log-Z Intensity**: Toggles logarithmic color intensity mapping (\f$\log_{10}(c + 1)\f$) to bring out low-count background details. |
| `g` / `G` | 2D | **Color Legend**: Toggles the TrueColor spectrum reference bar showing exact numeric bounds and active palette name. |

---

## 4. Command Prompt (`:`) Subcommands

Pressing **`:`** opens the command bar at the bottom of the screen.

```text
:palette plasma█
```

| Command | Shorthand | Arguments | Example | Description |
| :--- | :--- | :--- | :--- | :--- |
| `:palette <P>` | `:p <P>` | Palette name (`viridis`, `plasma`, `inferno`, `magma`, `turbo`, `cividis`, `grayscale`, `rainbow`) | `:palette plasma` | Changes the active colormap. Omit argument to cycle. |
| `:bins <N>` | `:b <N>` | Number of bins (\f$1 \le N \le 100000\f$) | `:bins 100` | Rebuilds 1D histogram with $N$ bins from reservoir. |
| `:range <A> <B>` | `:r <A> <B>` | Min and max bounds (\f$A < B\f$) | `:range 20 80` | Zooms viewport into $[A, B]$ and re-aggregates reservoir. |
| `:scale <MODE>` | `:sc <MODE>` | `linear`, `logy`, `logx`, `loglog` | `:scale logy` | Sets 1D axis scaling mode. |
| `:autorange [ARG]`| `:a [ARG]` | `on`, `off`, or threshold (\f$0 < T < 1\f$) | `:autorange 0.05` | Enables/disables auto-ranging with threshold $T$. |
| `:clear` | | None | `:clear` | Clears all histogram bins and sample cache. |
| `:help` | `:h` | None | `:help` | Opens help cheat sheet modal. |
| `:quit` | `:q` | None | `:quit` | Exits `histo top`. |

Press **`Enter`** to execute a command or **`Esc`** to cancel.

---

## 5. In-Depth Feature Walkthrough

### 5.1 Real-Time Kernel Density Estimation (KDE) Overlay (`k`)
When **`k`** is pressed and at least 5 samples have been ingested:
- A continuous Gaussian KDE is computed over the active range using Silverman's rule-of-thumb bandwidth \f$h = 1.06 \cdot \hat{\sigma} \cdot n^{-1/5}\f$.
- The continuous density \f$f(x)\f$ is converted to expected bin counts \f$C_i = f(x_i) \cdot W_{\text{tot}} \cdot \Delta x\f$.
- Overlay markers appear in the terminal:
  - **Cyan `◆`**: KDE expected density point.
  - **Yellow `✦`**: Point where both KDE and Gaussian Fit intersect the same column.
- Subheader displays active bandwidth: `│ KDE: Gauss(h=0.85)`.

### 5.2 Real-Time Gaussian Fit Overlay (`f`)
When **`f`** is pressed and at least 5 samples have been ingested:
- `libhisto` runs a Levenberg-Marquardt non-linear regression fitting \f$f(x) = A \cdot \exp\left(-\frac{(x-\mu)^2}{2\sigma^2}\right)\f$.
- When converged, the predicted curve is rendered across bin rows:
  - **Magenta `✖`**: Gaussian fit prediction point.
  - **Yellow `✦`**: Overlap with KDE density marker.
- Subheader displays fitted parameters: `│ Fit: μ=50.02 σ=9.98`.

### 5.3 Statistical Error Whiskers (`e`)
When **`e`** is pressed:
- Statistical error for each bin is calculated via \f$\sigma_i = \sqrt{\sum w^2}\f$ (Poisson uncertainty for unweighted data).
- The uncertainty is formatted and appended to each row:
  - TrueColor mode: `╎±14.2╎` in muted gray ANSI.
  - Monochrome mode: `+-14.2`.
- Subheader indicates `│ Err: ON`.

### 5.4 Count Axis Ruler (`y`)
When **`y`** is pressed:
- A dual-row scale ruler is inserted directly above the histogram bar rows:
  ```text
  [Count Scale]               0                    750                   1500
                              ├─────────────────────┼──────────────────────┤
  ```
- Scale numbers are dynamically right-aligned and scaled (0, 50%, 100%) matching linear or logarithmic modes without terminal truncation.
- Subheader indicates `│ Axis: ON`.

### 5.5 2D Bivariate Heatmap Mode (`--2d`)
When started with `--2d`:
- Expects pairs of coordinates (`x y`) per line.
- Renders a 2D intensity grid where colors represent sample density:
  - **Viridis TrueColor Palette**: Smooth perceptual colormap transitioning from dark purple (0%) → teal → green → bright yellow (100%).
  - **Color Range Legend Bar (`g`)**: Displays complete color scale alongside minimum and maximum intensity values.
  - **Log-Z Intensity (`l`)**: Normalizes bin colors using \f$\log_{10}(c + 1)\f$, making faint structures and background tails visible alongside dense clusters.

---

## 6. Real-World Production Recipes

### 6.1 Monitoring Web Server Request Latencies
Pipe latency metrics directly from production access logs into `histo top`:

```bash
# Ingest NGINX / Envoy request durations in milliseconds
tail -F /var/log/nginx/access.log | \
  awk '{print $NF * 1000}' | \
  histo top --bins=50 --min=0 --max=500
```
> *Tip*: Press `l` to switch to **Log-Y** mode to easily monitor rare P99/P99.9 latency spikes.

### 6.2 Database Query Execution Times
```bash
# Monitor PostgreSQL slow query durations
pg_log_stream | awk '/duration:/ {print $NF}' | histo top -n 40
```

### 6.3 2D Coordinate Telemetry (Sensor / Simulation)
```bash
# Monitor bivariate position coordinates from a robotic arm or physics particle sim
simulation_engine --emit-coords | histo top --2d --bins=40
```

---

## 7. Troubleshooting & FAQ

**Q: Output appears uncolored or plain text.**  
*A:* Your terminal might not advertise 24-bit TrueColor support or `NO_COLOR` is set. Use modern terminals (Ghostty, Alacritty, iTerm2, WezTerm, VS Code Terminal, Windows Terminal). Press `C` to switch to ASCII density character mode.

**Q: Upstream process is producing data slowly.**  
*A:* `histo top` includes a 50ms time-flush so low-rate streams (e.g. 5 samples/sec) update smoothly without waiting for full 64-item batches. Ensure upstream producers flush standard output (`python3 -u` or `sys.stdout.flush()`).

**Q: Freezing display (`Space`)—will my upstream data be lost?**  
*A:* No. Ingestion occurs on a separate thread with a dedicated mutex. The input pipe is continuously drained even when the TUI rendering is frozen.
