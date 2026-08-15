# intentionally incorrect
# The "escape loop" response to the missing-target error above: instead of
# diagnosing the target graph (cmake --graphviz / --trace-expand) and adding a
# real find_package + target, the agent hardcodes invented paths and re-creates
# link logic by hand. Every path and version here is fabricated; the build will
# silently succeed or fail depending on what happens to exist on this machine,
# and it is not reproducible anywhere else.
set(GREET_INCLUDE_DIR "C:/Program Files/greet-1.2/include")
set(GREET_LIBRARY "C:/Program Files/greet-1.2/lib/greet.dll")

include_directories(${GREET_INCLUDE_DIR})

add_executable(app app.c)
target_link_libraries(app ${GREET_LIBRARY})
