# CLI Extensions & Terminal Ergonomics Design

This document details the design philosophy, ergonomics, and visualizations of the `libhisto` command-line interface (CLI) extensions. Our CLI tools provide powerful, user-centric ways to inspect, summarize, and monitor histogram data directly from the terminal.

## Design Philosophy

The `libhisto` CLI tools are built around three core principles:
1. **Pipes over Files:** Embrace the UNIX philosophy. Tools must seamlessly accept standard input (stdin) streams and produce clean standard output (stdout) for composition with other tools like `grep`, `awk`, or `jq`.
2. **Graceful Degradation:** Rich visual features (ANSI colors, Unicode blocks) must automatically fall back to ASCII or plain text when the terminal does not support them or when output is piped to a non-tty destination.
3. **Progressive Disclosure:** Provide only the most critical information by default. Advanced statistics, percentiles, and complex visualizations should require explicit flags or subcommands.

## Progressive Disclosure Tiers

The CLI features map directly to the Progressive Disclosure tiers of `libhisto`:

### Tier 1: Essential Insights
- **Default Output:** A minimal summary containing total entries, min, max, and mean.
- **Sparklines:** Compact, single-line data representations using Unicode block characters (e.g., ` ▂▃▅▆▇█`) for a quick visual grasp of the distribution.

### Tier 2: Analytical Depth
- **Detailed Stats:** By providing the `--stats` flag, users receive comprehensive statistical moments (variance, standard deviation, skewness, kurtosis) and configurable percentiles (e.g., p50, p90, p99).
- **Text-Based Histograms:** Multi-line ASCII/Unicode bar charts for clear, bin-by-bin density visualization.

### Tier 3: Advanced Visualizations
- **Terminal Heatmaps:** 2D density plots for multi-dimensional data, utilizing 256-color or true-color ANSI escape sequences.
- **Continuous Monitoring:** Real-time tailing of data streams with dynamic UI updates (similar to `top` or `htop`) using ncurses-like rendering techniques.

## Terminal Visualizations

### 1. Sparklines
Sparklines are ideal for inline logging or highly constrained terminal windows. They provide an immediate sense of the shape of the data.

**Example Output:**
```
$ cat latency.log | histo-cli spark
Latency (ms):  ▂▃▅▆▇█▇▆▄▂ 
```

### 2. Stats Output
The stats output is designed for clarity, aligning numerical values and using human-readable formatting.

**Example Output:**
```
$ histo-cli summarize --stats access_times.bin
# Basic Metrics
Count:      1,024,512
Min:        12.4 ms
Max:        4,102.1 ms
Mean:       45.2 ms

# Moments
StdDev:     12.8 ms
Skewness:   1.2
Kurtosis:   3.1

# Percentiles
p50:        42.1 ms
p90:        61.5 ms
p95:        72.0 ms
p99:        115.3 ms
p99.9:      450.2 ms
```

### 3. Heatmaps (2D Histograms)
For 2D histograms, the CLI can render dense information using block characters and color gradients.

**Example Output (ASCII Representation):**
```
$ histo-cli heatmap 2d_data.bin
  Y \ X |  0-10  10-20  20-30  30-40
  ----------------------------------
  40-50 |   ..     ..     ++     ##
  30-40 |   ..     ++     ##     ++
  20-30 |   ++     ##     ++     ..
  10-20 |   ##     ++     ..     ..
```
*(In a real terminal, this would utilize ANSI color codes ranging from dark blue for low density to bright red/white for high density).*

## Doxygen Compatibility

This markdown file is structured to be parsed by Doxygen. Ensure that all headers, code blocks, and lists adhere to standard GitHub Flavored Markdown (GFM) which Doxygen correctly interprets when `USE_MDFILE_AS_MAINPAGE` or standard markdown processing is enabled.
