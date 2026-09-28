# 작업 385 작업 로그 — Linux IO 보드 포트 입출력 / Task 385 work log — I/O board port access on Linux

설계: [20260926-385-linux-legacy-io-ports.md](../design/20260926-385-linux-legacy-io-ports.md)
작업 지시서: [20260926-385-linux-legacy-io-ports.md](../work-orders/20260926-385-linux-legacy-io-ports.md)

## 진행 / Progress

Windows handler의 판정 규칙을 core `DecodeLegacyIoAccess`와 `MergeLegacyIoRead`로 옮겼다. Linux 두 폭의 signal handler는 SIGSEGV에서 이 core로 보드 접근인지 판정한다. 보드 접근이면 전원 켠 상태의 `LegacyIoPortBus`로 답하고 guest로 돌아간다.

실제 4th는 두 폭에서 포트 `0x0103`부터 세 번 읽는다. 이 순서는 Windows IO 포트 기록(`0x0103`, `0x0104`, `0x0105`)과 같다. 게임은 이어서 `winmm!mixerGetNumDevs`에서 멈췄다.

*The Windows handler's rules moved into the core's `DecodeLegacyIoAccess` and `MergeLegacyIoRead`. On SIGSEGV, both Linux widths' signal handlers use them to decide whether a fault is a board access; if it is, they answer through a `LegacyIoPortBus` in its power-on state and return to the guest.*

*On both widths the real 4th reads three ports starting at `0x0103`, in the same order as the Windows I/O port record (`0x0103`, `0x0104`, `0x0105`). The game then stopped at `winmm!mixerGetNumDevs`.*

## 변경 / Changes

- **core**: `legacy_io_trap.h/.cpp`(`LegacyIoTrapPolicy`, `LegacyIoAccess`, `DecodeLegacyIoAccess`, `MergeLegacyIoRead`).
  ***Core:** `legacy_io_trap.h/.cpp` (`LegacyIoTrapPolicy`, `LegacyIoAccess`, `DecodeLegacyIoAccess`, `MergeLegacyIoRead`).*
- **Windows**: `HandleLegacyIoPortException`의 판정과 EAX 합성이 core를 쓴다.
  ***Windows:** `HandleLegacyIoPortException`'s decision and EAX merge use the core.*
- **Linux**:
  - `native_legacy_io.h/.cpp`를 추가했고, x86·x64 signal handler의 SIGSEGV 경로에 연결했다.
  - `OriginalRunEnvironment::legacy_io`와 결과의 `legacy_io_*` 활동을 추가했다.
  - CLI의 `legacy io` 출력을 추가했다.

  ***Linux:***
  - *`native_legacy_io.h/.cpp` is added and wired into the x86 and x64 signal handlers' SIGSEGV path.*
  - *`OriginalRunEnvironment::legacy_io` and the result's `legacy_io_*` activity are added.*
  - *The CLI prints a `legacy io` line.*
- **단위 테스트**: `legacy_io_trap_test.cpp`에서 4th의 byte helper, 방향 제한, 폭 불일치, opcode 판단, word 접두, EAX 합성을 확인한다.
  ***Unit tests:** `legacy_io_trap_test.cpp` covers the 4th's byte helpers, the direction limits, width mismatches, the opcode fallback, word prefixes, and the EAX merge.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Windows x86 build, CTest | exit 0, 우리 코드 경고·오류 없음, 6/6 / exit 0, no warnings or errors from this project, 6/6 |
| Windows 실제 4th, 변경 전(`6280268`)·후 각 30초 / real 4th on Windows, pre-change (`6280268`) and post-change, 30 s each | ddraw 기록 1,410줄과 IO 포트 기록(`re2dj:vfs:io-*`) 257줄이 같다. / *The 1,410 ddraw lines and the 257 I/O port lines (`re2dj:vfs:io-*`) match.* |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 / no warnings or errors, 3/3 each |
| Linux probe, 기존 진단 네 개 / probes and the four diagnostics | 이전과 같음 / as before |
| 실제 4th, Linux 두 폭 / real 4th, both Linux widths | 호출 1,881번이며, 주소를 정규화하면 두 폭이 같다. `legacy io: reads=3 writes=0 unanswered=0 first=in 0x0103`. 그 뒤 `#1881 winmm.dll!mixerGetNumDevs`에서 멈춘다. / *1,881 calls, identical on both widths after address normalization, with `legacy io: reads=3 writes=0 unanswered=0 first=in 0x0103`. The run then stops at `#1881 winmm.dll!mixerGetNumDevs`.* |

## 다음 / Next

`winmm`의 mixer API(`mixerGetNumDevs`부터)다. Windows에서 측정해 구현한다.

*Next is `winmm`'s mixer API, from `mixerGetNumDevs`, measured on Windows and implemented.*
