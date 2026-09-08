include(CheckCCompilerFlag)

option(LIBHISTO_FUZZ_STANDALONE "Force standalone fuzz runner even if libFuzzer is available" OFF)

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/tools/include
)

set(FUZZ_TARGETS
    fuzz_deserialize_binary
    fuzz_deserialize_json
    fuzz_sketch_binary
    fuzz_fill
    fuzz_deserialize_2d_binary
    fuzz_deserialize_2d_json
    fuzz_fit
    fuzz_bpftrace
    fuzz_cli_opt
    fuzz_tui_command
    fuzz_kde
    fuzz_auto_bin
    fuzz_simd_diff
    fuzz_roundtrip
)

# Corpus generation executable
add_executable(generate_fuzz_corpus
    generate_corpus.c
)
target_link_libraries(generate_fuzz_corpus PRIVATE histo)
if(UNIX AND NOT APPLE)
    target_link_libraries(generate_fuzz_corpus PRIVATE m)
endif()

# Check for LLVM libFuzzer support
set(USE_LIBFUZZER OFF)
if(CMAKE_C_COMPILER_ID MATCHES "Clang" AND NOT LIBHISTO_FUZZ_STANDALONE)
    check_c_compiler_flag("-fsanitize=fuzzer" COMPILER_HAS_LIBFUZZER)
    if(COMPILER_HAS_LIBFUZZER)
        set(USE_LIBFUZZER ON)
    endif()
endif()

if(USE_LIBFUZZER)
    message(STATUS "Fuzzing: Building with native LLVM libFuzzer (-fsanitize=fuzzer,address,undefined)")
    
    foreach(target ${FUZZ_TARGETS})
        add_executable(${target}
            ${target}.c
        )
        target_compile_options(${target} PRIVATE -fsanitize=fuzzer,address,undefined)
        target_link_options(${target} PRIVATE -fsanitize=fuzzer,address,undefined)
        target_link_libraries(${target} PRIVATE histo histocli)
        if(UNIX AND NOT APPLE)
            target_link_libraries(${target} PRIVATE m)
        endif()
    endforeach()
else()
    message(STATUS "Fuzzing: Building with standalone regression fuzz runner (GCC / portable mode)")

    add_library(histo_fuzz_runner STATIC
        standalone_runner.c
    )
    target_include_directories(histo_fuzz_runner PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    foreach(target ${FUZZ_TARGETS})
        add_executable(${target}
            ${target}.c
        )
        target_link_libraries(${target} PRIVATE histo histocli histo_fuzz_runner)
        if(UNIX AND NOT APPLE)
            target_link_libraries(${target} PRIVATE m)
        endif()
    endforeach()

    # Register CTest regression suites
    add_test(NAME fuzz_deserialize_binary COMMAND fuzz_deserialize_binary ${CMAKE_CURRENT_SOURCE_DIR}/corpus/binary)
    add_test(NAME fuzz_deserialize_json COMMAND fuzz_deserialize_json ${CMAKE_CURRENT_SOURCE_DIR}/corpus/json)
    add_test(NAME fuzz_sketch_binary COMMAND fuzz_sketch_binary ${CMAKE_CURRENT_SOURCE_DIR}/corpus/sketch)
    add_test(NAME fuzz_fill COMMAND fuzz_fill ${CMAKE_CURRENT_SOURCE_DIR}/corpus/fill)
    add_test(NAME fuzz_deserialize_2d_binary COMMAND fuzz_deserialize_2d_binary ${CMAKE_CURRENT_SOURCE_DIR}/corpus/binary_2d)
    add_test(NAME fuzz_deserialize_2d_json COMMAND fuzz_deserialize_2d_json ${CMAKE_CURRENT_SOURCE_DIR}/corpus/json_2d)
    add_test(NAME fuzz_fit COMMAND fuzz_fit ${CMAKE_CURRENT_SOURCE_DIR}/corpus/fit)
    add_test(NAME fuzz_bpftrace COMMAND fuzz_bpftrace ${CMAKE_CURRENT_SOURCE_DIR}/corpus/bpftrace)
    add_test(NAME fuzz_cli_opt COMMAND fuzz_cli_opt ${CMAKE_CURRENT_SOURCE_DIR}/corpus/cli_opt)
    add_test(NAME fuzz_tui_command COMMAND fuzz_tui_command ${CMAKE_CURRENT_SOURCE_DIR}/corpus/tui_command)
    add_test(NAME fuzz_kde COMMAND fuzz_kde ${CMAKE_CURRENT_SOURCE_DIR}/corpus/kde)
    add_test(NAME fuzz_auto_bin COMMAND fuzz_auto_bin ${CMAKE_CURRENT_SOURCE_DIR}/corpus/auto_bin)
    add_test(NAME fuzz_simd_diff COMMAND fuzz_simd_diff ${CMAKE_CURRENT_SOURCE_DIR}/corpus/simd_diff)
    add_test(NAME fuzz_roundtrip COMMAND fuzz_roundtrip ${CMAKE_CURRENT_SOURCE_DIR}/corpus/roundtrip)
endif()
