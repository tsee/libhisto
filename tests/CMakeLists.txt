# Unity testing framework
add_library(unity STATIC
    vendor/unity/unity.c
)

target_include_directories(unity PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/unity
)

# Enable full double precision and 64-bit integer assertion support in Unity
target_compile_definitions(unity PUBLIC
    UNITY_INCLUDE_DOUBLE
    UNITY_SUPPORT_64
)

if(UNIX AND NOT APPLE)
    target_link_libraries(unity PUBLIC m)
endif()

# Unit tests
add_subdirectory(unit)

# Integration tests
add_subdirectory(integration)

# C++ wrapper tests (only enabled when working C++ compiler and cpp test directory are available)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cpp/CMakeLists.txt")
    include(CheckLanguage)
    check_language(CXX)
    if(CMAKE_CXX_COMPILER)
        # Probe whether C++ compiler can successfully link with active compiler flags (e.g. -m32 multilib)
        execute_process(
            COMMAND ${CMAKE_CXX_COMPILER} ${CMAKE_CXX_FLAGS} -x c++ /dev/null -o /dev/null
            RESULT_VARIABLE _CXX_PROBE_RESULT
            ERROR_QUIET
            OUTPUT_QUIET
        )
        if(_CXX_PROBE_RESULT EQUAL 0)
            enable_language(CXX)
            set(CMAKE_CXX_STANDARD 17)
            set(CMAKE_CXX_STANDARD_REQUIRED ON)
            set(CMAKE_CXX_EXTENSIONS OFF)
            add_subdirectory(cpp)
        else()
            message(STATUS "C++ compiler probe failed (missing multilib C++ runtime or incompatible flags); skipping C++ wrapper tests.")
        endif()
    endif()
endif()

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

# Language binding tests in CTest (disabled by default in standard core CMake/CTest)
if(LIBHISTO_ENABLE_BINDINGS_TESTS)
    if(Python3_FOUND)
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
endif()



