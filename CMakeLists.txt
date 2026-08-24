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
option(RE2DJ_BUILD_NATIVE_HELPER_PROBE
       "Build the Windows x86 native helper feasibility probe" OFF)
option(RE2DJ_BUILD_LINUX_NATIVE_HELPER_PROBE
       "Build the Linux i386 native helper feasibility probe" OFF)

# Win32 x86 is the primary Windows-native host during the current bring-up.
# A non-Windows 32-bit build remains reserved for explicit helper probes.
if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND NOT WIN32 AND NOT RE2DJ_BUILD_NATIVE_HELPER_PROBE AND
   NOT RE2DJ_BUILD_LINUX_NATIVE_HELPER_PROBE)
    message(WARNING
        "Configuring an unsupported 32-bit non-Windows host build.")
endif()

if(RE2DJ_BUILD_LINUX_NATIVE_HELPER_PROBE)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR
            "RE2DJ_BUILD_LINUX_NATIVE_HELPER_PROBE requires a Linux i386 build")
    endif()
    add_executable(re2dj_linux_native_ipc_helper_probe
        src/platform/linux/native_import_thunks.cpp
        src/platform/linux/native_ipc_helper_probe.cpp
    )
    target_compile_options(re2dj_linux_native_ipc_helper_probe
        PRIVATE -fno-omit-frame-pointer
    )
    target_link_libraries(re2dj_linux_native_ipc_helper_probe PRIVATE re2dj_core re2dj_warnings)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_library(re2dj_linux_native_backend STATIC
        src/platform/linux/native_helper_backend.cpp
    )
    target_link_libraries(re2dj_linux_native_backend
        PUBLIC re2dj_core
        PRIVATE re2dj_warnings
    )
    add_executable(re2dj_linux_native_ipc_host_probe
        src/platform/linux/native_ipc_host_probe.cpp
    )
    target_link_libraries(re2dj_linux_native_ipc_host_probe
        PRIVATE re2dj_linux_native_backend re2dj_warnings
    )
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
    src/device/lptdi_challenge_response.cpp
    src/device/lptdi_response_profile.cpp
    src/input/legacy_io_port_bus.cpp
    src/exe/pe_image.cpp
    src/hdd/hdd_root.cpp
    src/hdd/hdd_scan.cpp
    src/runtime/address_space.cpp
    src/runtime/pe_loader.cpp
    src/storage/guest_path.cpp
    src/storage/vfs_roots.cpp
    src/storage/vfs_file_table.cpp
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

if(RE2DJ_BUILD_NATIVE_HELPER_PROBE)
    if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR
            "RE2DJ_BUILD_NATIVE_HELPER_PROBE requires a Win32 x86 build")
    endif()
    add_executable(re2dj_native_helper_probe
        src/platform/windows/native_helper_probe.cpp
    )
    target_link_libraries(re2dj_native_helper_probe PRIVATE re2dj_warnings)
    add_executable(re2dj_native_ipc_helper
        src/platform/windows/native_import_thunks.cpp
        src/platform/windows/native_ipc_helper.cpp
        src/platform/windows/native_pe_image.cpp
    )
    target_link_libraries(re2dj_native_ipc_helper PRIVATE re2dj_core re2dj_warnings Psapi)
    target_link_options(re2dj_native_ipc_helper PRIVATE
        /BASE:0x20000000
        /DYNAMICBASE:NO
    )
    enable_testing()
    add_test(NAME re2dj_native_helper_probe COMMAND re2dj_native_helper_probe)
endif()

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    add_library(re2dj_windows_injected_runtime SHARED
        src/device/lptdi_challenge_response.cpp
        src/platform/windows/direct3d3_com_facade.cpp
        src/platform/windows/injected_runtime.cpp
    )
    target_include_directories(re2dj_windows_injected_runtime PRIVATE include)
    target_link_libraries(re2dj_windows_injected_runtime PRIVATE re2dj_warnings)
    if(RE2DJ_BUILD_TESTS)
        add_executable(re2dj_windows_vfs_runtime_probe
            src/tools/windows_vfs_runtime_probe/main.cpp
        )
        target_link_libraries(re2dj_windows_vfs_runtime_probe
            PRIVATE re2dj_windows_injected_runtime re2dj_warnings dxguid
        )
        add_test(NAME re2dj_windows_vfs_runtime_probe COMMAND re2dj_windows_vfs_runtime_probe)
    endif()
    add_executable(re2dj_windows_x86_launcher_probe
        src/tools/windows_original_process_probe/iat_verifier.cpp
        src/platform/windows/injected_runtime_loader.cpp
        src/platform/windows/runtime_export_locator.cpp
        src/tools/windows_x86_launcher_probe/remote_module_exports.cpp
        src/tools/windows_x86_launcher_probe/main.cpp
    )
    target_link_libraries(re2dj_windows_x86_launcher_probe
        PRIVATE re2dj_core re2dj_warnings
    )
endif()

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_library(re2dj_windows_native_backend STATIC
        src/platform/windows/native_helper_backend.cpp
    )
    target_link_libraries(re2dj_windows_native_backend
        PUBLIC re2dj_core
        PRIVATE re2dj_warnings
    )
    add_executable(re2dj_native_ipc_host_probe
        src/platform/windows/native_ipc_host_probe.cpp
    )
    target_link_libraries(re2dj_native_ipc_host_probe
        PRIVATE re2dj_windows_native_backend re2dj_warnings
    )
    add_executable(re2dj_windows_import_observer
        src/tools/windows_import_observer/main.cpp
    )
    target_link_libraries(re2dj_windows_import_observer
        PRIVATE re2dj_windows_native_backend re2dj_warnings
    )
    add_executable(re2dj_windows_original_process_probe
        src/tools/windows_original_process_probe/iat_verifier.cpp
        src/tools/windows_original_process_probe/main.cpp
        src/tools/windows_original_process_probe/runtime_loader.cpp
    )
    target_link_libraries(re2dj_windows_original_process_probe
        PRIVATE re2dj_core re2dj_warnings
    )
endif()

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

add_executable(re2dj_pe_loader
    src/tools/pe_loader/main.cpp
)
target_link_libraries(re2dj_pe_loader PRIVATE re2dj_core re2dj_warnings)

if(RE2DJ_BUILD_TESTS)
    enable_testing()
    add_executable(re2dj_unit_tests
        tests/unit/address_space_test.cpp
        tests/unit/execution_backend_test.cpp
        tests/unit/guest_path_test.cpp
        tests/unit/hdd_root_test.cpp
        tests/unit/main.cpp
        tests/unit/lptdi_challenge_response_test.cpp
        tests/unit/lptdi_response_profile_test.cpp
        tests/unit/legacy_io_port_bus_test.cpp
        tests/unit/pe_image_test.cpp
        tests/unit/pe_loader_test.cpp
        tests/unit/synthetic_pe32.cpp
        tests/unit/temporary_tree.cpp
        tests/unit/target_profile_test.cpp
        tests/unit/vfs_file_table_test.cpp
    )
    target_include_directories(re2dj_unit_tests PRIVATE tests/unit)
    target_link_libraries(re2dj_unit_tests PRIVATE re2dj_core re2dj_warnings)
    add_test(NAME re2dj_unit_tests COMMAND re2dj_unit_tests)
endif()
