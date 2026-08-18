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

# Documentation inline examples test
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    add_test(
        NAME test_doc_examples
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_doc_examples.py
                --source-dir ${PROJECT_SOURCE_DIR}
                --build-dir ${PROJECT_BINARY_DIR}
                --compiler ${CMAKE_C_COMPILER}
    )
    add_custom_target(test_doc_examples
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_doc_examples.py
                --source-dir ${PROJECT_SOURCE_DIR}
                --build-dir ${PROJECT_BINARY_DIR}
                --compiler ${CMAKE_C_COMPILER}
                --verbose
        DEPENDS histo histo-cli
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Testing inline documentation code examples and CLI pipelines"
    )
endif()

