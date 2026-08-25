# Real-Time Interactive Distribution Monitor (`histo top`) {#histo_top_manual}

**`histo top`** is a high-performance, real-time interactive terminal monitor for live streaming distributions. Similar to `top` or `htop` for system processes, `histo top` continuously ingests numeric streams from standard input, Unix pipes, log streams, or telemetry files, computes online statistical moments, and renders an interactive 60 FPS terminal dashboard with 1D bar charts, continuous KDE overlays, Gaussian curve fits, statistical error bars, and 24-bit TrueColor 2D bivariate heatmaps.

---

## 1. Overview & Key Capabilities

```text
 ┌─ libhisto top ─────────────────────────────────────────── [LIVE: 12.5k/s | N=45000] ─┐
 │ Mean: 50.02 ± 9.98 │ Med: 50.01 │ IQR: 13.48 │ P95: 66.45 │ P99: 73.21               │
 ├───────────────────────────────────────────────────────────────────────────────────────┤
 │ Range: [10.00, 90.00] │ Bins: 50 │ Scale: LIN │ Pal: viridis │ Auto: ON │ KDE: Gauss  │
 │ [Count Scale]               0                    1125                  2250           │
 │                             ├─────────────────────┼──────────────────────┤            │
 │ [ 40.40,  42.00) │    820 │ ████████████████▋                                ╎±28.6╎  │
 │ [ 42.00,  43.60) │   1105 │ ███████████████████████▍                         ╎±33.2╎  │
 │ [ 43.60,  45.20) │   1450 │ ██████████████████████████████▉                  ╎±38.1╎  │
 │ [ 45.20,  46.80) │   1810 │ █████████████████████████████████████▋           ╎±42.5╎  │
 │ [ 46.80,  48.40) │   2105 │ ███████████████████████████████████████████      ╎±45.9╎  │
 │ [ 48.40,  50.00) │   2245 │ ██████████████████████████████████████████████   ╎±47.4╎  │
 ├───────────────────────────────────────────────────────────────────────────────────────┤
 │ [Space] Freeze  [+/-] Zoom  [Tab] Col  [w] Win  [s] Save  [H] Compact  [:] Cmd  [?]   │
 └───────────────────────────────────────────────────────────────────────────────────────┘
```

### Key Highlights
- **Two-Thread Lockless Ingestion Architecture**: Producer pipe reads occur on a dedicated background ingestion thread. Pausing the display or resizing your window never blocks upstream data sources.
- **Micro-Batch Low-Latency Draining**: Ingests samples in 64-item batches with an automatic 50ms time-flush for low-rate streaming sources.
- **Dynamic Reservoir Rebinning & Outlier-Resistant Auto-Ranging**: Outlier resistance calculates [P1, P99] percentile spans to dynamically adjust histogram bounds without skew from rogue extreme values.
- **Rich 1D Analytical Overlays**: Toggle Kernel Density Estimation (`k`), Gaussian parameter fits (`f`), Poisson/SumW2 error whiskers (`e`), and count axis scale rulers (`y`).
- **Bivariate 2D Heatmap Mode (`--2d`)**: Full 2D interactive grid with 24-bit TrueColor Viridis palettes, Log-Z intensity contrast (`l`), and color range reference bars (`g`).
- **Multi-Column Live Switching (`Tab`, `1`..`9`, `:col`)**: Ingest multi-column TSV/CSV logs and switch active analysis metric instantaneously without restarting the process.
- **Rolling Window & Exponential Decay (`w`, `:window`, `:decay`)**: Restrict live computation to the most recent N samples or apply continuous exponential decay fading.
- **SGR Mouse Interaction**: Zoom dynamically with the mouse wheel and click directly on histogram rows to inspect exact bin boundaries, counts, percentages, and errors.
- **Instant Snapshots & Exports (`s`, `:save`, `:export`)**: Export live or paused state directly into deterministic binary (`.histo`) or JSON (`.json`) files.

---

## 2. Quickstart & Command-Line Syntax

### 2.1 Invocation Synopsis

```bash
histo top [OPTIONS] [FILE]
# Or using the direct tool binary alias:
histo-top [OPTIONS] [FILE]
```

If `FILE` is omitted or `-`, `histo top` reads from `stdin`.

### 2.2 CLI Options Reference Table

| Option | Shorthand | Description | Default |
| :--- | :--- | :--- | :--- |
| `--bins=<N>` | `-n <N>` | Initial number of bins (1D) or X/Y bins (2D) | `50` |
| `--min=<X>` | | Initial lower range boundary (1D) | `0.0` |
| `--max=<X>` | | Initial upper range boundary (1D) | `100.0` |
| `--auto-range` | `-a` | Enable dynamic quantile auto-ranging | Enabled (`ON`) |
| `--no-auto-range`| | Disable dynamic quantile auto-ranging | Disabled |
| `--2d` | | Enable bivariate 2D heatmap mode (expects `x y` input) | Disabled (1D) |
| `--xbins=<N>` | | Number of bins along X axis (2D mode) | `50` |
| `--xmin=<X>`, `--xmax=<X>` | | Initial X axis bounds (2D mode) | `[0.0, 100.0]` |
| `--ybins=<N>` | | Number of bins along Y axis (2D mode) | `50` |
| `--ymin=<Y>`, `--ymax=<Y>` | | Initial Y axis bounds (2D mode) | `[0.0, 100.0]` |
| `--weights` | `-w` | Input stream contains `value weight` (or `x y weight`) | Disabled |
| `--value-col=<COL>` | `--val-col=<COL>` | 1-based column for sample coordinate in 1D mode | `1` |
| `--xcol=<COL>` | | 1-based column for X coordinate in 2D mode | `1` |
| `--ycol=<COL>` | | 1-based column for Y coordinate in 2D mode | `2` |
| `--weights-col=<COL>` | | 1-based column for sample weight | `2` (1D) / `3` (2D) |
| `--delimiter=<CHAR>` | `-d <CHAR>` | Field delimiter character (e.g. `\t`, `,`, ` `) | Whitespace / auto |
| `--palette=<P>` | `-p <P>` | Color palette: `viridis`, `plasma`, `inferno`, `magma`, `turbo`, `cividis`, `grayscale`, `rainbow` (alias: `--colormap`) | `viridis` |
| `--mono` | `-M` | Monochrome mode (disable ANSI colors, use ASCII ramps) | Auto (color) |
| `--help` | `-h` | Display command-line options and exit | |

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

## 3. Interactive Keybindings Reference

When `histo top` is running, control the display using single-key shortcuts:

### 3.1 Navigation, Zoom & Pan

| Key | Mode | Description |
| :--- | :--- | :--- |
| `+` / `=` / `z` / `PgUp` | 1D / 2D | **Zoom In (1.25x)**: Shrinks the visible range window by 20% around the center. |
| `-` / `_` / `Z` / `PgDn` | 1D / 2D | **Zoom Out (0.8x)**: Expands the visible range window by 25% around the center. |
| `←` / `h` / `[` / `<` | 1D / 2D | **Pan Left (-10%)**: Shifts the visible range window 10% to the left. |
| `→` / `l` (in nav) / `]` / `>` | 1D / 2D | **Pan Right (+10%)**: Shifts the visible range window 10% to the right. |
| `↑` | 2D / 1D | **Pan Up (+10%)** in 2D mode / **Zoom In** in 1D mode. |
| `↓` | 2D / 1D | **Pan Down (-10%)** in 2D mode / **Zoom Out** in 1D mode. |
| `0` / `Home` | 1D / 2D | **Reset View**: Re-enables dynamic auto-ranging and resets zoom/pan viewport. |

### 3.2 Display & Analytical Overlays

| Key | Mode | Description |
| :--- | :--- | :--- |
| `l` | 1D | **Cycle Log Scales**: Cycles between `Linear` → `Log-Y` (log counts) → `Log-X` (logarithmic bin spacing) → `Log-Log`. |
| `l` | 2D | **Log-Z Intensity**: Toggles logarithmic color intensity mapping (log10(c + 1)) to highlight faint background details. |
| `k` | 1D | **KDE Overlay**: Toggles continuous Gaussian Kernel Density Estimation curve (cyan `◆` marks). |
| `f` | 1D | **Gaussian Fit Overlay**: Toggles real-time Levenberg-Marquardt Gaussian fit curve (magenta `✖` marks; yellow `✦` for overlap). |
| `e` | 1D | **Error Bars**: Toggles statistical uncertainty whiskers ±sqrt(sum w^2) (formatted as `╎±err╎`). |
| `y` / `Y` | 1D | **Count Axis Ruler**: Toggles the top count-scale tick ruler (`0 ... MaxCount`). |
| `g` / `G` | 2D | **Color Legend**: Toggles the TrueColor spectrum reference bar showing exact numeric bounds and active palette name. |
| `p` / `P` | 1D / 2D | **Cycle Colormaps**: Cycles live between `viridis` → `plasma` → `inferno` → `magma` → `turbo` → `cividis` → `grayscale` → `rainbow`. |
| `C` | 1D / 2D | **Monochrome Toggle**: Toggles between 24-bit TrueColor gradients and monochrome ASCII density glyphs (` .:-=+*#%@`). |
| `H` | 1D / 2D | **Compact Header**: Toggles between detailed 3-row stats header and compact 1-row header for smaller terminals. |
| `B` | 1D | **Auto-Fit Bins**: Toggles auto-fitting the bin count to the exact available terminal row height. |
| `a` / `A` | 1D | **Auto-Range**: Toggles dynamic outlier-resistant range detection (P1 to P99 with 5% margin). |
| `r` | 1D | **Halve Bins**: Halves bin count and re-aggregates from reservoir cache. |
| `R` | 1D | **Double Bins**: Doubles bin count and re-aggregates from reservoir cache. |

### 3.3 Columns, Windowing & Persistence

| Key | Mode | Description |
| :--- | :--- | :--- |
| `Tab` / `Shift+Tab` | 1D | **Cycle Active Column**: Cycles forward/backward through available data columns (1 to 16). |
| `1` .. `9` | 1D | **Select Column**: Directly switches to data column 1 through 9. |
| `w` | 1D / 2D | **Cycle Rolling Window**: Cycles rolling sample buffer: `ALL` → `1,000` → `5,000` → `10,000` → `50,000`. |
| `Space` | 1D / 2D | **Freeze / Resume**: Freezes live UI snapshot for close inspection while background thread continues draining the stream. |
| `s` | 1D / 2D | **Save Snapshot**: Exports instantaneous histogram snapshot to `./histo_snapshot_<timestamp>.histo`. |
| `c` | 1D / 2D | **Clear**: Clears all accumulated samples, resetting bin counts and reservoir history. |
| `?` | 1D / 2D | **Help Modal**: Opens the interactive cheat sheet modal. Press `Esc`, `?`, or `Space` to dismiss. |
| `:` | 1D / 2D | **Command Prompt**: Activates the command bar for direct numerical input (e.g. `:bins 80`). |
| `q` / `Q` / `Ctrl+C` | 1D / 2D | **Quit**: Exits cleanly and restores standard terminal modes. |

---

## 4. Interactive Command Prompt (`:`) Reference

Pressing **`:`** opens the interactive command bar at the bottom of the screen.

```text
:palette plasma█
```

| Command | Shorthand | Arguments | Example | Description |
| :--- | :--- | :--- | :--- | :--- |
| `:bins <N\|auto>` | `:b <N\|auto>` | Number of bins (1 <= N <= 100000) or `auto` | `:bins 100` | Rebuilds 1D histogram with N bins (or auto-fits to rows) from reservoir. |
| `:range <A> <B>` | `:r <A> <B>` | Min and max bounds (A < B) | `:range 20 80` | Zooms viewport into [A, B] and re-aggregates reservoir. |
| `:col <N>` | `:c <N>` | Column index (1 <= N <= 16) | `:col 3` | Switches active 1D analysis metric to column N. |
| `:xcol <N>` | | Column index (1 <= N <= 16) | `:xcol 2` | Switches 2D X-coordinate metric to column N. |
| `:ycol <N>` | | Column index (1 <= N <= 16) | `:ycol 4` | Switches 2D Y-coordinate metric to column N. |
| `:window <N>` | `:w <N>` | Window sample size (0 for all) | `:window 5000` | Restricts computation to the most recent N samples. |
| `:decay <lambda>` | | Decay rate lambda >= 0 (per second) | `:decay 0.1` | Applies exponential decay factor exp(-lambda * dt) to older samples. |
| `:save [path]` | `:s [path]` | Optional output file path | `:save /tmp/snap.histo` | Exports binary (`.histo`) or JSON (`.json`) snapshot. |
| `:export [path]` | | Optional output file path | `:export /tmp/snap.json` | Exports snapshot (inferred format by file extension). |
| `:palette <P>` | `:p <P>` | Palette name (`viridis`, `plasma`, etc.) | `:palette plasma` | Changes the active colormap. Omit argument to cycle. |
| `:scale <MODE>` | `:sc <MODE>` | `linear`, `logy`, `logx`, `loglog` | `:scale logy` | Sets 1D axis scaling mode. |
| `:autorange [ARG]`| `:a [ARG]` | `on`, `off`, or threshold (0 < T < 1) | `:autorange 0.05` | Enables/disables auto-ranging with quantile margin T. |
| `:zoom <factor>` | `:z <factor>` | Magnification factor (> 0) | `:zoom 2.0` | Zooms into current center by factor. |
| `:pan <dx> [dy]` | | Fractional pan offsets | `:pan 0.2` | Pans visible viewport by fractional offset. |
| `:compact` | `:H` | None | `:compact` | Toggles compact single-row header mode. |
| `:clear` | `:reset` | None | `:clear` | Clears all histogram bins and sample cache. |
| `:help` | `:h` | None | `:help` | Opens help cheat sheet modal. |
| `:quit` | `:q` | None | `:quit` | Exits `histo top`. |

Press **`Enter`** to execute a command or **`Esc`** to cancel.

---

## 5. Mouse Interaction (SGR Mouse Mode)

`histo top` automatically enables SGR extended mouse tracking on supported terminals:

- **Mouse Wheel Scroll Up**: Zooms in by 1.25x (visible range contracts by 20% around viewport midpoint).
- **Mouse Wheel Scroll Down**: Zooms out by 0.8x (visible range expands by 25% around viewport midpoint).
- **Left Mouse Click on Histogram Row (1D)**: Inspects the clicked bin row directly. The status bar immediately displays the exact numerical interval, sample count, percentage of total weight, and Poisson/SumW2 uncertainty:
  ```text
  >> Bin 14 [45.20, 46.80): Count=1810.00 (14.2%) ±42.54
  ```

---

## 6. Real-World Production Recipes

### 6.1 Linux Kernel Interrupt Distribution (`/proc/interrupts`)
Monitor hardware interrupt rates across CPU cores in real time:

```bash
# Ingest context switch interrupt counts per second
while true; do
  awk 'NR>1 {for(i=2;i<=NF-3;i++) print $i}' /proc/interrupts
  sleep 1
done | histo top --bins=40 --palette=turbo
```

### 6.2 System Context Switches & Block I/O (`vmstat`)
Monitor distribution of context switches per second:

```bash
# Ingest context switch column (col 12) from vmstat
vmstat 1 | awk 'NR>2 {print $12}' | histo top --bins=50 --palette=plasma
```

### 6.3 Multi-Core CPU Utilization (`mpstat`)
Inspect the distribution of `%idle` across all CPU cores:

```bash
# Ingest %idle column (col 12) from mpstat
mpstat -P ALL 1 | awk 'NR>3 && $3 ~ /^[0-9]+$/ {print $NF}' | histo top --bins=40 --min=0 --max=100
```

### 6.4 Network Packet Size Distribution (`tcpdump`)
Monitor packet length distribution on a live network interface:

```bash
# Ingest IP packet length in bytes
sudo tcpdump -l -n -e -i eth0 2>/dev/null | \
  awk '{print $NF}' | grep -E '^[0-9]+$' | \
  histo top --bins=60 --min=40 --max=1500
```

### 6.5 Web Server Latency Stream (NGINX / Envoy / Apache)
Pipe latency metrics directly from production access logs:

```bash
# Ingest NGINX / Envoy request durations in milliseconds
tail -F /var/log/nginx/access.log | \
  awk '{print $NF * 1000}' | \
  histo top --bins=50 --min=0 --max=500
```
> *Tip*: Press `l` to switch to **Log-Y** mode to easily monitor rare P99/P99.9 latency spikes.

### 6.6 Multi-Column Log Ingestion (Switching with `Tab` or `1`..`9`)
Stream multi-column CSV/TSV data and switch metrics on-the-fly:

```bash
# Stream: <latency_ms> <bytes_sent> <cpu_pct>
tail -F /var/log/app/metrics.tsv | histo top --val-col=1
# Press '1' to inspect Latency, '2' for Bytes, '3' for CPU!
```

### 6.7 Linux Kernel VFS Latencies (eBPF & `bpftrace`)
Inspect filesystem `read()` latencies in real time directly from the kernel:

```bash
# Ingest VFS read latency in microseconds (us)
sudo bpftrace -e '
kprobe:vfs_read { @start[tid] = nsecs; }
kretprobe:vfs_read /@start[tid]/ {
    $lat_us = (nsecs - @start[tid]) / 1000;
    printf("%d\n", $lat_us);
    delete(@start[tid]);
}' | histo top --bins 50 --autorange --window 100000
```
- **What it does**: Traces every Linux `vfs_read()` system call entry and return, measures exact elapsed duration in microseconds ($\mu\text{s}$), and streams latencies to `histo top`.
- **Why it is interesting**: Unveils disk I/O bottlenecks, filesystem cache hit vs. miss distributions, and tail latency outliers (P99, P99.9) on live production workloads without polling jitter.

### 6.8 Disk Block I/O Payload Size vs. Latency (2D Bivariate eBPF)
Correlate disk request sizes with completion times:

```bash
# Stream: <request_size_kb> <latency_us>
sudo bpftrace -e '
tracepoint:block:block_rq_issue { @start[args->dev, args->sector] = nsecs; }
tracepoint:block:block_rq_complete /@start[args->dev, args->sector]/ {
    $lat_us = (nsecs - @start[args->dev, args->sector]) / 1000;
    $kb = args->nr_sector / 2;
    printf("%d %d\n", $kb, $lat_us);
    delete(@start[args->dev, args->sector]);
}' | histo top --2d --xcol 1 --ycol 2 --autorange --palette plasma
```
- **What it does**: Hooks kernel block I/O tracepoints, emitting request size in kilobytes ($KB$) and round-trip completion latency ($\mu\text{s}$).
- **Why it is interesting**: Renders a live 2D spatial heatmap demonstrating how NVMe/SATA latency scales with payload size and revealing multi-queue congestion patterns.

> [!WARNING]
> **Security & Privilege Isolation Notice**:
> - **Only elevate the tracer, never the monitor**: In `sudo bpftrace ... | histo top`, root permissions apply strictly to `bpftrace` (required by the kernel to attach probes). `histo top` executes as your **unprivileged local user** on the other side of the UNIX pipe.
> - **Never run `sudo histo top`**: Running terminal UI tools as root violates the principle of least privilege and allows commands like `:save /path` or `:export` to write with root permissions.
> - **Alternative to `sudo`**: On modern Linux (5.8+), grant granular kernel capabilities directly to `bpftrace` without invoking `sudo`:
>   ```bash
>   sudo setcap cap_bpf,cap_perfmon,cap_sys_resource+ep $(which bpftrace)
>   ```

### 6.9 2D Coordinate Telemetry (Sensors / Simulation)
```bash
# Monitor bivariate position coordinates (X, Y)
simulation_engine --emit-coords | histo top --2d --bins=40 --palette=inferno
```

---

## 7. Troubleshooting & FAQ

**Q: Output appears uncolored or plain text.**  
*A:* Your terminal might not advertise 24-bit TrueColor support or `NO_COLOR` is set. Use modern terminals (Ghostty, Alacritty, iTerm2, WezTerm, VS Code Terminal, Windows Terminal). Press `C` to switch to ASCII density character mode.

**Q: Upstream process is producing data slowly.**  
*A:* `histo top` includes a 50ms time-flush so low-rate streams (e.g. 5 samples/sec) update smoothly without waiting for full 64-item batches. Ensure upstream producers flush standard output (`python3 -u` or `sys.stdout.flush()`).

**Q: Freezing display (`Space`)—will my upstream data be lost?**  
*A:* No. Ingestion occurs on a separate thread with a dedicated mutex. The input pipe is continuously drained even when the TUI rendering is frozen.
