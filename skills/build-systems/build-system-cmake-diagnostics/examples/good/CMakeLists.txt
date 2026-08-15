cmake_minimum_required(VERSION 3.16)
project(good_dep C)

# Diagnose first: the dependency is a real target in this project, so declare it
# as a proper library target and let CMake propagate usage requirements.
add_library(greet STATIC greet.c)
target_include_directories(greet PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(app app.c)

# target_link_libraries transfers usage requirements (the PUBLIC include dir) and
# the link dependency. The dependency graph is now real and visible to
# cmake --graphviz / ninja -t deps.
target_link_libraries(app PRIVATE greet)
