cmake_minimum_required(VERSION 3.15)
project(histo_native LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/vendor/cJSON
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/include
)

set(HISTO_NODE_SOURCES
    src/addon.c
    ../../src/histo.c
    ../../src/histo2d.c
    ../../src/fit.c
    ../../src/kde.c
    ../../src/simd.c
    ../../src/sketch.c
    ../../src/serialize.c
    ../../src/serialize_2d.c
    ../../src/simd_avx2.c
    ../../src/simd_avx512.c
    ../../src/simd_neon.c
    ../../src/vendor/cJSON/cJSON.c
    ../../tools/src/cli_common.c
    ../../tools/src/cli_opt.c
    ../../tools/src/cli_main.c
    ../../tools/src/cli_palette.c
    ../../tools/src/cmd_fill.c
    ../../tools/src/cmd_plot.c
    ../../tools/src/cmd_stats.c
    ../../tools/src/cmd_fit.c
    ../../tools/src/cmd_cmp.c
    ../../tools/src/cmd_top.c
    ../../tools/src/tui_term.c
    ../../tools/src/tui_engine.c
)

add_library(histo_native SHARED ${HISTO_NODE_SOURCES})
set_target_properties(histo_native PROPERTIES
    PREFIX ""
    SUFFIX ".node"
)

if(UNIX AND NOT APPLE)
    target_link_libraries(histo_native PRIVATE m)
endif()
