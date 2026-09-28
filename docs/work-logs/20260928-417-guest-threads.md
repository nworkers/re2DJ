# 작업 417 작업 로그 — Linux 게스트 스레드 / Task 417 work log — guest threads on Linux

설계: [20260928-417-guest-threads.md](../design/20260928-417-guest-threads.md) · 지시서: [20260928-417-guest-threads.md](../work-orders/20260928-417-guest-threads.md)

## 2026-09-28

- 1st의 복호화 이미지(`--image-dump`, 원본 자산이라 커밋하지 않음)에서 스레드 경로를 확인했다.
  - `0x41dea0`: `CreateThread(0, 0, 0x41df20, 0, 0, &id)`와 `SetThreadPriority(h, 1)`을 부른다.
  - `0x41df20`: 소리 스트리밍 루프다. `Sleep(6)`과 `Sleep(1)`을 부른다.
  - `0x41e040`: 스레드를 끝낸다. `WaitForSingleObject(h, 5000)`으로 기다리고, 시간이 지나면 `TerminateThread`를 부른다.

  *The thread path was confirmed in 1st's decrypted image (`--image-dump`; an original asset, not committed):*
  - *`0x41dea0` calls `CreateThread(0, 0, 0x41df20, 0, 0, &id)` and `SetThreadPriority(h, 1)`.*
  - *`0x41df20` is the sound-streaming loop, calling `Sleep(6)` and `Sleep(1)`.*
  - *`0x41e040` ends the thread: it waits with `WaitForSingleObject(h, 5000)` and calls `TerminateThread` on timeout.*
- Windows 11 측정 결과는 설계에 적었다.
  *The Windows 11 measurements are in the design.*
- 합성 스레드 probe:
  - x64: 스레드 TEB `0xef3ff000`, 메인 TEB `0xef8fe000`, 종료 값 `0xb9`(0x55 + 100). `SIGILL@0xefdff040`이 실행을 끝냈다. 그 뒤 재실행도 정상이다.
  - x86: 스레드 TEB `0xf7f9c000`, 메인 TEB `0xf7f9f000`, 같은 종료 값과 fault 종료.

  *Synthetic thread probe:*
  - *x64: thread TEB `0xef3ff000`, main TEB `0xef8fe000`, exit value `0xb9` (0x55 + 100); `SIGILL@0xefdff040` ended the run, and a rerun afterwards passed.*
  - *x86: thread TEB `0xf7f9c000`, main TEB `0xf7f9f000`, with the same exit value and fault end.*
- 테스트 결과(실패 0):
  - Windows x86: CTest 6개 통과, 단위 4921 checks.
  - Linux x64·x86: CTest 4개 통과(in-process probe 포함), 단위 4918 checks.

  *Test results (no failures):*
  - *Windows x86: all 6 CTest tests pass, 4921 unit checks.*
  - *Linux x64 and x86: all 4 CTest tests pass (the in-process probe included), 4918 unit checks.*
- Linux 1st(x64·x86 모두):
  - `CreateThread`(1027번째 호출)와 `SetThreadPriority`를 지난다. 이후 소리 스레드(`[thread 0f08]`)의 `Sleep(1)`과 메인 스레드의 파일 읽기·critical section·`IDirectSoundBuffer::Lock/Unlock`이 번갈아 실행된다.
  - 호출 1102번째(x86은 1101번째, 스레드 교대 시점 차이) 메인 스레드의 `kernel32!HeapValidate`에서 멈춘다.

  *Linux 1st (both x64 and x86):*
  - *It gets past `CreateThread` (call 1027) and `SetThreadPriority`, after which the sound thread's (`[thread 0f08]`) `Sleep(1)` alternates with the main thread's file reads, critical sections, and `IDirectSoundBuffer::Lock/Unlock`.*
  - *It stops at the main thread's `kernel32!HeapValidate` at call 1102 (1101 on x86; the thread switches fall at different points).*
- 실행기 핵심이 바뀌었으므로 4th 회귀를 90초 돌렸다. 두 폭 모두 멈추는 import 없이 창이 닫힐 때까지 돈다(x64 1,772,008 호출, x86 1,536,900 호출).
  *As the runner's core changed, the 4th regression ran for 90 s; on both widths it runs until the window closes with no stopping import (1,772,008 calls on x64, 1,536,900 on x86).*
- Windows 제품은 게스트를 Windows 위에서 직접 돌리므로 바뀐 것이 없어 실행 확인은 생략했다. HLE 변경은 Windows 단위 테스트로 확인했다.
  *The Windows product runs the guest directly on Windows and is unchanged, so no Windows run was made; the HLE changes are covered by the Windows unit tests.*
