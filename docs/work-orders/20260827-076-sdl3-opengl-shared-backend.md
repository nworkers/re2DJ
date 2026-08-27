# SDL3/OpenGL 공용 백엔드 작업 지시

## 상태

**구현 완료.** Win32 build·CTest, X11·Wayland Linux desktop build·CTest와 Linux x86-64/i386 helper 통합 probe가 통과했다. Web SDK 제한을 포함한 상세 결과는 [작업 로그](../work-logs/20260827-076-sdl3-opengl-shared-backend.md)에 기록한다.

## 목표

Windows x64 빌드 설정을 제거하고, Win32 전용 WGL backend를 Win32·Linux·Web에서 빌드 가능한 SDL3/OpenGL 공용 backend로 교체한다.

## 작업

1. 공용 legacy graphics library와 SDL3/OpenGL backend target을 정의한다.
2. SDL3 video를 활성화하고 SDL_mixer fetch를 Win32 x86 audio 범위로 제한한다.
3. 기존 WGL backend의 shader, texture cache, fixed-function state 변환, draw와 present를 SDL3 context/API로 이전한다.
4. Windows COM facade가 원본 HWND를 공용 backend 초기화 설정으로 전달하도록 바꾼다.
5. Windows x64 preset·target·CI·전용 script를 제거하고 사용자 문서의 명령을 정리한다.
6. Win32, Linux, Web 빌드와 범위에 맞는 CTest를 실행한다.
7. 누적 아키텍처·구현 상태와 작업 로그를 갱신하고 하나의 작업 커밋을 남긴다.
8. Linux desktop 후속 검증에서 발견된 overlay 대소문자 회귀와 i386 preset의 C compiler bitness를 바로잡고 CTest/helper build를 재검증한다.

## 완료 기준

- `opengl32`, WGL과 Windows OpenGL backend source가 제품 빌드에서 제거된다.
- 같은 SDL3/OpenGL backend source가 Win32, Linux, Web 구성에 포함된다.
- Windows 기본 빌드와 CI가 Win32 runtime을 검증하며 별도 Windows x64 구성은 남지 않는다.
- 원본 HDD 자산 없이 수행 가능한 빌드·테스트 결과와 제한 사항이 작업 로그에 기록된다.

---

# SDL3/OpenGL Shared Backend Work Order

## Status

**Implementation complete.** The Win32 build/CTest, X11/Wayland Linux desktop build/CTest, and Linux x86-64/i386 helper integration probe pass. The [work log](../work-logs/20260827-076-sdl3-opengl-shared-backend.md) records the remaining Web SDK limitation.

## Goal

Remove the Windows x64 build configuration and replace the Win32-only WGL backend with an SDL3/OpenGL backend that builds for Win32, Linux, and Web.

## Tasks

1. Define a shared legacy-graphics library and SDL3/OpenGL backend target.
2. Enable SDL3 video and limit SDL_mixer fetching to the Win32 x86 audio scope.
3. Move the existing shaders, texture cache, fixed-function state conversion, drawing, and presentation from WGL to SDL3 context APIs.
4. Pass the original HWND from the Windows COM facade through the shared backend initialization contract.
5. Remove Windows x64 presets, targets, CI, and dedicated scripts, then update user-facing commands.
6. Run Win32, Linux, and Web builds plus applicable CTest suites.
7. Update cumulative architecture/status documents and the work log, then leave one task commit.
8. Correct the overlay case regression and i386 preset C-compiler bitness exposed by Linux desktop follow-up verification, then rerun CTest and the helper build.

## Completion Criteria

- `opengl32`, WGL, and the Windows OpenGL backend source are absent from product builds.
- The same SDL3/OpenGL backend source is included in Win32, Linux, and Web configurations.
- The default Windows build and CI validate the Win32 runtime, with no separate Windows x64 configuration remaining.
- Build/test results and limitations that require no original HDD assets are recorded in the work log.
