add_executable(quickstart
    quickstart.c
)

target_link_libraries(quickstart
    PRIVATE
        histo
)

if(LIBHISTO_BUILD_TOOLS)
    add_executable(shm_producer_demo
        ebpf/shm_producer_demo.c
    )
    target_link_libraries(shm_producer_demo
        PRIVATE
            histocli
            histo
    )
    if(UNIX AND NOT APPLE)
        target_link_libraries(shm_producer_demo PRIVATE rt m)
    endif()
endif()
