# 작업 447 작업 로그 — SDL host 공용화 / Task 447 work log — sharing the SDL hosts

설계: [작업 446 설계](../design/20261004-446-windows-in-process-loader.md) · 지시서: [20261004-447-sdl-hosts-shared.md](../work-orders/20261004-447-sdl-hosts-shared.md)

## 2026-10-04

- **이동**: `host_presentation.cpp`, `host_audio.cpp`, `host_keyboard.{h,cpp}` → `src/platform/sdl/`, `host_presentation.h`·`host_audio.h` → `include/re2dj/platform/sdl/`. 세 파일 모두 SDL3·SDL3_mixer와 공용 헤더만 쓰며 OS 헤더가 없다는 것을 먼저 확인했다.
- **이름**: namespace `re2dj::platform::sdl`, `LinuxHostPresentation` → `SdlHostPresentation`, `LinuxHostAudio` → `SdlHostAudio`, `RE2DJ_LINUX_HOST_AUDIO` → `RE2DJ_SDL_HOST_AUDIO`. CLI는 `sdl_platform::`을 쓴다. 전역 변수 이름(`g_linux_presentation` 등)은 449에서 CLI를 두 OS 공용으로 정리할 때 바꾼다.
- **그대로 둔 것**: 자식 프로세스 런처(`posix_spawn`)는 Linux 전용이라 `src/platform/linux/`에 남는다. Windows 구현은 449에서 만든다.
- **문서**: `src/platform/sdl/README.md`(새로), Linux README, ARCHITECTURE(계층 표에 SDL host 행, 클래스 이름), AGENTS.md(디렉터리 규칙).
- **검증**: WSL `linux-x64-debug`(경고를 오류로) configure·build 성공, CTest 5개 통과. `re2dj ez2dj6th` 15초 실행: 런처·자식 두 번의 `io config`, `presentation: 16-bit colour`, `gamepads ready`, 창을 닫을 때 `host window closed`와 `ExitProcess(0)`. 이 단계에서 Windows는 SDL host를 빌드하지 않으므로 Windows build는 446과 같다.

  *Moved `host_presentation.cpp`, `host_audio.cpp` and `host_keyboard.{h,cpp}` to `src/platform/sdl/` and `host_presentation.h` and `host_audio.h` to `include/re2dj/platform/sdl/`, after confirming all three use only SDL3, SDL3_mixer and shared headers. Renamed: namespace `re2dj::platform::sdl`, `SdlHostPresentation`, `SdlHostAudio`, `RE2DJ_SDL_HOST_AUDIO`; the CLI uses `sdl_platform::`, and its globals (`g_linux_presentation` and so on) are renamed in 449 when the CLI becomes shared by both OSes. The child launcher (`posix_spawn`) stays Linux-only in `src/platform/linux/`, its Windows counterpart coming in 449. Documents: the new `src/platform/sdl/README.md`, the Linux README, ARCHITECTURE (an SDL host row in the layer table, class names) and AGENTS.md (directory rules). Verification: the WSL `linux-x64-debug` build (warnings as errors) and 5 CTest tests pass; a 15-second `re2dj ez2dj6th` run logs `io config` twice (launcher and child), `presentation: 16-bit colour` and `gamepads ready`, and ends with `host window closed` and `ExitProcess(0)` when the window closes. Windows builds no SDL host in this phase, so its build is as in 446.*
