cmake_minimum_required(VERSION 3.15)

project(Bdd VERSION 1.0)

option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(BUILD_PYTHON_BINDINGS "Build Python bindings" ON)

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/ss/configuration/cmake/ProjectOptions.cmake")
    message(FATAL_ERROR
        "Git submodules are not initialized. "
        "Run 'git submodule update --init --recursive' before configuring the project.")
endif()

include("${PROJECT_SOURCE_DIR}/ss/configuration/cmake/ProjectOptions.cmake")

require_initialized_submodules()
setup_project()


if(EXISTS "${PROJECT_SOURCE_DIR}/.git")
    require_same_dependency_commit(Configuration
        REFERENCES
            "${PROJECT_SOURCE_DIR}|ss/configuration"
            "${PROJECT_SOURCE_DIR}/ss/threading|ss/configuration"
    )
endif()

include_directories("${PROJECT_SOURCE_DIR}/include")

add_subdirectory(ss)

add_library(bdd INTERFACE)
target_include_directories(bdd INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/ariadne-bdd>
)
target_link_libraries(bdd INTERFACE threading)


add_subdirectory(src)
target_link_libraries(BDD_SRC PRIVATE bdd)

ariadne_register_public_headers(
    TARGET bdd
    DIRECTORIES "${PROJECT_SOURCE_DIR}/include/bdd"
    FILES "${PROJECT_SOURCE_DIR}/include/ariadne-bdd.hpp"
)

setup_project_library(
    TARGET ariadne-bdd
    OBJECTS BDD_SRC
    LINK_LIBRARIES bdd
)

setup_standalone_project_tests(
    TARGET ariadne-bdd
    TEST_DIRECTORY tests
    TEST_TARGET tests
    SOURCES
        "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_SOURCE_DIR}/src"
)

if(BUILD_PYTHON_BINDINGS)
    add_subdirectory(python)
endif()

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    ariadne_install_dependency_bundle(
        TARGET bdd
        DESTINATION include/ariadne-bdd
    )
endif()
