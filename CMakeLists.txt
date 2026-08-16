cmake_minimum_required(VERSION 3.15)

project(libhisto
    VERSION 0.1.0
    DESCRIPTION "Fast, portable, memory-safe 1D histogramming C library"
    LANGUAGES C
)

# Standard C99
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Export compile commands for language servers
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Project options
option(LIBHISTO_BUILD_TESTS "Build test suite" ON)
option(LIBHISTO_BUILD_BENCHMARKS "Build benchmark suite" ON)
option(LIBHISTO_BUILD_EXAMPLES "Build example programs" ON)
option(LIBHISTO_ENABLE_SANITIZERS "Enable ASan and UBSan" OFF)
option(LIBHISTO_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

# Configure common warning flags
add_library(histo_compiler_flags INTERFACE)
if(MSVC)
    target_compile_options(histo_compiler_flags INTERFACE /W4)
    if(LIBHISTO_WARNINGS_AS_ERRORS)
        target_compile_options(histo_compiler_flags INTERFACE /WX)
    endif()
else()
    target_compile_options(histo_compiler_flags INTERFACE -Wall -Wextra -Wpedantic)
    if(LIBHISTO_WARNINGS_AS_ERRORS)
        target_compile_options(histo_compiler_flags INTERFACE -Werror)
    endif()
endif()

# Sanitizers configuration
if(LIBHISTO_ENABLE_SANITIZERS AND NOT MSVC)
    target_compile_options(histo_compiler_flags INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(histo_compiler_flags INTERFACE -fsanitize=address,undefined)
endif()

# Core library target
add_subdirectory(src)

# Testing
if(LIBHISTO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Benchmarks
if(LIBHISTO_BUILD_BENCHMARKS)
    add_subdirectory(bench)
endif()

# Examples
if(LIBHISTO_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
