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
