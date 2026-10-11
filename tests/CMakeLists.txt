include(CTest)

add_executable(test_bdd test_bdd.cpp)
target_link_libraries(test_bdd ariadne-bdd bdd)
add_test(NAME test_bdd COMMAND $<TARGET_FILE:test_bdd>)
if(WIN32)
    set_tests_properties(test_bdd PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:ariadne-bdd>"
        TIMEOUT 120
    )
endif()

add_custom_target(tests)
add_dependencies(tests test_bdd)
