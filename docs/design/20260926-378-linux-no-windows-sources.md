# 작업 378 설계 — Linux가 Windows 소스를 참조하지 않게 / Task 378 design — Linux references no Windows sources

## 배경 / Background

사용자가 원칙을 다시 확인했다. Linux 코드와 Linux build target은 `src/platform/windows/`의 어떤 파일도 참조하면 안 된다. 저장소를 점검하니 Linux probe 3개가 Windows 소스를 `#include`해서 컴파일하고 있었다(작업 346 무렵과 357에서 들어옴). 이 probe는 `re2dj_linux_native_ipc_host_probe`와 두 폭의 `re2dj_linux_native_in_process_probe`다. 포함되던 파일은 `src/platform/windows/native_ipc_host_probe.cpp`였다. 이 파일 안에는 합성 PE32 fixture와 Linux 전용 `#if RE2DJ_LINUX_NATIVE_IPC_HOST_PROBE` 분기가 있었고, Linux 쪽은 `windows` namespace 별칭, `#define wmain`, header guard 선점으로 이 파일을 끼워 넣었다. 이 Windows 파일 자체는 현재 어떤 Windows target도 빌드하지 않는다.

*The user restated the rule: Linux code and Linux build targets must not reference any file under `src/platform/windows/`. An audit found three Linux probes (`re2dj_linux_native_ipc_host_probe` and both widths' `re2dj_linux_native_in_process_probe`, from around Task 346 and Task 357) compiling `src/platform/windows/native_ipc_host_probe.cpp` through `#include`. That file held the synthetic PE32 fixture and Linux-only `#if RE2DJ_LINUX_NATIVE_IPC_HOST_PROBE` branches, and the Linux side pulled it in with a `windows` namespace alias, `#define wmain`, and a pre-empted header guard. No Windows target builds the file itself any more.*

점검 결과 나머지 참조는 문제가 없다. CLI의 `re2dj/platform/windows/original_process_backend.h` include는 `#elif defined(_WIN32)` 안에 있다. 키보드 입력 테스트 2개는 Windows 전용 target이다.

*The other references are fine: the CLI includes `re2dj/platform/windows/original_process_backend.h` inside `#elif defined(_WIN32)`, and the two keyboard-input tests are Windows-only targets.*

## 결정 / Decisions

1. **공용 fixture.** 합성 PE32(`MakeSyntheticPe32`), 그 상수, `ProbeImportHandler`를 `src/platform/native_probe_fixture.h/.cpp`로 옮긴다. host-neutral platform helper(`native_helper_protocol.h`)와 같은 층이다. namespace는 `re2dj::platform::native_probe`다.
   ***Shared fixture:** the synthetic PE32 (`MakeSyntheticPe32`), its constants, and `ProbeImportHandler` move to `src/platform/native_probe_fixture.h/.cpp`, beside the host-neutral `native_helper_protocol.h`, in `re2dj::platform::native_probe`.*
2. **Linux IPC host probe.** Windows `wmain`을 빌리지 않는다. 기본 실행 흐름(`RunBaselineProbe`)과 guest memory 검사(`GuestMemoryChecks`)를 Linux 파일에 둔다. 인자는 `char**`로 받으므로 wide 문자열 변환도 없앤다. 모드(기본, `--reject`, `--stop`, 그 뒤의 fault 실행)와 종료 코드는 전과 같다.
   ***Linux IPC host probe:** no longer borrows Windows's `wmain`; its baseline run (`RunBaselineProbe`) and guest memory checks (`GuestMemoryChecks`) live in the Linux file, taking `char**` arguments without the wide-string conversion. The modes (baseline, `--reject`, `--stop`, and the fault run after the baseline) and exit codes are as before.*
3. **Linux in-process probe.** 공용 fixture만 include한다.
   ***Linux in-process probes:** include only the shared fixture.*
4. **Windows 파일.** 공용 fixture를 쓰고 Linux 분기를 지운다. 이 파일을 빌드하는 target은 없다. 지울지는 사용자가 정한다.
   ***The Windows file:** uses the shared fixture and loses the Linux branches; no target builds it, and whether to delete it is the user's call.*
