# 작업 447 작업 지시서 — SDL host 공용화 / Task 447 work order — sharing the SDL hosts

설계: [작업 446 설계의 2단계](../design/20261004-446-windows-in-process-loader.md)

## 절차 / Steps

1. `host_presentation`, `host_audio`, `host_keyboard`를 `src/platform/linux/`에서 `src/platform/sdl/`로, 공개 헤더를 `include/re2dj/platform/sdl/`로 `git mv`한다.
   *`git mv` `host_presentation`, `host_audio` and `host_keyboard` from `src/platform/linux/` to `src/platform/sdl/`, and their public headers to `include/re2dj/platform/sdl/`.*
2. namespace `re2dj::platform::sdl`, 클래스 `SdlHostPresentation`·`SdlHostAudio`, 매크로 `RE2DJ_SDL_HOST_AUDIO`로 바꾸고 CLI와 CMake를 맞춘다.
   *Switch to namespace `re2dj::platform::sdl`, classes `SdlHostPresentation` and `SdlHostAudio`, macro `RE2DJ_SDL_HOST_AUDIO`, and align the CLI and CMake.*
3. `src/platform/sdl/README.md`, Linux README, ARCHITECTURE, AGENTS.md 디렉터리 규칙을 갱신한다.
   *Update `src/platform/sdl/README.md`, the Linux README, ARCHITECTURE and AGENTS.md's directory rules.*
4. 검증: Linux x64 build·CTest(경고를 오류로), 6th 실행. Windows는 이 파일들을 아직 빌드하지 않는다(448에서 빌드).
   *Verify: the Linux x64 build and CTest (warnings as errors) and a 6th run; Windows does not build these files yet (448 will).*
