# Unity testing framework
add_library(unity STATIC
    vendor/unity/unity.c
)

target_include_directories(unity PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/unity
)

# Enable full double precision assertion support in Unity
target_compile_definitions(unity PUBLIC
    UNITY_INCLUDE_DOUBLE
)

if(UNIX AND NOT APPLE)
    target_link_libraries(unity PUBLIC m)
endif()

# Unit tests
add_subdirectory(unit)

# Integration tests
add_subdirectory(integration)

# Fuzzing harnesses
if(LIBHISTO_ENABLE_FUZZING)
    add_subdirectory(fuzz)
endif()
