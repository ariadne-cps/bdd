include(CTest)

add_executable(test_robdd test_robdd.cpp)
target_link_libraries(test_robdd PRIVATE ariadne-bdd bdd ariadne-testing)
add_test(NAME test_robdd COMMAND $<TARGET_FILE:test_robdd> --concise)
if(WIN32)
    set_tests_properties(test_robdd PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:ariadne-bdd>"
        TIMEOUT 120
    )
endif()

add_custom_target(tests)
add_dependencies(tests test_robdd)
