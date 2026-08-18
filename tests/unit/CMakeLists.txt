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
