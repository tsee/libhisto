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
    add_test(
        NAME test_python
        COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${PROJECT_SOURCE_DIR}/bindings/python/tests -v
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/bindings/python
    )
    set_tests_properties(test_python PROPERTIES ENVIRONMENT "PYTHONPATH=${PROJECT_SOURCE_DIR}/bindings/python")

    add_test(
        NAME test_python_dist
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_python_dist.py
    )
    add_custom_target(test_python_dist
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_python_dist.py
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Testing standalone Python distribution tarball building and installation"
    )
endif()

find_package(Perl)
if(PERL_FOUND)
    add_test(
        NAME test_perl_dist
        COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_perl_dist.pl
    )
    add_custom_target(test_perl_dist
        COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_perl_dist.pl
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Testing standalone Perl CPAN distribution tarball building and installation"
    )
endif()



