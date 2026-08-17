add_library(histo
    histo.c
    simd.c
    sketch.c
    serialize.c
    vendor/cJSON/cJSON.c
)

option(LIBHISTO_ENABLE_AVX2 "Enable AVX2 vectorization" ON)
option(LIBHISTO_ENABLE_AVX512 "Enable AVX-512 vectorization" ON)

if(LIBHISTO_ENABLE_AVX2)
    target_sources(histo PRIVATE simd_avx2.c)
    set_source_files_properties(simd_avx2.c PROPERTIES COMPILE_FLAGS "-mavx2 -mfma")
    target_compile_definitions(histo PRIVATE LIBHISTO_ENABLE_AVX2)
endif()

if(LIBHISTO_ENABLE_AVX512)
    target_sources(histo PRIVATE simd_avx512.c)
    set_source_files_properties(simd_avx512.c PROPERTIES COMPILE_FLAGS "-mavx512f -mfma")
    target_compile_definitions(histo PRIVATE LIBHISTO_ENABLE_AVX512)
endif()

target_link_libraries(histo
    PUBLIC
        histo_compiler_flags
)

if(UNIX AND NOT APPLE)
    target_link_libraries(histo PUBLIC m)
endif()

target_include_directories(histo
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/cJSON
)
