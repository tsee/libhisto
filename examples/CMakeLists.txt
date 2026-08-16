add_executable(quickstart
    quickstart.c
)

target_link_libraries(quickstart
    PRIVATE
        histo
)
