add_executable(test_lifecycle
    test_lifecycle.c
)

target_link_libraries(test_lifecycle
    PRIVATE
        histo
        unity
)

add_test(NAME test_lifecycle COMMAND test_lifecycle)
