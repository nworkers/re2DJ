# 작업 368 작업 지시서 — 원본 MSVC CRT 시작 / Task 368 work order — Original MSVC CRT start-up

설계: [20260925-368-original-crt-startup.md](../design/20260925-368-original-crt-startup.md)

## 절차 / Steps

1. `GuestHeap`, `GuestProcess`의 heap 목록·주 image·필터, `target::GuestRootPath`/`GuestExecutablePath`를 추가한다. launcher가 공용 규칙을 쓰게 한다.
   *Add `GuestHeap`, `GuestProcess`'s heaps, main image, and filter, and `target::GuestRootPath`/`GuestExecutablePath`, with the launcher using the shared rule.*
2. kernel32에 설계 1–6의 export를 구현한다. `ImportCallServices::GuestModuleName`을 추가한다.
   *Implement design §1–6's kernel32 exports and add `ImportCallServices::GuestModuleName`.*
3. Linux: arena 512 MiB와 `MAP_NORESERVE`를 적용한다. `RunOriginalInProcessContinuation`이 게스트 경로를 받는다. stop stub을 host trap으로 표시한다.
   *Linux: a 512 MiB arena with `MAP_NORESERVE`; `RunOriginalInProcessContinuation` takes the guest path; mark the stop stub as a host trap.*
4. 단위 테스트, 분석·TODO·ARCHITECTURE 갱신.
   *Unit tests and analysis/TODO/ARCHITECTURE updates.*

## 완료 조건 / Done when

- Linux 두 폭과 Windows x86 build·CTest가 통과한다. 기존 진단·probe가 그대로다.
  *Both Linux widths and Windows x86 build and pass CTest, with existing diagnostics and probes unchanged.*
- 실제 4th가 두 폭에서 CRT 시작을 마치고 `CreateEventA`(`0x00406fe3`)에서 멈춘다. 주소를 정규화한 기록이 같다.
  *On both widths the real 4th finishes CRT start-up and stops at `CreateEventA` (`0x00406fe3`), with identical records after address normalization.*
