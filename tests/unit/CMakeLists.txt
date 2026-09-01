# Unit Test: Lifecycle & Memory
add_executable(test_lifecycle
    test_lifecycle.c
)
target_link_libraries(test_lifecycle PRIVATE histo unity)
add_test(NAME test_lifecycle COMMAND test_lifecycle)

# Unit Test: Bin Lookup, Ingestion & IEEE-754 Edge Cases
add_executable(test_fill
    test_fill.c
)
target_link_libraries(test_fill PRIVATE histo unity)
add_test(NAME test_fill COMMAND test_fill)

# Unit Test: Queries, Integrals, Moments & Quantiles
add_executable(test_stats
    test_stats.c
)
target_link_libraries(test_stats PRIVATE histo unity)
add_test(NAME test_stats COMMAND test_stats)

# Unit Test: Arithmetic, Transformations, Rebinning & CDF
add_executable(test_arithmetic
    test_arithmetic.c
)
target_link_libraries(test_arithmetic PRIVATE histo unity)
add_test(NAME test_arithmetic COMMAND test_arithmetic)

# Unit Test: Binary and JSON Serialization & Deserialization
add_executable(test_serialization
    test_serialization.c
)
target_link_libraries(test_serialization PRIVATE histo unity)
add_test(NAME test_serialization COMMAND test_serialization)

# Unit Test: Audit Moments and Statistics
add_executable(test_audit_moments
    test_audit_moments.c
)
target_link_libraries(test_audit_moments PRIVATE histo unity)
add_test(NAME test_audit_moments COMMAND test_audit_moments)

# Unit Test: Boundary Mathematics, Ingestion, and Arithmetic Flaw Audit
add_executable(test_audit_boundaries
    test_audit_boundaries.c
)
target_link_libraries(test_audit_boundaries PRIVATE histo unity)
add_test(NAME test_audit_boundaries COMMAND test_audit_boundaries)

# Unit Test: Two-Histogram Comparison & Distance Metrics
add_executable(test_comparison
    test_comparison.c
)
target_link_libraries(test_comparison PRIVATE histo unity)
add_test(NAME test_comparison COMMAND test_comparison)

# Unit Test: Sketch
add_executable(test_sketch
    test_sketch.c
)
target_link_libraries(test_sketch PRIVATE histo unity)
add_test(NAME test_sketch COMMAND test_sketch)

# Unit Test: SIMD Vector Acceleration
add_executable(test_simd
    test_simd.c
)
target_link_libraries(test_simd PRIVATE histo unity)
add_test(NAME test_simd COMMAND test_simd)

# Unit Test: Curve Fitting & Non-Linear Regression Engine
add_executable(test_fit
    test_fit.c
)
target_link_libraries(test_fit PRIVATE histo unity)
add_test(NAME test_fit COMMAND test_fit)

# Unit Test: 2-Dimensional Histograms (Lifecycle, Ingestion, 9-Guard Zones)
add_executable(test_histo2d
    test_histo2d.c
)
target_link_libraries(test_histo2d PRIVATE histo unity)
add_test(NAME test_histo2d COMMAND test_histo2d)

# Unit Test: 2D Moments, Projections, Profiles & Transformations
add_executable(test_histo2d_stats
    test_histo2d_stats.c
)
target_link_libraries(test_histo2d_stats PRIVATE histo unity)
add_test(NAME test_histo2d_stats COMMAND test_histo2d_stats)

# Unit Test: Automated Optimal Bin Width Heuristics
add_executable(test_auto_bin
    test_auto_bin.c
)
target_link_libraries(test_auto_bin PRIVATE histo unity)
add_test(NAME test_auto_bin COMMAND test_auto_bin)

# Unit Test: Kernel Density Estimation (KDE) Engine
add_executable(test_kde
    test_kde.c
)
target_link_libraries(test_kde PRIVATE histo unity)
add_test(NAME test_kde COMMAND test_kde)

# CLI and TUI Tool Unit Tests (only enabled when building tools)
if(LIBHISTO_BUILD_TOOLS)
    # Unit Test: Interactive TUI Engine & Reservoir Cache
    add_executable(test_tui_engine
        test_tui_engine.c
    )
    target_link_libraries(test_tui_engine PRIVATE histocli histo unity Threads::Threads)
    add_test(NAME test_tui_engine COMMAND test_tui_engine)

    # Unit Test: CLI Palette Engine & Colormaps
    add_executable(test_palette
        test_palette.c
    )
    target_link_libraries(test_palette PRIVATE histocli histo unity)
    add_test(NAME test_palette COMMAND test_palette)

    # Unit Test: Declarative CLI Option Parsing Framework
    add_executable(test_cli_opt
        test_cli_opt.c
    )
    target_link_libraries(test_cli_opt PRIVATE histocli histo unity)
    add_test(NAME test_cli_opt COMMAND test_cli_opt)

    # Unit Test: bpftrace ASCII Table Parser
    add_executable(test_bpftrace_parser
        test_bpftrace_parser.c
    )
    target_link_libraries(test_bpftrace_parser PRIVATE histocli histo unity)
    add_test(NAME test_bpftrace_parser COMMAND test_bpftrace_parser)

    # Unit Test: Shared Memory Ring Buffer IPC
    add_executable(test_shm_ring
        test_shm_ring.c
    )
    target_link_libraries(test_shm_ring PRIVATE histocli histo unity Threads::Threads)
    add_test(NAME test_shm_ring COMMAND test_shm_ring)
endif()

