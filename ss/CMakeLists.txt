if(NOT TARGET threading)
    ensure_submodule_initialized(threading)
    add_subdirectory(threading)
endif()

if(BUILD_PYTHON_BINDINGS AND NOT TARGET pyariadne-python-common)
    ensure_submodule_initialized(python-common)
    add_subdirectory(python-common)
endif()
