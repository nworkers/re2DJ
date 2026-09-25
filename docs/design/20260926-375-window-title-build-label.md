# 작업 375 설계 — 창 제목의 빌드 표시 / Task 375 design — build label in the window title

## 배경 / Background

창 제목은 `re2DJ v<version> - Build <date> - SDL3 OpenGL - FPS : <value>`였다. 같은 버전이라도 어느 host(OS·아키텍처)용 빌드인지, Debug인지 Release인지는 보이지 않았다. 사용자가 이 둘을 제목에 넣기로 했다(예: `re2DJ (Win/x64 Debug)`, `re2DJ (Linux/x86 Release)`, `re2DJ (Linux/x64 Debug)`).

*The window title was `re2DJ v<version> - Build <date> - SDL3 OpenGL - FPS : <value>`, which did not show which host (OS and architecture) a build targets or whether it is Debug or Release. The user asked for both in the title (for example `re2DJ (Win/x64 Debug)`, `re2DJ (Linux/x86 Release)`, `re2DJ (Linux/x64 Debug)`).*

## 결정 / Decisions

1. **형식.** 버전 뒤에 괄호로 넣는다: `re2DJ v0.0.52 (Win/x86 Debug) - Build Sep 26 2026 - SDL3 OpenGL - FPS : 60.0`. OS는 `Win`·`Linux`, 아키텍처는 `x86`·`x64`, 빌드 형식은 CMake 구성 이름(`Debug`, `Release`, …)이다.
   ***Format:** in parentheses after the version: `re2DJ v0.0.52 (Win/x86 Debug) - Build Sep 26 2026 - SDL3 OpenGL - FPS : 60.0`. The OS is `Win` or `Linux`, the architecture `x86` or `x64`, and the build type the CMake configuration name (`Debug`, `Release`, ...).*
2. **값의 출처.** 모두 빌드된 binary 자신에서 온다. OS·아키텍처는 compiler macro(`_WIN32`/`__linux__`, `_M_X64`/`__x86_64__`/`_M_IX86`/`__i386__`)로 정한다. 빌드 형식은 CMake가 모든 target에 넘기는 `RE2DJ_BUILD_CONFIG="$<CONFIG>"`로 정한다. `$<CONFIG>`를 쓰므로 Visual Studio처럼 구성을 여러 개 두는 generator에서도 정확하다. CMake 밖에서 빌드하면 `NDEBUG`로 정한다. Windows 제목은 32비트 게임 프로세스에 주입된 x86 DLL이 붙이므로 지금은 `Win/x86`이다. x64로 빌드하면 저절로 `Win/x64`가 된다.
   ***Sources:** all from the built binary itself: the OS and architecture from compiler macros (`_WIN32`/`__linux__`, `_M_X64`/`__x86_64__`/`_M_IX86`/`__i386__`), the build type from `RE2DJ_BUILD_CONFIG="$<CONFIG>"`, which CMake passes to every target, so multi-configuration generators such as Visual Studio are right too (outside CMake, from `NDEBUG`). The Windows title is set by the x86 DLL injected into the 32-bit game, so it reads `Win/x86` today, and an x64 build would read `Win/x64` by itself.*
3. **공용 위치.** `re2dj/version.h`의 header-only `BuildLabel()`이다. core를 link하지 않는 Windows injected runtime도 같은 문자열을 쓴다. Linux는 아직 창이 없다. DirectX core 5단계에서 Linux가 창을 만들 때 같은 함수로 제목을 붙인다.
   ***Shared home:** the header-only `BuildLabel()` in `re2dj/version.h`, so the Windows injected runtime, which does not link the core, uses the same text. Linux has no window yet; it titles one with the same function when DirectX core phase 5 creates it.*

## 범위 밖 / Out of scope

- OSD의 버전 줄(`re2DJ v<version> <date>`)과 CLI 시작 log. / *The OSD version line (`re2DJ v<version> <date>`) and the CLI start-up log.*
