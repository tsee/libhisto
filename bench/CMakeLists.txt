add_executable(bench_histo
    bench_main.c
)

target_link_libraries(bench_histo
    PRIVATE
        histo
)
