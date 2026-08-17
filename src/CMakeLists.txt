add_library(histo
    histo.c
    sketch.c
    serialize.c
    vendor/cJSON/cJSON.c
)

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
