# 작업 417 작업 지시서 — Linux 게스트 스레드 / Task 417 work order — guest threads on Linux

설계: [20260928-417-guest-threads.md](../design/20260928-417-guest-threads.md)

## 절차 / Steps

1. 1st가 스레드로 무엇을 하는지 복호화 이미지로 확인하고, Windows에서 스레드 API의 동작을 측정한다.
   *Check what 1st does with its thread in the decrypted image, and measure the thread APIs on Windows.*
2. 폭 중립 게스트 잠금과 스레드 목록, 두 폭의 스레드별 실행 상태와 스레드 시작, x64 transition 상태의 인계, 프로세스 종료 전달을 구현한다.
   *Implement the width-neutral guest lock and thread list, both widths' per-thread run state and thread start, the x64 hand-over of transition state, and process-ending propagation.*
3. HLE 서비스와 `GuestProcess` 스레드 기록, kernel32 스레드 API, 스레드별 last error와 TEB ClientId를 구현한다.
   *Implement the HLE services, `GuestProcess` thread records, the kernel32 thread APIs, and per-thread last error and TEB ClientId.*
4. 단위 테스트와 두 폭의 합성 스레드 probe(정상 종료, fault 종료)를 만든다. Linux 두 폭과 Windows에서 테스트하고, 1st와 4th를 Linux 두 폭에서 실행한다.
   *Add unit tests and a synthetic thread probe on both widths (normal end, fault end); test on both Linux widths and Windows, and run 1st and 4th on both Linux widths.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 테스트를 통과한다.
  *Windows x86 and both Linux widths build and pass their tests.*
- 합성 probe에서 게스트 스레드가 자기 TEB로 import를 부르고 끝나며, 스레드의 fault가 메인 실행을 끝낸다.
  *In the synthetic probe a guest thread calls an import with its own TEB and ends, and a thread's fault ends the main run.*
- 1st가 `CreateThread`를 지나 소리 스레드와 메인 스레드가 번갈아 실행되고, 4th는 전처럼 창이 닫힐 때까지 돈다.
  *1st gets past `CreateThread` with its sound thread and main thread taking turns, and 4th still runs until its window closes.*
