# Interactive Terminal Mode (`histo top` / `histo plot -i`): User-Centric Design Specification

This document defines the architectural, visual, and user-experience design for the interactive terminal monitoring mode in `libhisto`.

---

## 1. Motivation & User Personas

### Target Personas
1. **Performance Engineer / SRE**:
   - *Use Case*: Piping streaming latencies (`tail -f access.log | awk '{print $NF}' | histo top`) to monitor P95/P99 latency spikes in real time.
   - *Need*: Instant visual feedback on distribution shape shifts, live percentile badges, and the ability to freeze/pause the frame when an anomaly occurs.
2. **Data Scientist / Experimentalist**:
   - *Use Case*: Live telemetry from a sensor, Monte Carlo simulation, or particle detector.
   - *Need*: Interactive zooming into resonance peaks, toggling between linear and log scales, dynamic rebinning without restarting the process, and instant Gaussian/KDE curve overlays.
3. **Command-Line Enthusiast**:
   - *Use Case*: Ad-hoc distribution exploration on large CSV files.
   - *Need*: Zero-configuration, zero-dependency TUI (no ncurses requirement), lightning-fast startup (<5ms), and intuitive Vim-style keybindings.

---

## 2. CLI Invocation & Pipeline Ergonomics

The interactive mode is accessible via the multi-call alias `histo top` or flag `histo plot -i` / `histo plot --interactive`:

```bash
# Live stream monitoring from a pipe (default 10 Hz refresh or event-driven)
tail -f request_latencies.log | awk '{print $9}' | histo top

# Interactive exploration of an existing file
histo top data.csv

# 2D Bivariate interactive heatmap monitoring
tail -f coordinates.tsv | histo top --2d

# Interactive exploration with specific initial options
histo top --log --kde --bins=50 benchmark_results.dat
```

---

## 3. Terminal Screen Layout & Wireframes

The TUI maintains a 3-region responsive layout:
1. **Header Bar**: Stream status, ingestion rate (ops/s), sample count, and summary moments.
2. **Main Viewport**: High-resolution Unicode chart (1D histogram, KDE curve overlay, CDF, or 2D TrueColor heatmap).
3. **Footer & Status Line**: Interactive keybind hints, zoom window coordinates, and status notifications.

### 3.1 Wireframe: 1D Live Histogram with KDE Overlay (`histo top`)

```text
┌─ libhisto top ────────────────────────── [LIVE: 142.5 k-ops/s | N=250,000 | W=250,000.0] ───┐
│ Mean: 45.21 ± 8.12 │ Median: 44.95 │ IQR: 10.42 │ P95: 58.80 │ P99: 64.12 │ Mode: 44.80 (peak)│
├─────────────────────────────────────────────────────────────────────────────────────────────┤
│ Range: [10.00, 80.00] │ Bins: 35 (Δx=2.00) │ Scale: LINEAR │ KDE: GAUSSIAN (h=1.42)         │
│                                                                                             │
│ [10.0, 12.0)      12 │ ▏                                                                    │
│ [12.0, 14.0)      45 │ ▍                                                                    │
│ [14.0, 16.0)     128 │ █▏                                                                   │
│ [16.0, 18.0)     390 │ ███                                                                  │
│ [18.0, 20.0)    1120 │ █████████                                                            │
│ [20.0, 22.0)    2890 │ ████████████████████▎                                                │
│ [22.0, 24.0)    5410 │ ███████████████████████████████████▌                                 │
│ [24.0, 26.0)    8120 │ █████████████████████████████████████████████████████▌  <-- [KDE Peak]│
│ [26.0, 28.0)    7980 │ ████████████████████████████████████████████████████▍                │
│ [28.0, 30.0)    5200 │ ████████████████████████████████████                                 │
│ [30.0, 32.0)    2740 │ ███████████████████                                                  │
│ [32.0, 34.0)    1050 │ ███████▍                                                             │
│ [34.0, 36.0)     380 │ ██▋                                                                  │
│ [36.0, 38.0)     110 │ ▉                                                                    │
│ [38.0, 40.0)      32 │ ▎                                                                    │
├─────────────────────────────────────────────────────────────────────────────────────────────┤
│ [Space] Pause  [l] Log  [k] KDE  [f] Fit  [r/R] Rebin  [+/-] Zoom  [h/l] Pan  [s] Save  [q]│
└─────────────────────────────────────────────────────────────────────────────────────────────┘
```

### 3.2 Wireframe: Live Fitted Peak View (`f` key active)

```text
┌─ libhisto top [FIT MODE] ─────────────── [PAUSED | Model: GAUSSIAN | Loss: CHI2] ───────────┐
│ Amplitude: 8150.2 ± 32.1 │ Mean (μ): 25.12 ± 0.04 │ Sigma (σ): 4.15 ± 0.03 │ χ²/NDF: 1.04   │
├─────────────────────────────────────────────────────────────────────────────────────────────┤
│ Visual Fit Curve Overlay:  [█ = Bin Data | · = Gaussian Fit Function]                       │
│                                                                                             │
│ [20.0, 22.0)    2890 │ ████████████████████▎   ·                                            │
│ [22.0, 24.0)    5410 │ ███████████████████████████████████▌        ·                        │
│ [24.0, 26.0)    8120 │ █████████████████████████████████████████████████████▌·              │
│ [26.0, 28.0)    7980 │ ████████████████████████████████████████████████████▍·               │
│ [28.0, 30.0)    5200 │ ████████████████████████████████████        ·                        │
├─────────────────────────────────────────────────────────────────────────────────────────────┤
│ Fit converged in 6 iterations. p-value: 0.4120. Press [f] to toggle fit overlay.             │
└─────────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Interactive Keybindings & Controls

All keybindings use single keystrokes without requiring `Enter`:

| Keybinding | Action | User Experience & Feedback |
| :--- | :--- | :--- |
| **`Space`** | **Pause / Resume** | Freezes incoming stream ingestion into the display; buffers incoming data in the background so no samples are dropped. |
| **`l`** | **Toggle Log Scale** | Toggles bar heights between linear ($y$) and logarithmic ($\log_{10}(y + 1)$) scales to expose faint background tails. |
| **`k`** | **Toggle KDE Overlay** | Computes and displays continuous density estimation points and bandwidth ($h$) alongside discrete bins. |
| **`f`** | **Toggle Curve Fit** | Fits built-in model (Gaussian, Exponential, Polynomial) and displays parameter table + overlay curve. |
| **`e`** | **Toggle Error Bars** | Toggles $\sqrt{\sum w^2}$ Poisson uncertainty bars on/off. |
| **`r` / `R`** | **Rebin Coarser / Finer** | `r` merges adjacent bins ($N/2$); `R` splits bins ($2N$) when raw sample cache is available. |
| **`+` / `-`** (or `z` / `Z`) | **Zoom Range** | Expands or contracts the visible range $[\min, \max]$ around the median. |
| **`h` / `l`** (or `←` / `→`) | **Pan Range** | Shifts the visible viewport left or right along the X-axis. |
| **`0`** | **Reset View** | Resets zoom and pan to the global bounds $[x_{\min}, x_{\max}]$. |
| **`c`** | **Clear / Reset** | Flushes accumulator counters and restarts ingestion from sample zero. |
| **`Tab`** | **Cycle View Mode** | Cycles between **1D Bar Plot**, **Sparkline Strip**, **Cumulative CDF**, and **2D Heatmap**. |
| **`s`** | **Save Snapshot** | Prompts for filename (e.g. `snapshot.json` or `snapshot.lhisto`) and exports binary/JSON wire format. |
| **`?`** | **Help Modal** | Pops up an in-terminal overlay window with full documentation and keybind reference. |
| **`q` / `Ctrl+C`** | **Quit** | Cleanly restores terminal cursor, disables raw mode, and exits. |

---

## 5. Technical Architecture & Engineering Standards

### 5.1 Zero-Dependency Pure C99 Terminal Driver
To ensure portability across Linux, macOS, BSD, and Windows WSL without external link dependencies:
- **Raw Mode**: Uses POSIX `termios` (`tcgetattr`, `tcsetattr` with `ECHO` and `ICANON` disabled).
- **ANSI / VT100 Sequences**:
  - Alternate screen buffer: `\033[?1049h` (enter) / `\033[?1049l` (exit).
  - Cursor hiding: `\033[?25l` (hide) / `\033[?25h` (show).
  - Cursor positioning: `\033[H` (home) / `\033[K` (clear line).
- **No ncurses dependency**: Pure standard C99, compiling into a tiny standalone binary with instant startup.

### 5.2 Non-Blocking Event Loop & Stream Multiplexing
The interactive engine multiplexes terminal user input (`stdin` on `/dev/tty` when piping) and data stream input:
```c
// Event loop pseudo-architecture
int tty_fd = open("/dev/tty", O_RDONLY | O_NONBLOCK);
int data_fd = fileno(stdin); // When piped, data comes from stdin

while (running) {
    struct pollfd fds[2] = {
        { .fd = tty_fd, .events = POLLIN },
        { .fd = data_fd, .events = POLLIN }
    };
    int ret = poll(fds, 2, 50); // 50ms tick = 20 FPS UI refresh

    if (fds[0].revents & POLLIN) {
        handle_keyboard_input(tty_fd);
    }
    if (fds[1].revents & POLLIN) {
        ingest_streaming_batch(data_fd);
    }
    if (needs_redraw || time_for_frame()) {
        render_double_buffered_frame();
    }
}
```

### 5.3 Double-Buffering for Zero Flicker
- Screen output is rendered into an in-memory string buffer (`char frame_buf[65536]`).
- The entire buffer is flushed to the terminal in a single `write()` call, eliminating tearing and terminal cursor flicker.

### 5.4 Live Terminal Resize Handling (`SIGWINCH`)
- Listens for `SIGWINCH` signals.
- Automatically queries terminal columns and rows (`ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)`) and dynamically resizes bin bar lengths, padding, and layout without dropping stream state.

---

## 6. Implementation Roadmap

1. **Phase 1: Terminal Control Core (`tools/src/tui_core.c`)**:
   - Terminal raw mode enter/exit with `atexit` cleanup guards.
   - Non-blocking keyboard input parser (arrow keys, escape sequences, single keys).
   - Double-buffered ANSI renderer.
2. **Phase 2: Live Ingestion Engine (`tools/src/tui_engine.c`)**:
   - Multiplexed `poll()` loop separating data stream and `/dev/tty` user input.
   - Ring buffer sample cache for dynamic rebinning and zooming.
3. **Phase 3: Interactive Visualizations (`tools/src/cmd_top.c`)**:
   - 1D histogram bar view, Sparkline strip view, KDE density curve overlay, and Levenberg-Marquardt fit inspector.
4. **Phase 4: CLI Wiring & Manpages**:
   - Register `histo top` subcommand in `tools/src/main.c`.
   - Update `docs/manual.md` Level 1 CLI documentation.
