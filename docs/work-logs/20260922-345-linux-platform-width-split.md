# 작업 345 작업 로그 — Linux 플랫폼 비트 폭 분리 / Task 345 work log — Splitting the Linux platform tree by bit width

설계: [20260922-345-linux-platform-width-split.md](../design/20260922-345-linux-platform-width-split.md)
작업 지시: [20260922-345-linux-platform-width-split.md](../work-orders/20260922-345-linux-platform-width-split.md)
선행: [작업 347 플랫폼 비트 폭 디렉터리 규칙](20260921-347-platform-architecture-directories.md)

## 한국어

### 한 일

`src/platform/linux/` 루트의 i386 전용 구현 20개 파일을 `src/platform/linux/x86/`로 옮겼다. 동작은 바꾸지 않았고 `#include` 경로와 CMake source list만 맞췄다.

| 이동 | 파일 |
| --- | --- |
| `x86/`로 | `native_dynamic_thunk`, `native_fault_observation`, `native_import_bridge`, `native_import_thunks`, `native_in_process_probe`, `native_in_process_runner`, `native_ipc_helper`, `native_ipc_helper_main`, `native_pe_image`, `native_pe_session`, `native_process_bootstrap` |
| 루트 유지 | `native_create_file_observation.cpp`, `native_helper_backend.cpp`, `native_ipc_capability_rejection_helper.cpp`, `native_ipc_host_probe.cpp`, `original_runner.cpp` |

`include/re2dj/platform/linux/`의 공개 헤더 둘은 규칙대로 그대로 뒀다.

### 분류를 판단이 아니라 측정으로 했다

규칙이 "CMake가 한 비트 폭에서만 source를 선택하면 하위 디렉터리 대상"이라고 정하므로, 파일을 읽고 인상으로 나누는 대신 `CMakeLists.txt`의 가드를 추출해 근거로 삼았다. Linux source가 놓인 가드는 `CMAKE_SIZEOF_VOID_P EQUAL 4`와 `RE2DJ_BUILD_LINUX_NATIVE_HELPER` 둘뿐이고, 후자는 Linux·32비트가 아니면 `FATAL_ERROR`로 거부되므로 역시 i386 전용이다.

이 방식의 값은 검증에서 드러났다. **Linux x64 빌드가 분류의 시험대**다. x86 전용 파일을 루트에 잘못 남겼다면 x64에서 컴파일이 깨진다. 실제로 깨지지 않았다.

### `#if`가 있는 두 파일을 나누지 않은 이유

`native_create_file_observation.cpp`(413줄)와 `original_runner.cpp`(538줄)는 두 폭 모두에서 컴파일되면서 본문이 `__i386__`로 갈린다. 규칙은 "한 파일 안의 대규모 `#if`로 두 구현을 섞지 않는다"고 한다.

이 둘의 `#if`는 두 구현을 섞은 것이 아니라 x64에서 기능 미지원을 반환하는 짧은 대체 경로다. 지금 나누면 공개 선언과 구현 사이에 adapter 계층을 하나 만들게 되는데, 그 경계는 Linux x64 compatibility-mode trampoline이 실제로 생길 때 필요에 맞춰 정하는 편이 낫다. 근거 없는 모양을 먼저 고정하지 않기로 하고 루트에 남겼다.

### 옮기면서 고친 상대 경로

파일이 한 단계 내려가면서 상위 상대 경로가 어긋났다. 네 곳을 고쳤다.

* `x86/native_import_thunks.cpp`, `x86/native_ipc_helper.cpp` — `../native_helper_protocol.h` → `../../`
* `x86/native_guest_module_set.h` — `../native_import_bridge.h` → 같은 디렉터리로 내려왔으므로 접두사 제거
* `x86/native_in_process_probe.cpp` — `../windows/native_ipc_host_probe.cpp` → `../../windows/`

마지막 것은 Linux probe가 Windows probe source를 매크로로 감싸 통째로 포함하는 기존 구조다. 이번 작업에서 만든 것이 아니며 경로만 맞췄다.

루트에 남은 두 파일이 옮긴 헤더를 참조하는 곳에는 `x86/` 접두사를 붙였다. 작업 344가 이미 쓰던 형태와 같다.

### 검증

| 검증 | 결과 |
| --- | --- |
| Linux i386(`linux-x86-debug`) 빌드 | 오류 0건 |
| Linux i386 단위 테스트 | `checks: 2107, failures: 0` |
| Linux i386 helper 타깃(`re2dj_linux_native_ipc_helper`) | 링크까지 성공 |
| Linux x64(`linux-x64-debug`) 빌드 | 오류 0건 |
| Linux x64 단위 테스트 | `checks: 2107, failures: 0` |
| Windows x86 빌드 | 오류 0건 |
| Windows x86 단위 테스트 | `checks: 2107, failures: 0` |
| 실제 4th CHD 회귀 | 이동 전과 동일 |

회귀 출력은 facade base `0x6f000000`, `CreateFileA` registry·정적 IAT·게스트 관측값 모두 `0x6f002039`, `GetVersion` `0x6f002026`, `CreateFileA("\\\\.\\NTICE")` 도달로 작업 344와 같다.

### 기존 실패 확인 — helper 구성의 `re2dj` 타깃

`linux-x86-helper` 구성 전체 빌드는 `re2dj` CLI 링크에서 실패한다. `main.cpp`가 `RunOriginalInProcessCreateFileCall` 등 `original_runner.cpp`의 심볼을 참조하는데, helper 구성에서는 그 source가 컴파일되지 않기 때문이다.

이 실패가 이번 이동 때문인지 확인하기 위해 작업 내용을 stash하고 HEAD에서 같은 빌드를 돌렸다. **동일하게 실패했다.** 따라서 기존 문제이며 이번 작업과 무관하다. helper 구성의 목적인 `re2dj_linux_native_ipc_helper` 타깃은 새 경로에서 정상 빌드된다.

이 구성이 `re2dj` 타깃까지 정의하는 것이 맞는지는 별도 판단이 필요하므로 후속으로 남긴다.

### 범위 밖

`src/platform/windows/` 재배치는 넣지 않았다. CMake가 참조하는 Windows platform source 34개가 전부 Win32 x86 전용이라, 규칙을 그대로 적용하면 루트가 비고 트리 전체가 한 단계 내려간다. 그것이 옳은 결과인지 아니면 일부가 x64 host에서도 빌드될 폭 중립 코드인지는 58개 파일에 대한 파일 단위 판단이 필요하다. Linux와 성격이 다른 문제이므로 `docs/TODO.md`에 후속으로 올렸다.

## English

Design: [20260922-345-linux-platform-width-split.md](../design/20260922-345-linux-platform-width-split.md)
Work order: [20260922-345-linux-platform-width-split.md](../work-orders/20260922-345-linux-platform-width-split.md)
Prerequisite: [Task 347, platform bit-width directory policy](20260921-347-platform-architecture-directories.md)

### What was done

Twenty i386-only files moved from the `src/platform/linux/` root into `src/platform/linux/x86/`. Behavior is unchanged; only include paths and the CMake source lists were adjusted. `native_create_file_observation.cpp`, `native_helper_backend.cpp`, `native_ipc_capability_rejection_helper.cpp`, `native_ipc_host_probe.cpp` and `original_runner.cpp` stay at the root, and the two public headers under `include/re2dj/platform/linux/` stay where the rule puts them.

### Classification was measured, not judged

The rule makes CMake's selection the criterion, so instead of reading each file and forming an impression, the guards were extracted from `CMakeLists.txt` and used as the evidence. Linux sources sit under only `CMAKE_SIZEOF_VOID_P EQUAL 4` and `RE2DJ_BUILD_LINUX_NATIVE_HELPER`, and the latter is rejected with `FATAL_ERROR` unless the build is Linux and 32-bit, so it is i386-only as well.

The value of that approach showed up in verification: **the Linux x64 build is the test of the classification**, because an x86-only file wrongly left at the root fails to compile there. Nothing failed.

### Why the two files with `#if` were not split

`native_create_file_observation.cpp` (413 lines) and `original_runner.cpp` (538 lines) compile on both widths while their bodies branch on `__i386__`, and the rule warns against mixing two implementations behind large `#if` blocks. These blocks are not two implementations but a short unsupported-path return for x64. Splitting now would add an adapter layer whose boundary is better decided when the Linux x64 compatibility-mode trampoline actually exists, so rather than fix a shape on guesswork they stay at the root.

### Relative paths corrected by the move

Dropping one directory level broke four parent-relative includes: `../native_helper_protocol.h` became `../../` in `x86/native_import_thunks.cpp` and `x86/native_ipc_helper.cpp`; `../native_import_bridge.h` lost its prefix in `x86/native_guest_module_set.h` because the target came down with it; and `../windows/native_ipc_host_probe.cpp` became `../../windows/` in `x86/native_in_process_probe.cpp`. That last one is the pre-existing arrangement where the Linux probe wraps the Windows probe source in macros and includes it wholesale; only its path changed. The two retained root files gained the `x86/` prefix on moved headers, the form Task 344 already used.

### Verification

Linux i386 built with no errors and unit tests reported `checks: 2107, failures: 0`; the `re2dj_linux_native_ipc_helper` target linked from the new paths; Linux x64 built with no errors and the same unit-test result; Windows x86 built and tested the same. The real 4th CHD regression matched the pre-move result exactly: facade base `0x6f000000`, `CreateFileA` agreeing at `0x6f002039` across registry, static IAT and guest, `GetVersion` at `0x6f002026`, and `CreateFileA("\\\\.\\NTICE")` reached.

### Confirmed pre-existing failure — the `re2dj` target in the helper configuration

A full `linux-x86-helper` build fails linking the `re2dj` CLI, because `main.cpp` references symbols from `original_runner.cpp`, which that configuration does not compile. To attribute the failure, the work was stashed and the same build run at HEAD: it **failed identically**, so this is pre-existing and unrelated. The target that configuration exists for, `re2dj_linux_native_ipc_helper`, builds cleanly from the new paths. Whether that configuration should define the `re2dj` target at all needs its own judgment and is left as a follow-up.

### Out of scope

`src/platform/windows/` was not reorganized. All 34 Windows platform sources CMake references are Win32 x86-only, so applying the rule literally empties the root and moves the whole tree down a level. Whether that is right, or whether some of it is width-neutral code a future x64 host would also build, needs a per-file judgment across 58 files — a different problem from Linux, queued in `docs/TODO.md`.
