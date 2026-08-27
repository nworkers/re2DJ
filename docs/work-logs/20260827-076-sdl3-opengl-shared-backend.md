# SDL3/OpenGL 공용 백엔드 작업 로그

## 결과

Windows 전용 `Direct3d3OpenGlBackend`와 WGL context 관리 코드를 제거하고 `Sdl3OpenGlBackend`를 공용 graphics 경계로 추가했다. 공용 backend는 SDL3 video subsystem과 window property를 사용한다. Windows에서는 원본이 만든 HWND를 `SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER`로 감싸며, native handle이 없는 Linux/Web 경로에서는 SDL window를 생성할 수 있다.

OpenGL 함수는 모두 `SDL_GL_GetProcAddress`로 해석한다. draw command, texture identity/revision cache, RGB565 변환, color-key discard, alpha test·blend·filter 의미는 유지했다. drawable 크기는 `SDL_GetWindowSizeInPixels`, present는 `SDL_GL_SwapWindow`를 사용한다. desktop shader는 GLSL 1.20, Web 분기는 GLSL ES 1.00과 OpenGL ES 2.0 context를 요청한다. injected runtime의 import dependency에서 `opengl32.dll`은 사라졌다.

플랫폼 중립 draw/texture/vertex-buffer 구현을 `re2dj_legacy_graphics`로 분리하고, `re2dj_core`, SDL backend와 Windows runtime이 같은 library를 사용하도록 했다. SDL3 video는 Win32·Linux·Web 기본 구성에서 활성화하고, SDL_mixer fetch와 SDL audio는 Windows x86 runtime 범위에 유지했다. Linux i386 helper 전용 구성은 SDL3를 가져오지 않는다.

Windows x64 configure/build/test preset, x64 CI job, x64 host/helper 실행·staging script와 x64 전용 CMake product target을 제거했다. Windows CI는 `-A Win32`로 실제 injected runtime을 빌드한다. 64비트 Windows host 지원 방식은 Win32 runtime의 WOW64 실행으로 유지한다. Linux CI에는 SDL3 desktop build용 X11·Wayland·OpenGL 개발 패키지를 명시했다.

후속 Linux CTest에서 드러난 overlay 읽기 대소문자 회귀를 수정했다. overlay도 원본 HDD와 같은 Win32 경로 의미를 따르도록 각 구성요소에서 exact match를 우선하고 없으면 ASCII case-insensitive match를 사용한다. Linux i386 helper preset은 C compiler ABI 검사도 32비트가 되도록 `CMAKE_C_FLAGS=-m32`를 추가했다.

## 검증

- Windows Win32 `RE2DJ_WARNINGS_AS_ERRORS=ON` configure와 전체 build 성공.
- Windows CTest 2/2 성공: `re2dj_windows_vfs_runtime_probe`, `re2dj_unit_tests`.
- `dumpbin /DEPENDENTS`로 `re2dj_windows_injected_runtime.dll`에 `opengl32.dll` 또는 SDL shared-library dependency가 없음을 확인했다.
- WSL Ubuntu 24.04 일반 desktop 구성에서 SDL3의 X11, Wayland, OpenGL과 OpenGL ES driver가 모두 활성화됐고, warnings-as-errors 전체 build가 성공했다. 선택적인 `libdecor` 부재 경고는 GNOME/Weston client-side decoration에만 해당하며 X11·Wayland backend 생성과 컴파일에는 영향이 없었다.
- overlay 대소문자 해석 수정 후 Linux `re2dj_unit_tests` 366 checks와 CTest 1/1이 통과했다.
- Linux i386 preset의 C/C++ ABI 검사가 모두 `-m32`로 통과했고 helper가 `ELF 32-bit Intel 80386`으로 빌드됐다. x86-64 host/i386 helper IPC probe는 `result=51`, `child=0`으로 성공했다.
- Web preset은 이 host에 `EMSDK`와 Emscripten compiler가 없어 실행하지 못했다. Web CI가 같은 backend target을 포함하며, GLSL ES/OpenGL ES 분기는 source와 CMake target에 반영했다.
- 원본 HDD 자산은 읽거나 수정하지 않았다. 실제 원본 실행의 화면 출력과 SDL external-HWND present 정확성은 사용자 runtime 재검증 대상으로 남긴다.

## 회고

기존 backend는 draw 입력만 중립적이고 context·함수 loader·present는 WGL에 고정되어 있었다. SDL window 경계를 backend 내부로 옮기면서 COM facade에는 HWND 전달과 HRESULT orchestration만 남았고, Linux/Web build가 같은 rendering source의 플랫폼 의존성 회귀를 검출할 수 있게 됐다. Linux desktop SDK와 Emscripten SDK는 source portability와 별개의 host prerequisite이므로 CI와 반복 가이드에서 명시적으로 관리한다.

---

# SDL3/OpenGL Shared Backend Work Log

## Result

Removed the Windows-only `Direct3d3OpenGlBackend` and WGL context code, replacing them with a shared `Sdl3OpenGlBackend`. SDL3 owns video initialization and window properties. Windows wraps the original HWND through `SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER`; Linux and Web can create an SDL window when no native handle is supplied.

All OpenGL entry points resolve through `SDL_GL_GetProcAddress`. Draw commands, texture identity/revision caching, RGB565 conversion, color-key discard, alpha testing, blending, and filtering retain their prior semantics. Drawable sizing uses `SDL_GetWindowSizeInPixels`, and presentation uses `SDL_GL_SwapWindow`. Desktop shaders use GLSL 1.20; the Web branch requests an OpenGL ES 2.0 context and GLSL ES 1.00. `opengl32.dll` is absent from the injected runtime's import dependencies.

Platform-neutral draw, texture, and vertex-buffer code now forms `re2dj_legacy_graphics`, shared by `re2dj_core`, the SDL backend, and the Windows runtime. SDL3 video is enabled for standard Win32, Linux, and Web builds. SDL_mixer fetching and SDL audio stay scoped to the Windows x86 runtime, while the Linux i386 helper-only configuration skips SDL3.

Removed Windows x64 configure/build/test presets, the x64 CI job, x64 host/helper execution and staging scripts, and x64-only CMake product targets. Windows CI now builds the actual injected runtime with `-A Win32`; a 64-bit Windows host runs it through WOW64. Linux CI installs the SDL3 desktop X11, Wayland, and OpenGL development packages.

Follow-up Linux CTest exposed and corrected overlay lookup case sensitivity. Overlay reads now preserve Win32 path semantics by preferring exact component matches and then falling back to ASCII case-insensitive matches, like original-HDD resolution. The Linux i386 helper preset now applies `CMAKE_C_FLAGS=-m32` so its C compiler ABI check is also 32-bit.

## Verification

The warnings-as-errors Win32 configure and full build pass, followed by CTest 2/2. `dumpbin /DEPENDENTS` confirms no `opengl32.dll` or shared SDL dependency. A normal WSL Ubuntu 24.04 desktop configuration enables SDL3's X11, Wayland, OpenGL, and OpenGL ES drivers and completes the warnings-as-errors build. The missing optional `libdecor` package only disables GNOME/Weston client-side decorations. After correcting overlay case-insensitive lookup, all 366 Linux unit checks and CTest 1/1 pass. Both C and C++ i386 ABI checks pass with `-m32`; the helper is an Intel 80386 ELF, and its x86-64 host integration probe completes with result 51 and child exit 0. The host has no Emscripten SDK, so the Web preset could not run locally; Web CI includes the same backend target and the source contains the GLSL ES/OpenGL ES branch. No original HDD assets were read or modified. Original-runtime visual output and external-HWND presentation remain user revalidation items.
