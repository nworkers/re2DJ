# 작업 372 작업 지시서 — 게스트 호출과 window 생성 / Task 372 work order — guest calls and window creation

설계: [20260926-372-guest-callbacks-and-window.md](../design/20260926-372-guest-callbacks-and-window.md)

## 절차 / Steps

1. Windows에서 `LoadIconA`/`LoadCursorA`, `GetStockObject`, `RegisterClassA`, `CreateWindowExA`의 메시지·구조체, `UpdateWindow`를 32비트 PowerShell로 측정한다.
   *Measure `LoadIconA`/`LoadCursorA`, `GetStockObject`, `RegisterClassA`, `CreateWindowExA`'s messages and structures, and `UpdateWindow` from 32-bit PowerShell on Windows.*
2. `CallNativeGuestStdcall`을 x86(현재 stack의 naked asm helper)과 x64(중첩 compat 전환)에 구현한다.
   *Implement `CallNativeGuestStdcall` for x86 (a naked asm helper on the current stack) and x64 (a nested compatibility-mode transition).*
3. `ImportCallServices::CallGuest`와 `GuestCall`을 추가한다. 기록 장식자는 이것을 `call` 사건으로 남긴다. Linux 진단의 dispatch를 재진입 가능하게 하고, API log를 머리·결과로 나눈다.
   *Add `ImportCallServices::CallGuest` and `GuestCall`, noted by the recording decorator as `call` events; make the Linux diagnostic's dispatch re-entrant and split the API log into head and outcome.*
4. `GuestUser`와 user32 export 6개(`GetActiveWindow` 갱신 포함)를 구현한다. `gdi32_module`과 `GetStockObject`를 추가한다.
   *Add `GuestUser`, the six user32 exports (and update `GetActiveWindow`), and `gdi32_module` with `GetStockObject`.*
5. 단위 테스트, 문서 갱신.
   *Unit tests and documentation.*

## 완료 조건 / Done when

- Linux 두 폭과 Windows x86 build·CTest가 통과하고, 기존 진단·probe가 그대로다.
  *Both Linux widths and Windows x86 build and pass CTest, with existing diagnostics and probes unchanged.*
- 실제 4th가 두 폭에서 window를 만들고 WndProc가 측정한 메시지를 받은 뒤 DirectDraw 첫 호출까지 간다.
  *On both widths the real 4th creates its window, its WndProc receives the measured messages, and it reaches its first DirectDraw call.*
