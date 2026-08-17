add_library(histo_tools_lib STATIC
    src/cli_common.c
    src/cmd_fill.c
    src/cmd_plot.c
    src/cmd_stats.c
    src/cmd_cmp.c
)

target_include_directories(histo_tools_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(histo_tools_lib PUBLIC histo)

add_executable(histo-cli
    src/main.c
)

set_target_properties(histo-cli PROPERTIES OUTPUT_NAME "histo")

target_include_directories(histo-cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(histo-cli PRIVATE histo_tools_lib)

# Create symlink aliases in build directory
set(TOOL_ALIASES histo-fill histo-plot histo-stats histo-cmp)
foreach(alias ${TOOL_ALIASES})
    add_custom_command(TARGET histo-cli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:histo-cli> ${CMAKE_CURRENT_BINARY_DIR}/${alias}
        COMMENT "Creating CLI alias symlink ${alias} -> histo"
    )
endforeach()

install(TARGETS histo-cli RUNTIME DESTINATION bin)
