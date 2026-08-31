cmake_minimum_required(VERSION 3.20)

project(re2DJ LANGUAGES C CXX)

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
option(RE2DJ_BUILD_LINUX_NATIVE_HELPER
       "Build the Linux i386 native helper" OFF)
option(RE2DJ_ENABLE_SDL3_AUDIO "Build the SDL3/SDL3_mixer audio backend" ON)

if(NOT RE2DJ_BUILD_LINUX_NATIVE_HELPER)
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL_VIDEO ON CACHE BOOL "" FORCE)
    set(SDL_RENDER OFF CACHE BOOL "" FORCE)
    set(SDL_GPU OFF CACHE BOOL "" FORCE)
    set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
    set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
    set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
    set(SDL_HIDAPI OFF CACHE BOOL "" FORCE)
    set(SDL_POWER OFF CACHE BOOL "" FORCE)
    set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
    set(SDL_DIALOG OFF CACHE BOOL "" FORCE)
    set(SDL_TRAY OFF CACHE BOOL "" FORCE)
    if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4 AND RE2DJ_ENABLE_SDL3_AUDIO)
        set(SDL_AUDIO ON CACHE BOOL "" FORCE)
    else()
        set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
    endif()
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG 147a8ee32dbf9ac02f3794964490687b6bbda1bc
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL3)
endif()

# Win32 x86 is the primary Windows-native host during the current bring-up.
# A non-Windows 32-bit build remains reserved for explicit helper probes.
if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND NOT WIN32 AND NOT RE2DJ_BUILD_NATIVE_HELPER_PROBE AND
   NOT RE2DJ_BUILD_LINUX_NATIVE_HELPER)
    message(WARNING
        "Configuring an unsupported 32-bit non-Windows host build.")
endif()

if(RE2DJ_BUILD_LINUX_NATIVE_HELPER)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR
            "RE2DJ_BUILD_LINUX_NATIVE_HELPER requires a Linux i386 build")
    endif()
    add_executable(re2dj_linux_native_ipc_helper
        src/platform/linux/native_import_thunks.cpp
        src/platform/linux/native_ipc_helper.cpp
        src/platform/linux/native_process_bootstrap.cpp
    )
    target_compile_options(re2dj_linux_native_ipc_helper
        PRIVATE -fno-omit-frame-pointer
    )
    target_link_libraries(re2dj_linux_native_ipc_helper PRIVATE re2dj_core re2dj_warnings)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_library(re2dj_linux_native_backend STATIC
        src/platform/linux/native_helper_backend.cpp
        src/platform/linux/original_runner.cpp
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
    target_compile_options(re2dj_warnings INTERFACE /W4 /permissive- /utf-8)
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

add_library(re2dj_legacy_graphics STATIC
    src/graphics/legacy_draw_command.cpp
    src/graphics/legacy_texture.cpp
    src/graphics/legacy_transform.cpp
    src/graphics/legacy_vertex_buffer.cpp
)
target_include_directories(re2dj_legacy_graphics PUBLIC include)
target_link_libraries(re2dj_legacy_graphics PRIVATE re2dj_warnings)

if(NOT RE2DJ_BUILD_LINUX_NATIVE_HELPER)
    add_library(re2dj_sdl3_opengl_backend STATIC
        src/graphics/sdl3_opengl_backend.cpp
    )
    target_include_directories(re2dj_sdl3_opengl_backend PUBLIC include)
    target_link_libraries(re2dj_sdl3_opengl_backend
        PUBLIC re2dj_legacy_graphics
        PRIVATE SDL3::SDL3-static re2dj_warnings
    )
endif()

# Platform-neutral core. Nothing here may include a host OS header; see the
# portability rules in docs/CODING_STYLE.md.
add_library(re2dj_core STATIC
    src/audio/legacy_audio_buffer.cpp
    src/device/hardlock_450_response.cpp
    src/device/hardlock_api_descriptor.cpp
    src/device/lptdi_challenge_response.cpp
    src/device/lptdi_response_profile.cpp
    src/input/ez2dj_io_board.cpp
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

target_link_libraries(re2dj_core
    PUBLIC re2dj_legacy_graphics
    PRIVATE re2dj_warnings
)

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
    if(RE2DJ_ENABLE_SDL3_AUDIO)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_INSTALL OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_TESTS OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_AIFF OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_WAVE OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_VOC OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_AU OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_FLAC OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_GME OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_MOD OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_MP3 OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_MIDI OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_OPUS OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_VORBIS_STB OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_VORBIS_VORBISFILE OFF CACHE BOOL "" FORCE)
        set(SDLMIXER_WAVPACK OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(SDL3_mixer
            GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
            GIT_TAG 72a81869b45e249e8e67102db4e98dd2441f05a1
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(SDL3_mixer)
    endif()

    add_library(re2dj_windows_injected_runtime SHARED
        src/device/hardlock_api_descriptor.cpp
        src/device/lptdi_challenge_response.cpp
        src/input/ez2dj_io_board.cpp
        src/input/legacy_io_port_bus.cpp
        src/platform/windows/audio_volume_trace.cpp
        src/platform/windows/direct3d3_com_facade.cpp
        src/platform/windows/ez2dj_keyboard_input.cpp
        src/platform/windows/host_window_shell.cpp
        src/platform/windows/ini_profile_hle.cpp
        src/platform/windows/injected_runtime.cpp
        src/platform/windows/window_mode.cpp
        src/platform/windows/winmm_mixer_trace.cpp
    )
    target_include_directories(re2dj_windows_injected_runtime PRIVATE include)
    target_compile_definitions(re2dj_windows_injected_runtime PRIVATE
        RE2DJ_VERSION="${RE2DJ_VERSION}"
    )
    target_link_libraries(re2dj_windows_injected_runtime
        PRIVATE re2dj_sdl3_opengl_backend re2dj_warnings dwmapi dxguid winmm
    )
    if(RE2DJ_ENABLE_SDL3_AUDIO)
        target_sources(re2dj_windows_injected_runtime PRIVATE
            src/audio/legacy_audio_buffer.cpp
            src/audio/sdl3_mixer_audio_backend.cpp
            src/platform/windows/directsound_com_facade.cpp
        )
        target_link_libraries(re2dj_windows_injected_runtime
            PRIVATE SDL3_mixer::SDL3_mixer-static SDL3::SDL3-static
        )
    endif()
    if(RE2DJ_BUILD_TESTS)
        add_executable(re2dj_windows_vfs_runtime_probe
            src/tools/windows_vfs_runtime_probe/main.cpp
        )
        target_compile_definitions(re2dj_windows_vfs_runtime_probe PRIVATE
            RE2DJ_VERSION="${RE2DJ_VERSION}"
        )
        target_link_libraries(re2dj_windows_vfs_runtime_probe
            PRIVATE re2dj_windows_injected_runtime re2dj_warnings dwmapi dxguid
        )
        add_test(NAME re2dj_windows_vfs_runtime_probe COMMAND re2dj_windows_vfs_runtime_probe)
        set_tests_properties(re2dj_windows_vfs_runtime_probe PROPERTIES
            ENVIRONMENT "SDL_AUDIODRIVER=dummy"
        )
        add_executable(re2dj_windows_hardlock_descriptor_probe
            src/tools/windows_hardlock_descriptor_probe/main.cpp
        )
        target_link_libraries(re2dj_windows_hardlock_descriptor_probe
            PRIVATE re2dj_windows_injected_runtime re2dj_warnings
        )
        add_test(NAME re2dj_windows_hardlock_descriptor_probe
                 COMMAND re2dj_windows_hardlock_descriptor_probe)
    endif()
    add_library(re2dj_windows_original_process_backend STATIC
        src/tools/windows_original_process_probe/iat_verifier.cpp
        src/platform/windows/injected_runtime_loader.cpp
        src/platform/windows/original_process_backend.cpp
        src/platform/windows/runtime_export_locator.cpp
        src/tools/windows_x86_launcher_probe/remote_module_exports.cpp
        src/tools/windows_x86_launcher_probe/main.cpp
    )
    target_include_directories(re2dj_windows_original_process_backend PUBLIC include)
    target_link_libraries(re2dj_windows_original_process_backend
        PRIVATE re2dj_core re2dj_warnings
    )
    add_executable(re2dj_windows_x86_launcher_probe
        src/tools/windows_x86_launcher_probe/entry.cpp
    )
    target_link_libraries(re2dj_windows_x86_launcher_probe
        PRIVATE re2dj_windows_original_process_backend re2dj_warnings
    )
    if(RE2DJ_BUILD_TESTS)
        add_executable(re2dj_windows_product_loader_probe
            src/tools/windows_product_loader_probe/main.cpp
        )
        target_link_libraries(re2dj_windows_product_loader_probe
            PRIVATE re2dj_windows_original_process_backend re2dj_warnings
        )
        add_test(NAME re2dj_windows_product_loader_probe
                 COMMAND re2dj_windows_product_loader_probe)
    endif()
endif()

# std::filesystem needs no separate link library on GCC 9+, Clang with
# libstdc++ 9+, MSVC, or Emscripten, which covers every supported host.

# Command-line host.
add_executable(re2dj
    src/host/cli/main.cpp
)
target_link_libraries(re2dj PRIVATE re2dj_core re2dj_warnings)
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_link_libraries(re2dj PRIVATE re2dj_windows_original_process_backend)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    target_link_libraries(re2dj PRIVATE re2dj_linux_native_backend)
endif()

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
        tests/unit/ez2dj_io_board_test.cpp
        tests/unit/guest_path_test.cpp
        tests/unit/hardlock_450_response_test.cpp
        tests/unit/hardlock_api_descriptor_test.cpp
        tests/unit/hdd_root_test.cpp
        tests/unit/main.cpp
        tests/unit/lptdi_challenge_response_test.cpp
        tests/unit/lptdi_response_profile_test.cpp
        tests/unit/legacy_io_port_bus_test.cpp
        tests/unit/legacy_draw_command_test.cpp
        tests/unit/legacy_texture_test.cpp
        tests/unit/legacy_transform_test.cpp
        tests/unit/legacy_vertex_buffer_test.cpp
        tests/unit/legacy_audio_buffer_test.cpp
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
