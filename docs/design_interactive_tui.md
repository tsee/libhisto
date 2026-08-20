# Interactive Terminal Mode (`histo top` / `histo plot -i`): User-Centric Design Specification

This document defines the architectural, visual, and user-experience design for the interactive terminal monitoring and exploration mode (`histo top`) in `libhisto`.

---

## 1. Motivation & User Personas

### Target Personas
1. **Performance Engineer / SRE**:
   - *Use Case*: Piping streaming latencies (`tail -f access.log | awk '{print $NF}' | histo top`) to monitor P95/P99 latency spikes in real time.
   - *Need*: Instant visual feedback on distribution shifts, live percentile badges, and freezing the view without causing upstream pipe stalls.
2. **Data Scientist / Experimentalist**:
   - *Use Case*: Live telemetry from a sensor, Monte Carlo simulation, or particle physics detector.
   - *Need*: Interactive zooming into resonance peaks, flexible logarithmic scaling across coordinates and counts, dynamic rebinning without restarting, and instant curve fitting overlays.
3. **CLI & Terminal Power Users**:
   - *Use Case*: Ad-hoc distribution exploration on large CSV/TSV files with Vim-style keyboard navigation and a command prompt.
   - *Need*: Zero external dependencies (no ncurses), instant startup (<5ms), prompt bar (`:bins 100`, `:range 0 500`), and interactive help (`?`).

---

## 2. Multi-Threaded Architecture & Snapshot Concurrency

To eliminate pipe backpressure stalls and guarantee smooth 60 FPS UI rendering, `histo top` uses a **2-Thread Decoupled Architecture**:

```text
 ┌────────────────────────────────────────────────────────────────────────┐
 │                         INGESTION THREAD                               │
 │                                                                        │
 │   stdin / Stream ──► Fast Ingestion Loop ──► histo_t (Live Accumulator)│
 │   (Unblocked wire    (Reads 64KB blocks,    ▲                          │
 │    speed)             never touches UI)     │ 50ms Snapshot            │
 └─────────────────────────────────────────────┼──────────────────────────┘
                                               │ (1µs Cloned Deep Copy)
 ┌─────────────────────────────────────────────┼──────────────────────────┐
 │                            UI THREAD        │                          │
 │                                             ▼                          │
 │   /dev/tty ──► Keypress / Command ──► histo_t (Display Snapshot)       │
 │   (Instant       - Space (View Freeze)      │                          │
 │    0ms latency)  - ':' (Command Prompt)     ▼                          │
 │                  - '?' (Help Overlay)    Double-Buffered Frame Engine  │
 │                  - Fit / KDE / Log       (60 FPS ANSI Terminal)        │
 └────────────────────────────────────────────────────────────────────────┘
```

### Snapshot Cloning & Zero Display Artifacts
1. **Zero Data Races**: The UI thread never reads the active live histogram while the ingestion thread is modifying it.
2. **Atomic 1µs Snapshot**: Every 33–50 ms, the UI thread acquires a mutex for $< 1\ \mu\text{s}$ to clone `H_live` into `H_display` (or pointer-swap twin buffers).
3. **View Freezing (`Space`)**:
   - Pausing the display freezes `H_display` in place.
   - The ingestion thread continues draining `stdin` into `H_live` in the background, completely preventing pipe buffer overflow ($64\text{ KB}$ Linux limit) and avoiding upstream producer stalls.
   - Pressing `Space` again unfreezes the view, instantly snapping to the current live distribution.

---

## 3. Dynamic Rebinning & Reservoir Sample Cache

### The Challenge of Lossy Rebinning
In classical fixed-bin histograms, merging adjacent bins ($N \to N/2$) is mathematically exact, but splitting bins ($N \to 2N$) is impossible without the original data. Rebinning back and forth without raw samples causes irreversible loss of resolution.

### The Solution: Rolling Reservoir Cache
1. **In-Memory Rolling Cache**: The engine maintains a circular ring buffer of the most recent $K$ raw samples (e.g. $100,000$ samples, occupying $\approx 800\text{ KB}$ of RAM).
2. **Lossless Resizing & Re-ranging**: When the user changes bin count (`:bins 100` or `r`/`R`) or zooms into a sub-range, the histogram is dynamically reconstructed on the fly from the rolling sample cache.
3. **Graceful Degraded Mode**: If the stream exceeds the cache size and raw samples are evicted, the UI clearly indicates:
   - `Bins: 50 (Rebinned from Cache: Lossless)` vs `Bins: 25 (Integer Merged: Exact Sum)`.

---

## 4. Multi-Axis Logarithmic Scaling (1D & 2D)

Distributions often span multiple orders of magnitude across **counts** (frequency) and **coordinates** (values).

### 4.1 1D Logarithmic Modes
- **Log Y (Counts / Height)**: $\log_{10}(\text{count} + 1)$. Essential for exposing faint background tails and rare outliers.
- **Log X (Coordinates / Bins)**: Logarithmically spaced bins $[10^0, 10^1, 10^2, 10^3, \dots]$. Essential for wide dynamic range data (e.g., latencies spanning $1\ \mu\text{s}$ to $10\text{ s}$).
- **Log-Log**: Both axes logarithmic.

### 4.2 2D Logarithmic Modes
- **Log Z (Color / Density)**: Logarithmic mapping for the TrueColor intensity gradient.
- **Log X**: Logarithmic binning along the X coordinate axis.
- **Log Y**: Logarithmic binning along the Y coordinate axis.
- **Log XY / Log XYZ**: Full logarithmic coordinate and density mapping.

### 4.3 Log Scale UX & Cycling
- Pressing **`l`** cycles through the most common logarithmic modes:
  - 1D: `[LIN] ──► [LOG Y (Counts)] ──► [LOG X (Values)] ──► [LOG-LOG] ──► [LIN]`
  - 2D: `[LIN] ──► [LOG Z (Color)] ──► [LOG XY] ──► [LOG XYZ] ──► [LIN]`
- Pressing **`L`** opens the **Axis Scale Selector Modal**:
  ```text
  ┌─ Select Axis Scale ───────────┐
  │ [1] Linear (All axes)         │
  │ [2] Log Y (Counts / Height)   │
  │ [3] Log X (Value Coordinates) │
  │ [4] Log-Log (X and Y)         │
  │ [Esc] Cancel                  │
  └───────────────────────────────┘
  ```
- The active scale is prominently displayed in the status header: `Scale: [X: LIN | Y: LOG]`.

---

## 5. Command Prompt Bar (`:`)

For explicit configuration and precise numeric inputs, pressing **`:`** opens an interactive command prompt at the bottom of the screen:

```text
:[ prompt cursor ]
```

### Supported Commands

| Command | Shorthand | Description & Example |
| :--- | :--- | :--- |
| `:bins <N>` | `:b <N>` | Set exact number of bins (e.g. `:bins 100`, `:b 250`). |
| `:range <MIN> <MAX>` | `:r <MIN> <MAX>` | Set explicit visible coordinate range (e.g. `:range 0 500`). |
| `:min <X>`, `:max <X>` | | Set single boundary (e.g. `:min 10.5`). |
| `:fit <MODEL>` | `:f <MODEL>` | Run parametric fit (`:fit gaussian`, `:fit exp`, `:fit poly 3`, `:fit bw`). |
| `:kde [on|off|<KERNEL>]`| `:k` | Configure KDE overlay (`:kde gaussian`, `:kde epanechnikov`, `:kde off`). |
| `:scale <MODE>` | | Set scale (`:scale logy`, `:scale logx`, `:scale loglog`, `:scale lin`). |
| `:title <STRING>` | | Set custom chart header title. |
| `:export <FILE>` | `:w <FILE>`, `:s`| Export current snapshot (`:export latencies.json`, `:export dump.lhisto`). |
| `:clear` | `:reset` | Flush accumulators and restart ingestion from sample zero. |
| `:help` | `:h`, `?` | Open the interactive help and keybinding cheatsheet modal. |
| `:quit` | `:q` | Cleanly exit interactive mode and restore terminal state. |

### Command Prompt UX Features
- **Tab Completion**: Typing `:b<Tab>` auto-completes to `:bins `.
- **Command History**: `Up` / `Down` arrow keys recall previous commands.
- **Inline Error Feedback**: If an invalid command or parameter is entered (e.g. `:range 100 50`), a clear red error message appears inline (`Error: MIN must be strictly less than MAX`) without crashing or interrupting stream ingestion.
- **Cancel**: Pressing `Esc` or `Ctrl+C` clears the prompt and returns to normal navigation.

---

## 6. Online Help Modal (`?`)

Pressing **`?`** (or typing `:help`) displays an in-terminal interactive help cheatsheet centered over the viewport:

```text
┌─ libhisto top ─ Interactive Help Cheatsheet ──────────────────────────────────────────────┐
│                                                                                           │
│ NAVIGATION & VIEWPORT              DISPLAY & SCALING              ANALYSIS & CURVES       │
│   + / -       Zoom range in / out    l     Cycle Log scale (X/Y)    k   Toggle KDE curve  │
│   h / l, ←/→  Pan view left / right  L     Scale Selector Modal     f   Toggle Curve Fit  │
│   0           Reset to full range    e     Toggle Error bars (±σ)   m   Cycle Moments mode│
│   r / R       Rebin coarser / finer  S     Toggle Sparkline mode    Tab Cycle 1D/2D/CDF   │
│                                                                                           │
│ COMMANDS & SNAPSHOTS               STREAM CONTROL                                         │
│   :           Open Command Prompt    Space Pause / Freeze display (Ingestion continues)   │
│   s           Quick-export JSON      c     Clear / Reset all counters                     │
│   ?           Toggle this Help box   q     Quit and restore terminal                      │
│                                                                                           │
│ Press [?], [Esc], or [Space] to dismiss this help window.                                 │
└───────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 7. Complete Visual Wireframes

### 7.1 Main Live View with Log-Y & KDE Overlay (`histo top`)

```text
┌─ libhisto top ────────────────────────── [LIVE: 142.5 k-ops/s | N=250,000 | W=250,000.0] ───┐
│ Mean: 45.21 ± 8.12 │ Median: 44.95 │ IQR: 10.42 │ P95: 58.80 │ P99: 64.12 │ Mode: 44.80     │
├─────────────────────────────────────────────────────────────────────────────────────────────┤
│ Range: [10.00, 80.00] │ Bins: 35 (Δx=2.00) │ Scale: [X: LIN | Y: LOG10] │ KDE: GAUSSIAN     │
│                                                                                             │
│ [10.0, 12.0)      12 │ █▎                                                                   │
│ [12.0, 14.0)      45 │ ██▏                                                                  │
│ [14.0, 16.0)     128 │ ███                                                                  │
│ [16.0, 18.0)     390 │ ████                                                                 │
│ [18.0, 20.0)    1120 │ █████▎                                                               │
│ [20.0, 22.0)    2890 │ ██████▏                                                              │
│ [22.0, 24.0)    5410 │ ███████                                                              │
│ [24.0, 26.0)    8120 │ ████████  <-- [KDE Peak: 8,150.2]                                    │
│ [26.0, 28.0)    7980 │ ███████▉                                                             │
│ [28.0, 30.0)    5200 │ ███████                                                              │
│ [30.0, 32.0)    2740 │ ██████▏                                                              │
│ [32.0, 34.0)    1050 │ █████▎                                                               │
│ [34.0, 36.0)     380 │ ████                                                                 │
│ [36.0, 38.0)     110 │ ██▉                                                                  │
│ [38.0, 40.0)      32 │ █▉                                                                   │
├─────────────────────────────────────────────────────────────────────────────────────────────┤
│ [Space] Freeze  [l] Log  [k] KDE  [f] Fit  [r/R] Rebin  [+/-] Zoom  [:] Cmd  [?] Help  [q]  │
└─────────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 8. Summary of Keyboard Shortcuts

| Key | Action |
| :--- | :--- |
| **`Space`** | **Pause / Freeze Display** (background ingestion continues without pipe backpressure). |
| **`:`** | **Open Command Prompt** (`:bins 100`, `:range 0 500`, `:fit gaussian`, `:export out.json`). |
| **`?`** | **Open Online Help Modal** with full interactive keybinding cheatsheet. |
| **`l` / `L`** | **Toggle / Select Logarithmic Scale** (Log Y counts, Log X coordinates, Log-Log, Log Color). |
| **`k`** | **Toggle KDE Density Curve Overlay** and Silverman bandwidth estimation. |
| **`f`** | **Toggle Levenberg-Marquardt Curve Fit** with parameter error badges. |
| **`e`** | **Toggle Poisson Uncertainty Error Bars** ($\pm \sqrt{\sum w^2}$). |
| **`r` / `R`** | **Rebin Coarser / Finer** (reconstructed losslessly via rolling reservoir sample cache). |
| **`+` / `-`** | **Zoom Coordinate Range** around the distribution center. |
| **`h` / `l`** (or `←` / `→`) | **Pan Viewport** across distribution coordinates. |
| **`0`** | **Reset Viewport** to global bounds $[x_{\min}, x_{\max}]$. |
| **`c`** | **Clear Accumulators** and restart sample count from zero. |
| **`Tab`** | **Cycle Views** (1D Bars $\to$ Sparklines $\to$ Cumulative CDF $\to$ 2D Heatmap). |
| **`s`** | **Quick Export Snapshot** to JSON or binary `.lhisto`. |
| **`q`** / **`Ctrl+C`** | **Quit** and cleanly restore terminal state. |
