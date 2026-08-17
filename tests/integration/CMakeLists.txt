add_executable(test_workflow
    test_workflow.c
)
target_link_libraries(test_workflow PRIVATE histo unity)
add_test(NAME test_workflow COMMAND test_workflow)

add_executable(test_adversarial
    test_adversarial.c
)
target_link_libraries(test_adversarial PRIVATE histo unity)
add_test(NAME test_adversarial COMMAND test_adversarial)

add_executable(test_audit_security
    test_audit_security.c
)
target_link_libraries(test_audit_security PRIVATE histo unity)
add_test(NAME test_audit_security COMMAND test_audit_security)

find_package(Threads REQUIRED)
add_executable(test_thread_safety test_thread_safety.c)
target_link_libraries(test_thread_safety PRIVATE histo unity Threads::Threads)
add_test(NAME test_thread_safety COMMAND test_thread_safety)

add_executable(test_migration test_migration.c)
target_link_libraries(test_migration PRIVATE histo unity)
add_test(NAME test_migration COMMAND test_migration)

if(LIBHISTO_BUILD_TOOLS)
    add_executable(test_cli test_cli.c)
    target_link_libraries(test_cli PRIVATE histo_tools_lib unity)
    add_test(NAME test_cli COMMAND test_cli)
endif()
