# Diagnose the dependency before declaring it: zlib is an external package, so
# find it through the toolchain environment (pkg-config here) and consume the
# imported target. No versions or paths are invented — pkg-config reports what
# is installed. Note: find_package(ZLIB) alone failed in this environment
# because the MSYS2 ucrt64 prefix is not in CMake's default search path; the
# fix is to configure the toolchain environment (CMAKE_PREFIX_PATH /
# pkg-config), never to hardcode an absolute path.
cmake_minimum_required(VERSION 3.16)
project(good_pkgconfig C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ZLIB REQUIRED IMPORTED_TARGET zlib)

add_executable(app app.c)
target_link_libraries(app PRIVATE PkgConfig::ZLIB)
