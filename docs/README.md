# libhisto Documentation

This directory contains technical guides, reference specifications, and manuals for `libhisto`.

## Core Guides & Manuals
- [User Manual & Architecture Reference (Levels 0–5)](manual.md): Comprehensive guide to 1D histograms, moments, CLI, bindings, and benchmarks.
- [Kernel Density Estimation & Automated Binning Guide](kde_guide.md): 1D continuous KDE engine, standard kernels, bandwidth selectors, and optimal binning heuristics.
- [Curve Fitting & Non-Linear Regression Guide](curve_fitting_guide.md): Levenberg-Marquardt optimizer, built-in parametric models, custom callbacks, and covariance interpretation.
- [2D Histograms, Projections & Heatmaps Guide](histo2d_guide.md): 2D histograms, axes, 9-region guards, projections, slices, profile histograms, and heatmaps.
- [Interactive Terminal TUI Design (`histo top`)](design_interactive_tui.md): Specification for real-time interactive streaming monitor, keybindings, and layout.
- [Release, Versioning & Tagging Guide](release_guide.md): Release checklist, version synchronization, tag naming, and distribution packaging.

## Technical & Mathematical Specifications
- [Statistical & Analytical Formulae Reference](statistical_formulae.md): Exact mathematical formulations for Welford moments, robust dispersion, mode/peak estimation, and two-sample statistical distances.
- [Numerical Behavior, Precision & IEEE-754 Rules](numerical_behavior.md): Floating-point edge cases, non-finite handling, subnormals, and precision boundaries.
- [Serialization Wire Formats (V1, V2, V3, JSON)](serialization_format.md): Canonical Little-Endian binary format specifications, header schemas, and JSON representations.

## Project Management & Guidelines
- [Task Tracker & Roadmap](tasks.md): Active development tasks and feature roadmap.
- [Contributing Guidelines](../CONTRIBUTING.md): C99 coding standards, memory safety rules, and verification requirements.
- [Agent Guidelines & Operational Rules](../AGENTS.md): Operational instructions and engineering standards for AI agents.
