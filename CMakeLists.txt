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
# The build configuration (Debug, Release, ...) the window title shows; see
# re2dj/version.h.
add_compile_definitions(RE2DJ_BUILD_CONFIG="$<CONFIG>")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

find_package(spdlog 1.14.1 EXACT CONFIG QUIET)
if(NOT spdlog_FOUND)
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
    set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.14.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(spdlog)
endif()

if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

option(RE2DJ_BUILD_TESTS "Build the unit test suite" ON)
option(RE2DJ_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(RE2DJ_ENABLE_SDL3_AUDIO "Build the SDL3/SDL3_mixer audio backend" ON)

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
# Sound plays through SDL3_mixer on the 32-bit Windows runtime and on Linux.
if(RE2DJ_ENABLE_SDL3_AUDIO AND ((WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4) OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
    set(RE2DJ_SDL3_AUDIO_ACTIVE ON)
else()
    set(RE2DJ_SDL3_AUDIO_ACTIVE OFF)
endif()
if(RE2DJ_SDL3_AUDIO_ACTIVE)
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

# Dear ImGui ships no CMake project, so only its sources are fetched and the
# on-screen display library below builds the parts it uses. The platform
# backends are deliberately left out: the OSD feeds input itself so the
# shared core never depends on a host windowing API.
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG 01380c579715e62fb9a8d6ec0502c4ea83bfde6e  # v1.92.9
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

# Win32 x86 and Linux x86 are supported product-host configurations. Other
# non-Windows 32-bit configurations remain reserved for explicit probes.
if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND NOT WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(WARNING
        "Configuring an unsupported 32-bit non-Windows host build.")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_library(re2dj_linux_native_backend STATIC
        src/platform/linux/native_continuation_observation.cpp
        src/platform/linux/native_create_file_observation.cpp
        src/platform/linux/native_dynamic_thunk.cpp
        src/platform/linux/native_fault_observation.cpp
        src/platform/linux/native_getversion_observation.cpp
        src/platform/linux/native_guest_module_image.cpp
        src/platform/linux/native_guest_module_set.cpp
        src/platform/linux/native_guest_seh.cpp
        src/platform/linux/native_guest_threads.cpp
        src/platform/linux/native_import_thunks.cpp
        src/platform/linux/native_in_process_runner.cpp
        src/platform/linux/native_instruction_trace.cpp
        src/platform/linux/native_kernel32_diagnostic.cpp
        src/platform/linux/native_legacy_io.cpp
        src/platform/linux/native_pe_image.cpp
        src/platform/linux/host_presentation.cpp
        src/platform/linux/host_keyboard.cpp
        src/platform/linux/native_pe_session.cpp
        src/platform/linux/original_runner.cpp
    )
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        target_sources(re2dj_linux_native_backend PRIVATE
            src/platform/linux/x86/native_import_bridge.cpp
            src/platform/linux/x86/native_low_memory.cpp
            src/platform/linux/x86/native_process_bootstrap.cpp
        )
        target_compile_options(re2dj_linux_native_backend PRIVATE -fno-omit-frame-pointer)
        add_executable(re2dj_linux_native_in_process_probe
            src/platform/linux/x86/native_in_process_probe.cpp
            src/platform/linux/native_thread_probe.cpp
            src/platform/native_probe_fixture.cpp
        )
        target_link_libraries(re2dj_linux_native_in_process_probe
            PRIVATE re2dj_linux_native_backend re2dj_warnings
        )
        add_executable(re2dj_linux_native_guest_module_probe
            src/platform/linux/x86/native_guest_module_probe.cpp
        )
        target_link_libraries(re2dj_linux_native_guest_module_probe
            PRIVATE re2dj_linux_native_backend re2dj_warnings
        )
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        target_sources(re2dj_linux_native_backend PRIVATE
            src/platform/linux/x64/native_compat_mode.cpp
            src/platform/linux/x64/native_compat_mode_transition.cpp
            src/platform/linux/x64/native_import_bridge.cpp
            src/platform/linux/x64/native_low_memory.cpp
            src/platform/linux/x64/native_process_bootstrap.cpp
        )
        target_compile_options(re2dj_linux_native_backend PRIVATE -fno-omit-frame-pointer)
        add_executable(re2dj_linux_compat_mode_probe
            src/platform/linux/x64/native_compat_mode_probe.cpp
        )
        target_link_libraries(re2dj_linux_compat_mode_probe
            PRIVATE re2dj_linux_native_backend re2dj_warnings
        )
        add_executable(re2dj_linux_native_in_process_probe
            src/platform/linux/x64/native_in_process_probe.cpp
            src/platform/linux/native_thread_probe.cpp
            src/platform/native_probe_fixture.cpp
        )
        target_link_libraries(re2dj_linux_native_in_process_probe
            PRIVATE re2dj_linux_native_backend re2dj_warnings
        )
    endif()
    target_link_libraries(re2dj_linux_native_backend
        PUBLIC re2dj_core
        PRIVATE re2dj_sdl3_opengl_backend re2dj_osd SDL3::SDL3-static re2dj_logging re2dj_warnings
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

# libchdr is the CHD implementation boundary. It carries MAME's BSD-3-Clause
# CHD reader and its permissively licensed codec dependencies, so the core does
# not duplicate the format or codec implementations.
add_library(re2dj_chdr_lzma STATIC
    third_party/libchdr/deps/lzma-26.02/src/LzmaDec.c
)
target_include_directories(re2dj_chdr_lzma PRIVATE
    third_party/libchdr/deps/lzma-26.02/include
)

add_library(re2dj_chdr_miniz STATIC
    third_party/libchdr/deps/miniz-3.1.2/miniz.c
)
target_include_directories(re2dj_chdr_miniz PRIVATE
    third_party/libchdr/deps/miniz-3.1.2
)
target_compile_definitions(re2dj_chdr_miniz PUBLIC
    MINIZ_NO_ARCHIVE_APIS
    MINIZ_NO_DEFLATE_APIS
    MINIZ_NO_STDIO
    MINIZ_NO_TIME
)

add_library(re2dj_chdr_zstd STATIC
    third_party/libchdr/deps/zstd-1.5.7/zstddeclib.c
)
target_include_directories(re2dj_chdr_zstd PRIVATE
    third_party/libchdr/deps/zstd-1.5.7
)

add_library(re2dj_chdr STATIC
    third_party/libchdr/src/libchdr_bitstream.c
    third_party/libchdr/src/libchdr_cdrom.c
    third_party/libchdr/src/libchdr_chd.c
    third_party/libchdr/src/libchdr_codec_avhuff.c
    third_party/libchdr/src/libchdr_codec_cdfl.c
    third_party/libchdr/src/libchdr_codec_cdlz.c
    third_party/libchdr/src/libchdr_codec_cdzl.c
    third_party/libchdr/src/libchdr_codec_cdzs.c
    third_party/libchdr/src/libchdr_codec_flac.c
    third_party/libchdr/src/libchdr_codec_huff.c
    third_party/libchdr/src/libchdr_codec_lzma.c
    third_party/libchdr/src/libchdr_codec_zlib.c
    third_party/libchdr/src/libchdr_codec_zstd.c
    third_party/libchdr/src/libchdr_flac.c
    third_party/libchdr/src/libchdr_huffman.c
)
target_include_directories(re2dj_chdr PUBLIC third_party/libchdr/include)
target_link_libraries(re2dj_chdr
    PRIVATE re2dj_chdr_lzma re2dj_chdr_miniz re2dj_chdr_zstd
)
target_compile_definitions(re2dj_chdr PRIVATE
    WANT_RAW_DATA_SECTOR=1
    WANT_SUBCODE=1
    VERIFY_BLOCK_CRC=1
    LOWRAM_TARGET=0
    _CRT_SECURE_NO_WARNINGS
)

add_library(re2dj_storage_common STATIC
    src/storage/guest_path.cpp
)
target_include_directories(re2dj_storage_common PUBLIC include)
target_link_libraries(re2dj_storage_common PRIVATE re2dj_warnings)

add_library(re2dj_logging STATIC
    src/logging/logging.cpp
)
target_include_directories(re2dj_logging PUBLIC include)
target_link_libraries(re2dj_logging
    PUBLIC spdlog::spdlog
    PRIVATE re2dj_warnings
)

add_library(re2dj_chd_storage STATIC
    src/storage/chd_hunk_cache.cpp
    src/storage/fat32_chd.cpp
    src/storage/fat32_directory_name.cpp
    src/storage/guest_find.cpp
    src/storage/mame_chd.cpp
)
target_include_directories(re2dj_chd_storage
    PUBLIC include
    PRIVATE third_party/libchdr/include
)
target_link_libraries(re2dj_chd_storage
    PUBLIC re2dj_chdr re2dj_storage_common
    PRIVATE re2dj_warnings
)

# The DirectX HLE's shared description of itself and its guest ABI, used by
# the Windows COM facade and the Linux gate facade alike.
add_library(re2dj_directx STATIC
    src/directx/direct3d_description.cpp
    src/directx/direct3d_device.cpp
    src/directx/direct3d_draw.cpp
    src/directx/direct3d_vertex_buffer.cpp
    src/directx/directdraw_description.cpp
    src/directx/directdraw_display.cpp
    src/directx/directdraw_surface.cpp
    src/directx/directinput.cpp
)
target_include_directories(re2dj_directx PUBLIC include)
target_link_libraries(re2dj_directx PUBLIC re2dj_legacy_graphics PRIVATE re2dj_warnings)

add_library(re2dj_legacy_graphics STATIC
    src/graphics/color_depth.cpp
    src/graphics/legacy_draw_command.cpp
    src/graphics/legacy_texture.cpp
    src/graphics/legacy_transform.cpp
    src/graphics/legacy_vertex_buffer.cpp
    src/graphics/present_interval_histogram.cpp
    src/graphics/present_sync.cpp
    src/graphics/present_pacer.cpp
    src/graphics/presentation_filter.cpp
    src/graphics/true_color.cpp
)
target_include_directories(re2dj_legacy_graphics PUBLIC include)
target_link_libraries(re2dj_legacy_graphics PRIVATE re2dj_warnings)

add_library(re2dj_sdl3_opengl_backend STATIC
    src/graphics/sdl3_opengl_backend.cpp
)
target_include_directories(re2dj_sdl3_opengl_backend PUBLIC include)
target_link_libraries(re2dj_sdl3_opengl_backend
    PUBLIC re2dj_legacy_graphics
    PRIVATE SDL3::SDL3-static re2dj_warnings
)

add_library(re2dj_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(re2dj_imgui SYSTEM PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
# The on-screen display. Platform-neutral: it takes input through its own
# queue and draws through the backend's present overlay point.
add_library(re2dj_osd STATIC
    src/ui/display_controls.cpp
    src/ui/osd.cpp
)
target_include_directories(re2dj_osd PUBLIC include)
target_link_libraries(re2dj_osd
    PUBLIC re2dj_legacy_graphics
    PRIVATE re2dj_imgui re2dj_warnings
)

# Platform-neutral core. Nothing here may include a host OS header; see the
# portability rules in docs/CODING_STYLE.md.
add_library(re2dj_core STATIC
    src/analysis/code_region_score.cpp
    src/audio/directsound_buffer_policy.cpp
    src/audio/directsound_device.cpp
    src/audio/legacy_audio_buffer.cpp
    src/config/hardlock_secret_config.cpp
    src/hle/hardlock/handshake_response.cpp
    src/hle/hardlock/api_descriptor.cpp
    src/hle/hardlock/protocol.cpp
    src/hle/hardlock/device.cpp
    src/hle/hardlock/device_call.cpp
    src/hle/hardlock/device_material.cpp
    src/hle/hardlock/engine.cpp
    src/hle/hardlock/payload_responses.cpp
    src/hle/hardlock/seed_config.cpp
    src/hle/hardlock/transform_responses.cpp
    src/hle/guest_device_path.cpp
    src/hle/api_call_record.cpp
    src/hle/guest_devices.cpp
    src/hle/guest_files.cpp
    src/hle/private_profile.cpp
    src/hle/wsprintf.cpp
    src/hle/guest_heap.cpp
    src/hle/guest_com.cpp
    src/hle/guest_process.cpp
    src/hle/gdi_raster.cpp
    src/hle/bitmap_file.cpp
    src/hle/guest_gdi.cpp
    src/hle/guest_font.cpp
    src/hle/guest_mixer.cpp
    src/hle/guest_user.cpp
    src/hle/win32_time.cpp
    src/hle/import_dispatcher.cpp
    src/hle/import_event_loop.cpp
    src/hle/modules/advapi32_module.cpp
    src/hle/modules/guest_module.cpp
    src/hle/modules/guest_module_registry.cpp
    src/hle/modules/guest_pe_facade.cpp
    src/hle/modules/kernel32_module.cpp
    src/hle/modules/resolve_only_modules.cpp
    src/hle/modules/ddraw_device7.cpp
    src/hle/modules/ddraw_direct3d7.cpp
    src/hle/modules/ddraw_vertex_buffer7.cpp
    src/hle/modules/ddraw_direct_draw4.cpp
    src/hle/modules/ddraw_module.cpp
    src/hle/modules/ddraw_surface7.cpp
    src/hle/modules/ddraw_texture2.cpp
    src/hle/modules/dinput_module.cpp
    src/hle/modules/dsound_module.cpp
    src/hle/modules/facade_com.cpp
    src/hle/modules/gdi32_module.cpp
    src/hle/modules/gdi_bitmaps.cpp
    src/hle/modules/user32_module.cpp
    src/hle/modules/winmm_module.cpp
    src/hle/modules/wtsapi32_module.cpp
    src/device/lptdi_challenge_response.cpp
    src/device/lptdi_response_profile.cpp
    src/input/ez2dj_io_board.cpp
    src/input/ez2dancer_io_board.cpp
    src/input/ez2dancer_io_port_bus.cpp
    src/input/legacy_io_port_bus.cpp
    src/input/legacy_io_trap.cpp
    src/input/virtual_keys.cpp
    src/input/ez2dj_keyboard_map.cpp
    src/input/ez2dancer_keyboard_map.cpp
    src/exe/code_scan.cpp
    src/exe/immediate_scan.cpp
    src/exe/pe_image.cpp
    src/hdd/hdd_root.cpp
    src/hdd/hdd_scan.cpp
    src/runtime/address_space.cpp
    src/runtime/pe_loader.cpp
    src/storage/vfs_roots.cpp
    src/storage/vfs_file_table.cpp
    src/target/target_profile.cpp
    src/version.cpp
)

target_include_directories(re2dj_core
    PUBLIC
        include
    PRIVATE
        third_party/libchdr/include
)

target_compile_definitions(re2dj_core
    PRIVATE
        RE2DJ_VERSION="${RE2DJ_VERSION}"
)

target_link_libraries(re2dj_core
    PUBLIC re2dj_directx re2dj_legacy_graphics re2dj_chd_storage
    PRIVATE re2dj_logging re2dj_warnings
)

if(RE2DJ_SDL3_AUDIO_ACTIVE)
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

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND RE2DJ_SDL3_AUDIO_ACTIVE)
    # The guest's DirectSound plays through the backend the Windows facade uses.
    target_sources(re2dj_linux_native_backend PRIVATE
        src/audio/sdl3_mixer_audio_backend.cpp
        src/platform/linux/host_audio.cpp
    )
    target_include_directories(re2dj_linux_native_backend PRIVATE src/audio)
    target_link_libraries(re2dj_linux_native_backend PRIVATE SDL3_mixer::SDL3_mixer-static)
    target_compile_definitions(re2dj_linux_native_backend PUBLIC RE2DJ_LINUX_HOST_AUDIO=1)
    if(RE2DJ_BUILD_TESTS)
        add_executable(re2dj_audio_stream_progress_test
            tests/audio/stream_progress_test.cpp
            src/audio/directsound_device.cpp
            src/audio/legacy_audio_buffer.cpp
            src/audio/sdl3_mixer_audio_backend.cpp
        )
        target_include_directories(re2dj_audio_stream_progress_test PRIVATE include src/audio)
        target_link_libraries(re2dj_audio_stream_progress_test PRIVATE
            SDL3_mixer::SDL3_mixer-static SDL3::SDL3-static)
        add_test(NAME re2dj_audio_stream_progress_test COMMAND re2dj_audio_stream_progress_test)
        set_tests_properties(re2dj_audio_stream_progress_test PROPERTIES
            ENVIRONMENT "SDL_AUDIODRIVER=dummy" TIMEOUT 15)
    endif()
endif()

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)

    add_library(re2dj_windows_injected_runtime SHARED
        src/hle/hardlock/api_descriptor.cpp
        src/hle/hardlock/protocol.cpp
        src/hle/hardlock/device.cpp
        src/hle/hardlock/device_call.cpp
        src/hle/guest_device_path.cpp
        src/hle/private_profile.cpp
        src/hle/hardlock/engine.cpp
        src/hle/hardlock/payload_responses.cpp
        src/hle/hardlock/transform_responses.cpp
        src/device/lptdi_challenge_response.cpp
        src/input/ez2dj_io_board.cpp
        src/input/ez2dancer_io_board.cpp
        src/input/ez2dancer_io_port_bus.cpp
        src/input/legacy_io_port_bus.cpp
        src/input/legacy_io_trap.cpp
        src/input/virtual_keys.cpp
        src/input/ez2dj_keyboard_map.cpp
        src/input/ez2dancer_keyboard_map.cpp
        src/platform/windows/audio_volume_trace.cpp
        src/platform/windows/direct3d3_com_facade.cpp
        src/platform/windows/direct3d7_com_facade.cpp
        src/platform/windows/direct3d7_vertex_buffer_facade.cpp
        src/platform/windows/directdraw7_com_facade.cpp
        src/platform/windows/directinput7_com_facade.cpp
        src/platform/windows/display_mode_boundary.cpp
        src/platform/windows/ez2dancer_keyboard_input.cpp
        src/platform/windows/ez2dj_keyboard_input.cpp
        src/platform/windows/game_controls.cpp
        src/platform/windows/graphics_trace_log.cpp
        src/platform/windows/host_window_shell.cpp
        src/platform/windows/ini_profile_hle.cpp
        src/platform/windows/guest_wait_accounting.cpp
        src/platform/windows/injected_runtime.cpp
        src/platform/windows/keyboard_input_common.cpp
        src/platform/windows/message_box_boundary.cpp
        src/platform/windows/osd_host.cpp
        src/platform/windows/runtime_log.cpp
        src/platform/windows/timer_resolution_probe.cpp
        src/platform/windows/window_mode.cpp
        src/platform/windows/winmm_mixer_trace.cpp
    )
    target_include_directories(re2dj_windows_injected_runtime PRIVATE include)
    target_compile_definitions(re2dj_windows_injected_runtime PRIVATE
        RE2DJ_VERSION="${RE2DJ_VERSION}"
    )
    target_link_libraries(re2dj_windows_injected_runtime
        PRIVATE re2dj_directx re2dj_sdl3_opengl_backend re2dj_osd re2dj_chd_storage spdlog::spdlog re2dj_warnings dwmapi dxguid winmm
    )
    if(RE2DJ_ENABLE_SDL3_AUDIO)
        target_sources(re2dj_windows_injected_runtime PRIVATE
            src/audio/directsound_buffer_policy.cpp
            src/audio/directsound_device.cpp
            src/audio/legacy_audio_buffer.cpp
            src/audio/sdl3_mixer_audio_backend.cpp
            src/platform/windows/directsound_com_facade.cpp
        )
        target_link_libraries(re2dj_windows_injected_runtime
            PRIVATE SDL3_mixer::SDL3_mixer-static SDL3::SDL3-static
        )
        if(RE2DJ_BUILD_TESTS)
            add_executable(re2dj_audio_stream_progress_test
                tests/audio/stream_progress_test.cpp
                src/audio/directsound_device.cpp
                src/audio/legacy_audio_buffer.cpp
                src/audio/sdl3_mixer_audio_backend.cpp
            )
            target_include_directories(re2dj_audio_stream_progress_test PRIVATE include src/audio)
            target_link_libraries(re2dj_audio_stream_progress_test PRIVATE
                SDL3_mixer::SDL3_mixer-static SDL3::SDL3-static)
            add_test(NAME re2dj_audio_stream_progress_test COMMAND re2dj_audio_stream_progress_test)
            set_tests_properties(re2dj_audio_stream_progress_test PROPERTIES
                ENVIRONMENT "SDL_AUDIODRIVER=dummy" TIMEOUT 15)
        endif()
    endif()
    if(RE2DJ_BUILD_TESTS)
        add_executable(re2dj_windows_vfs_runtime_probe
            src/tools/windows_vfs_runtime_probe/main.cpp
        )
        target_include_directories(re2dj_windows_vfs_runtime_probe PRIVATE include)
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

        add_executable(re2dj_ez2dj_keyboard_input_test
            tests/unit/ez2dj_keyboard_input_test.cpp
            src/platform/windows/ez2dj_keyboard_input.cpp
            src/platform/windows/keyboard_input_common.cpp
            tests/unit/temporary_tree.cpp
        )
        target_include_directories(re2dj_ez2dj_keyboard_input_test PRIVATE include tests/unit .)
        target_compile_definitions(re2dj_ez2dj_keyboard_input_test PRIVATE
            RE2DJ_VERSION="${RE2DJ_VERSION}"
        )
        target_link_libraries(re2dj_ez2dj_keyboard_input_test
            PRIVATE re2dj_core re2dj_warnings
        )
        add_test(NAME re2dj_ez2dj_keyboard_input_test COMMAND re2dj_ez2dj_keyboard_input_test)

        add_executable(re2dj_ez2dancer_keyboard_input_test
            tests/unit/ez2dancer_keyboard_input_test.cpp
            src/platform/windows/ez2dancer_keyboard_input.cpp
            src/platform/windows/keyboard_input_common.cpp
            tests/unit/temporary_tree.cpp
        )
        target_include_directories(re2dj_ez2dancer_keyboard_input_test PRIVATE include tests/unit .)
        target_compile_definitions(re2dj_ez2dancer_keyboard_input_test PRIVATE
            RE2DJ_VERSION="${RE2DJ_VERSION}"
        )
        target_link_libraries(re2dj_ez2dancer_keyboard_input_test
            PRIVATE re2dj_core re2dj_warnings
        )
        add_test(NAME re2dj_ez2dancer_keyboard_input_test COMMAND re2dj_ez2dancer_keyboard_input_test)
    endif()
    add_library(re2dj_windows_original_process_backend STATIC
        src/tools/windows_original_process_probe/iat_verifier.cpp
        src/platform/windows/injected_runtime_loader.cpp
        src/platform/windows/original_process_backend.cpp
        src/platform/windows/process_image_dump.cpp
        src/platform/windows/runtime_export_locator.cpp
        src/tools/windows_x86_launcher_probe/remote_module_exports.cpp
        src/tools/windows_x86_launcher_probe/null_context_object_state.cpp
        src/tools/windows_x86_launcher_probe/child_process_handoff.cpp
        src/tools/windows_x86_launcher_probe/main.cpp
    )
    target_include_directories(re2dj_windows_original_process_backend PUBLIC include)
    target_compile_definitions(re2dj_windows_original_process_backend PRIVATE
        RE2DJ_VERSION="${RE2DJ_VERSION}"
    )
    target_link_libraries(re2dj_windows_original_process_backend
        PRIVATE re2dj_core re2dj_logging re2dj_warnings
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
# libstdc++ 9+ or MSVC, which covers every supported host.

# Command-line host.
add_executable(re2dj
    src/host/cli/main.cpp
)
target_link_libraries(re2dj PRIVATE re2dj_core re2dj_logging re2dj_warnings)
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_link_libraries(re2dj PRIVATE re2dj_windows_original_process_backend)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(re2dj PRIVATE re2dj_linux_native_backend)
endif()

# Non-executing analysis tools.
add_executable(re2dj_hdd_probe
    src/tools/hdd_probe/main.cpp
)
target_link_libraries(re2dj_hdd_probe PRIVATE re2dj_core re2dj_warnings)

add_executable(re2dj_chd_probe
    src/tools/chd_probe/main.cpp
    src/tools/chd_probe/chd_extract.cpp
)
target_link_libraries(re2dj_chd_probe PRIVATE re2dj_core re2dj_warnings)

add_executable(re2dj_pe_analyzer
    src/tools/pe_analyzer/main.cpp
)
target_link_libraries(re2dj_pe_analyzer PRIVATE re2dj_core re2dj_warnings)

add_executable(re2dj_code_score
    src/tools/code_score/main.cpp
)
target_link_libraries(re2dj_code_score PRIVATE re2dj_core re2dj_warnings)

add_executable(re2dj_port_helper_scan
    src/tools/port_helper_scan/main.cpp
)
target_link_libraries(re2dj_port_helper_scan PRIVATE re2dj_core re2dj_warnings)

add_executable(re2dj_pe_loader
    src/tools/pe_loader/main.cpp
)
target_link_libraries(re2dj_pe_loader PRIVATE re2dj_core re2dj_warnings)

if(RE2DJ_BUILD_TESTS)
    enable_testing()
    add_executable(re2dj_unit_tests
        tests/unit/address_space_test.cpp
        tests/unit/code_region_score_test.cpp
        tests/unit/execution_backend_test.cpp
        tests/unit/import_dispatcher_test.cpp
        tests/unit/import_event_loop_test.cpp
        tests/unit/guest_module_registry_test.cpp
        tests/unit/guest_pe_facade_test.cpp
        tests/unit/kernel32_module_test.cpp
        tests/unit/user32_module_test.cpp
        tests/unit/ddraw_module_test.cpp
        tests/unit/dinput_module_test.cpp
        tests/unit/winmm_module_test.cpp
        tests/unit/gdi_raster_test.cpp
        tests/unit/dsound_module_test.cpp
        tests/unit/directx_display_test.cpp
        tests/unit/directx_device_test.cpp
        tests/unit/directx_input_test.cpp
        tests/unit/directx_surface_test.cpp
        tests/unit/version_test.cpp
        tests/unit/guest_process_test.cpp
        tests/unit/kernel32_crt_test.cpp
        tests/unit/api_call_record_test.cpp
        tests/unit/guest_files_test.cpp
        tests/unit/logging_test.cpp
        tests/unit/ez2dj_io_board_test.cpp
        tests/unit/ez2dancer_io_board_test.cpp
        tests/unit/guest_path_test.cpp
        tests/unit/hardlock_material_config_test.cpp
        tests/unit/hardlock_handshake_response_test.cpp
        tests/unit/hardlock_protocol_test.cpp
        tests/unit/hardlock_api_descriptor_test.cpp
        tests/unit/hardlock_device_test.cpp
        tests/unit/hardlock_device_material_test.cpp
        tests/unit/hardlock_payload_responses_test.cpp
        tests/unit/hardlock_transform_responses_test.cpp
        tests/unit/hardlock_engine_test.cpp
        tests/unit/code_scan_test.cpp
        tests/unit/port_helper_scan_test.cpp
        tests/unit/immediate_scan_test.cpp
        tests/unit/hdd_root_test.cpp
        tests/unit/main.cpp
        tests/unit/lptdi_challenge_response_test.cpp
        tests/unit/bitmap_file_test.cpp
        tests/unit/lptdi_response_profile_test.cpp
        tests/unit/legacy_io_port_bus_test.cpp
        tests/unit/legacy_io_trap_test.cpp
        tests/unit/legacy_draw_command_test.cpp
        tests/unit/presentation_filter_test.cpp
        tests/unit/legacy_texture_test.cpp
        tests/unit/true_color_test.cpp
        tests/unit/legacy_transform_test.cpp
        tests/unit/legacy_vertex_buffer_test.cpp
        tests/unit/directsound_buffer_policy_test.cpp
        tests/unit/directsound_device_test.cpp
        tests/unit/legacy_audio_buffer_test.cpp
        tests/unit/mame_chd_test.cpp
        tests/unit/fat32_chd_test.cpp
        tests/unit/chd_hunk_cache_test.cpp
        tests/unit/fat32_directory_name_test.cpp
        tests/unit/chd_extract_name_test.cpp
        src/tools/chd_probe/chd_extract.cpp
        tests/unit/pe_image_test.cpp
        tests/unit/pe_loader_test.cpp
        tests/unit/synthetic_pe32.cpp
        tests/unit/temporary_tree.cpp
        tests/unit/present_interval_histogram_test.cpp
        tests/unit/present_sync_test.cpp
        tests/unit/window_policy_test.cpp
        tests/unit/ez2dj_keyboard_map_test.cpp
        tests/unit/guest_find_test.cpp
        tests/unit/target_profile_test.cpp
        tests/unit/vfs_file_table_test.cpp
    )
    target_include_directories(re2dj_unit_tests PRIVATE tests/unit src/tools/chd_probe)
    target_link_libraries(re2dj_unit_tests PRIVATE re2dj_core re2dj_logging re2dj_warnings)
    # Where the tests find repository files such as config/ examples.
    target_compile_definitions(re2dj_unit_tests PRIVATE RE2DJ_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME re2dj_unit_tests COMMAND re2dj_unit_tests)
    if(TARGET re2dj_linux_native_guest_module_probe)
        add_test(NAME re2dj_linux_native_guest_module_probe
                 COMMAND re2dj_linux_native_guest_module_probe)
    endif()
    if(TARGET re2dj_linux_native_in_process_probe)
        add_test(NAME re2dj_linux_native_in_process_probe
                 COMMAND re2dj_linux_native_in_process_probe)
    endif()
    if(TARGET re2dj_linux_compat_mode_probe)
        add_test(NAME re2dj_linux_compat_mode_probe
                 COMMAND re2dj_linux_compat_mode_probe)
    endif()

    # Run explicitly on a desktop with an OpenGL driver, not in headless CTest.
    if(TARGET re2dj_sdl3_opengl_backend)
        add_executable(re2dj_opengl_blend_probe tests/graphics/opengl_blend_probe.cpp)
        target_link_libraries(re2dj_opengl_blend_probe
            PRIVATE re2dj_sdl3_opengl_backend SDL3::SDL3-static re2dj_warnings
        )
        add_executable(re2dj_opengl_true_color_probe tests/graphics/opengl_true_color_probe.cpp)
        target_link_libraries(re2dj_opengl_true_color_probe
            PRIVATE re2dj_sdl3_opengl_backend SDL3::SDL3-static re2dj_warnings
        )
    endif()
endif()
