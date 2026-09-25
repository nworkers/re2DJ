# 작업 376 설계 — 모든 버전 표시를 한 머리말로 / Task 376 design — one banner for every version display

선행: [작업 375 설계](20260926-375-window-title-build-label.md)

## 배경 / Background

작업 375는 창 제목에만 빌드 표시(`Win/x86 Debug` 등)를 넣었다. OSD 버전 줄, CLI 시작 log, `--help` 첫 줄, `--version`, 진단 도구의 사용법 첫 줄은 여전히 버전만 보였다. 사용자가 이것들도 같이 맞추기로 했다.

*Task 375 put the build label (`Win/x86 Debug` and so on) in the window title only; the OSD version line, the CLI start-up log, the first `--help` line, `--version`, and the diagnostic tools' usage lines still showed the version alone. The user asked for them to match.*

## 결정 / Decisions

1. **머리말 하나.** `re2dj/version.h`의 header-only `VersionBanner(program, version)`가 `<program> v<version> (<BuildLabel()>)`를 만든다. 버전을 인자로 받는 이유는 Windows injected runtime에는 `VersionString()`이 없고 `RE2DJ_VERSION` macro만 있기 때문이다.
   ***One banner:** the header-only `VersionBanner(program, version)` in `re2dj/version.h` builds `<program> v<version> (<BuildLabel()>)`. The version is a parameter because the Windows injected runtime has only the `RE2DJ_VERSION` macro, not `VersionString()`.*
2. **쓰는 곳.** / ***Users:***
   - 창 제목: `re2DJ v0.0.52 (Win/x86 Debug) - Build <date> - SDL3 OpenGL - FPS : <value>` (형식은 그대로) / *the window title (format unchanged);*
   - OSD: `re2DJ v0.0.52 (Win/x86 Debug) - Build <date>`. 창 제목과 같은 앞부분이다 / *the OSD, with the title's leading part;*
   - CLI 시작 log: `re2DJ v0.0.52 (Linux/x64 Debug) starting` / *the CLI start-up log;*
   - `--help` 첫 줄과 `--version`: `re2DJ v0.0.52 (Linux/x64 Debug)`. `--version`을 읽는 script나 test는 없다(작업 365에서 확인) / *the first `--help` line and `--version`; nothing reads `--version`'s output (checked in Task 365);*
   - `re2dj_code_score`, `re2dj_hdd_probe`, `re2dj_pe_analyzer`, `re2dj_pe_loader`의 사용법 첫 줄 / *the usage lines of the four diagnostic tools.*
3. **남기는 것.** launcher 진단 JSON의 `re2dj_version` 값은 기계가 읽는 field라 버전만 둔다. SDL window의 초기 제목 `re2DJ`는 첫 `Re2djUpdateWindowTitle`이 바로 덮어쓴다.
   ***Left alone:** the launcher diagnostic JSON's `re2dj_version` stays the bare version, as a machine-read field, and the SDL window's initial `re2DJ` title is overwritten by the first `Re2djUpdateWindowTitle`.*
