# Preferable to avoid warnings during execution
cmake_minimum_required(VERSION 3.15)

project(TutorialBdd VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
find_package(AriadneBdd REQUIRED)

add_executable(tutorial_bdd tutorial_bdd.cpp)
target_link_libraries(tutorial_bdd PRIVATE AriadneBdd::ariadne-bdd)
