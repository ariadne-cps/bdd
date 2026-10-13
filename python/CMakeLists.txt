set(Python3_FIND_VIRTUALENV FIRST)
set(Python3_FIND_STRATEGY LOCATION)
set(PYBIND11_FINDPYTHON ON)

find_package(Python3 3.11 COMPONENTS Interpreter Development.Module)

option(PYTHON_ENFORCE_TESTS "Enforce Python tests" OFF)

if(Python3_FOUND)
    report_python_configuration_once()

    if(NOT TARGET pybind11::module)
        get_target_property(PYTHON_COMMON_SOURCE_DIR pyariadne-python-common SOURCE_DIR)
        add_subdirectory(
            "${PYTHON_COMMON_SOURCE_DIR}/pybind11"
            "${CMAKE_CURRENT_BINARY_DIR}/pybind11"
        )
    endif()

    add_subdirectory(bindings)

    if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
        ariadne_finalize_python_module(NAME bdd)

        set(PYTHON_BINDINGS_INSTALL_DIR "${Python3_SITEARCH}"
            CACHE STRING "Install destination for Python targets")
        install(TARGETS pyariadne DESTINATION "${PYTHON_BINDINGS_INSTALL_DIR}")

        add_subdirectory(tests)
        add_dependencies(bindings pyariadne)
    endif()
else()
    if(PYTHON_ENFORCE_TESTS)
        message(FATAL_ERROR "Python >= 3.11 with Interpreter and Development.Module components is required when PYTHON_ENFORCE_TESTS=ON.")
    else()
        message(STATUS "Compatible Python development environment not found: Python bindings will not be built.")
    endif()
endif()
