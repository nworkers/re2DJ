# 작업 444 작업 지시서 — 게임패드 입력 / Task 444 work order — gamepad input

설계: [20261003-444-gamepad-input.md](../design/20261003-444-gamepad-input.md)

## 절차 / Steps

1. 공용 코어: `input/gamepad.h`(컨트롤 번호·이름·파서), 바인딩 표의 `default_gamepad`, `input/io_bindings.h`(풀린 바인딩과 INI 본문 로더), `hle::HostInputState::gamepad`.
   *Shared core: `input/gamepad.h` (control numbers, names, parser), `default_gamepad` in the binding tables, `input/io_bindings.h` (resolved bindings and the INI-text loader), `hle::HostInputState::gamepad`.*
2. 예제 INI 두 개에 `[gamepad]` 섹션을 기본값으로 더한다.
   *Add a `[gamepad]` section at the defaults to both example INIs.*
3. SDL 빌드 옵션(`SDL_JOYSTICK`, `SDL_HIDAPI` 켬, `SDL_HIDAPI_LIBUSB` 끔)과 `re2dj_sdl3_gamepad` 라이브러리(`src/input/sdl3_gamepad_reader.cpp`).
   *The SDL build options (`SDL_JOYSTICK` and `SDL_HIDAPI` on, `SDL_HIDAPI_LIBUSB` off) and the `re2dj_sdl3_gamepad` library (`src/input/sdl3_gamepad_reader.cpp`).*
4. Linux 연결: `LinuxHostPresentation`의 reader 소유·이벤트·`Present` 뒤 읽기, `native_legacy_io`의 바인딩 수용과 키·패드 OR, `OriginalRunEnvironment::io_bindings`, CLI의 Linux `--io-config` 수용과 로더 호출.
   *The Linux wiring: the reader owned by `LinuxHostPresentation` with its events and the read after `Present`, `native_legacy_io` taking the bindings and OR-ing key and pad, `OriginalRunEnvironment::io_bindings`, and the CLI accepting `--io-config` on Linux and calling the loader.*
5. 테스트: `re2dj_unit_tests`의 게임패드 이름·기본값·로더 검사, SDL 가상 조이스틱으로 reader를 검사하는 `re2dj_sdl3_gamepad_test`.
   *Tests: gamepad names, defaults and the loader in `re2dj_unit_tests`, and `re2dj_sdl3_gamepad_test` driving the reader with an SDL virtual joystick.*
6. 빌드 패키지 목록(`ci.yml`, `release.yml`, Linux 빌드 가이드)에 `libudev-dev`.
   *`libudev-dev` in the build package lists (`ci.yml`, `release.yml`, the Linux build guide).*
7. 문서: README, ARCHITECTURE, `docs/kb/sdl3-gamepad-input.md`와 kb 색인, 작업 로그.
   *Documents: README, ARCHITECTURE, `docs/kb/sdl3-gamepad-input.md` with the kb index, and the work log.*
8. 검증: WSL `linux-x64-debug` build·CTest(경고를 오류로), `linux-x64-release` build와 `package_release.sh`, Windows x86 build와 키보드 입력 테스트.
   *Verification: the WSL `linux-x64-debug` build and CTest with warnings as errors, the `linux-x64-release` build with `package_release.sh`, and the Windows x86 build with the keyboard input tests.*

## 완료 조건 / Done when

Linux와 Windows build·테스트가 통과하고, Linux 릴리스 패키지의 NEEDED가 변하지 않으며, 실제 패드 확인은 사용자 몫으로 작업 로그에 적혀 있다.

*The Linux and Windows builds and tests pass, the Linux release package's NEEDED is unchanged, and the real-pad check is recorded in the work log as the user's.*
