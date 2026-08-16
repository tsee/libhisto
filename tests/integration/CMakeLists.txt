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
