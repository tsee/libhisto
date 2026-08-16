# Unity testing framework
add_library(unity STATIC
    vendor/unity/unity.c
)

target_include_directories(unity PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/unity
)

# Enable full double and float assertion support in Unity
target_compile_definitions(unity PUBLIC
    UNITY_INCLUDE_DOUBLE
)

if(UNIX AND NOT APPLE)
    target_link_libraries(unity PUBLIC m)
endif()

# Subdirectories for separated test suites (to be populated in Phase C)
# add_subdirectory(unit)
# add_subdirectory(integration)
