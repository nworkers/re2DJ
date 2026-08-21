cmake_minimum_required(VERSION 3.20)

project(re2DJ LANGUAGES CXX)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" RE2DJ_VERSION LIMIT_COUNT 1)
string(STRIP "${RE2DJ_VERSION}" RE2DJ_VERSION)
if(NOT RE2DJ_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must use major.minor.patch format")
endif()
message(STATUS "re2DJ version ${RE2DJ_VERSION}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

option(RE2DJ_BUILD_TESTS "Build the unit test suite" ON)
option(RE2DJ_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# The guest is 32-bit x86 while every supported host is 64-bit or WebAssembly.
# A 32-bit host build is therefore not a supported configuration, and saying so
# at configure time is cheaper than a confusing failure later.
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(WARNING
        "Configuring a 32-bit host build. The supported hosts are 64-bit "
        "Windows, Linux x86-64, and WebAssembly.")
endif()

add_library(re2dj_warnings INTERFACE)
if(MSVC)
    target_compile_options(re2dj_warnings INTERFACE /W4 /permissive-)
    if(RE2DJ_WARNINGS_AS_ERRORS)
        target_compile_options(re2dj_warnings INTERFACE /WX)
    endif()
    # The CRT "secure" warnings push code toward Microsoft-only replacements,
    # which the portability rules forbid in the shared core.
    target_compile_definitions(re2dj_warnings INTERFACE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(re2dj_warnings INTERFACE -Wall -Wextra -Wpedantic)
    if(RE2DJ_WARNINGS_AS_ERRORS)
        target_compile_options(re2dj_warnings INTERFACE -Werror)
    endif()
endif()

# Platform-neutral core. Nothing here may include a host OS header; see the
# portability rules in docs/CODING_STYLE.md.
add_library(re2dj_core STATIC
    src/exe/pe_image.cpp
    src/hdd/hdd_root.cpp
    src/hdd/hdd_scan.cpp
    src/storage/guest_path.cpp
    src/target/target_profile.cpp
    src/version.cpp
)

target_include_directories(re2dj_core
    PUBLIC
        include
)

target_compile_definitions(re2dj_core
    PRIVATE
        RE2DJ_VERSION="${RE2DJ_VERSION}"
)

target_link_libraries(re2dj_core PRIVATE re2dj_warnings)

# std::filesystem needs no separate link library on GCC 9+, Clang with
# libstdc++ 9+, MSVC, or Emscripten, which covers every supported host.

# Command-line host.
add_executable(re2dj
    src/host/cli/main.cpp
)
target_link_libraries(re2dj PRIVATE re2dj_core re2dj_warnings)

# Non-executing analysis tools.
add_executable(re2dj_hdd_probe
    src/tools/hdd_probe/main.cpp
)
target_link_libraries(re2dj_hdd_probe PRIVATE re2dj_core re2dj_warnings)

add_executable(re2dj_pe_analyzer
    src/tools/pe_analyzer/main.cpp
)
target_link_libraries(re2dj_pe_analyzer PRIVATE re2dj_core re2dj_warnings)

if(RE2DJ_BUILD_TESTS)
    enable_testing()
    add_executable(re2dj_unit_tests
        tests/unit/guest_path_test.cpp
        tests/unit/hdd_root_test.cpp
        tests/unit/main.cpp
        tests/unit/pe_image_test.cpp
        tests/unit/synthetic_pe32.cpp
        tests/unit/temporary_tree.cpp
        tests/unit/target_profile_test.cpp
    )
    target_include_directories(re2dj_unit_tests PRIVATE tests/unit)
    target_link_libraries(re2dj_unit_tests PRIVATE re2dj_core re2dj_warnings)
    add_test(NAME re2dj_unit_tests COMMAND re2dj_unit_tests)
endif()
