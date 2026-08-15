# intentionally incorrect
# The library target "dep" declares a PUBLIC dependency on the imported-style
# target "greet::greet", but neither add_library(greet) nor find_package(greet)
# exists anywhere in the project. CMake's error names the missing target and
# lists the possible causes (typo / missing find_package / missing ALIAS).
# The fix is to diagnose the target graph, NOT to invent a version, hardcode an
# absolute path, or paste hand-rolled detection logic.
cmake_minimum_required(VERSION 3.16)
project(bad_dep C)

add_library(dep STATIC dep.c)

# PUBLIC propagates "greet::greet" into every consumer's link interface.
# greet::greet does not exist -> configure fails with "target was not found".
target_link_libraries(dep PUBLIC greet::greet)

add_executable(app app.c)
target_link_libraries(app PRIVATE dep)
