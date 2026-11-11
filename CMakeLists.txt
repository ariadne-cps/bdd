cmake_minimum_required(VERSION 3.15)

project(AriadneBdd VERSION 3.0)

option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(BUILD_PYTHON_BINDINGS "Build Python bindings" ON)
option(ARIADNE_BDD_UNIQUE_TABLE_ORDERED
    "Use legacy ordered canonical unique table for manual A/B measurements" OFF)

if(NOT TARGET ariadne-build)
    find_package(Git REQUIRED)
    # Keep initial Build checkout output in English.
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "LC_ALL=C"
                "${GIT_EXECUTABLE}" -C "${CMAKE_CURRENT_SOURCE_DIR}"
                submodule update --init -- ss/bui
        RESULT_VARIABLE _result)
    if(_result)
        message(FATAL_ERROR "Cannot initialize build submodule")
    endif()
    include("${CMAKE_CURRENT_SOURCE_DIR}/ss/bui/cmake/ProjectOptions.cmake")
endif()

require_same_dependency_commit(Build CHAINS
    "build" "threading|build")

require_same_dependency_commit(Logging CHAINS
    "threading|logging" "benchmarking|logging")

ariadne_setup_module(bdd)

# Public ABI selection: must propagate consistently to clients and sources.
if(ARIADNE_BDD_UNIQUE_TABLE_ORDERED)
    target_compile_definitions(bdd INTERFACE
        ARIADNE_BDD_UNIQUE_TABLE_ORDERED=1)
endif()
