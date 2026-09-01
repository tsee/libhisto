# CMake build configuration for libhisto C++ wrapper test suite

set(CPP_TEST_SOURCES
    test_main.cpp
    test_status_result.cpp
    test_histogram.cpp
    test_histogram_view.cpp
    test_move_semantics.cpp
    test_zero_overhead.cpp
    test_iterators.cpp
    test_arithmetic.cpp
    test_serialization.cpp
    test_histogram2d.cpp
    test_sketches.cpp
    test_kde_fit.cpp
    test_no_exceptions.cpp
)

add_executable(test_histo_cpp ${CPP_TEST_SOURCES})

target_include_directories(test_histo_cpp PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(test_histo_cpp PRIVATE
    histo
    histo_compiler_flags
)

if(UNIX AND NOT APPLE)
    target_link_libraries(test_histo_cpp PRIVATE m)
endif()

if(NOT MSVC)
    target_compile_options(test_histo_cpp PRIVATE -fno-exceptions)
endif()

add_test(NAME test_histo_cpp COMMAND test_histo_cpp)
