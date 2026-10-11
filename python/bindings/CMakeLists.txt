include_directories(${Python3_INCLUDE_DIRS})

get_target_property(PYTHON_COMMON_SOURCE_DIR pyariadne-python-common SOURCE_DIR)

add_library(pyariadne-bdd-submodule-obj OBJECT
    bdd_submodule.cpp
)
target_include_directories(pyariadne-bdd-submodule-obj PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PYTHON_COMMON_SOURCE_DIR}/include"
)
target_link_libraries(pyariadne-bdd-submodule-obj PRIVATE
    pyariadne-python-common
    pybind11::module
    bdd
)
set_target_properties(pyariadne-bdd-submodule-obj PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(pyariadne-bdd INTERFACE)
target_sources(pyariadne-bdd INTERFACE
    $<TARGET_OBJECTS:pyariadne-bdd-submodule-obj>
)
target_include_directories(pyariadne-bdd INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_link_libraries(pyariadne-bdd INTERFACE
    pyariadne-python-common
)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    add_library(pyariadne-bdd-module-obj OBJECT
        bdd_module.cpp
    )
    target_include_directories(pyariadne-bdd-module-obj PRIVATE
        "${PYTHON_COMMON_SOURCE_DIR}/include"
    )
    target_link_libraries(pyariadne-bdd-module-obj PRIVATE
        pyariadne-python-common
        pybind11::module
    )
    set_target_properties(pyariadne-bdd-module-obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
