if(NOT TARGET threading)
    add_subdirectory(threading)
endif()

if(BUILD_PYTHON_BINDINGS AND NOT TARGET pyariadne-python-common)
    add_subdirectory(python-common)
endif()
