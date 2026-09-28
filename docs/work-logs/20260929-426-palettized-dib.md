# 작업 426 작업 로그 — StretchDIBits의 8비트 팔레트 DIB / Task 426 work log — 8-bit palettized DIBs in StretchDIBits

설계: [20260929-426-palettized-dib.md](../design/20260929-426-palettized-dib.md) · 지시서: [20260929-426-palettized-dib.md](../work-orders/20260929-426-palettized-dib.md)

## 2026-09-29

- 사용자 로그: 호출 85,127 `StretchDIBits(dc, 0, 0, 8, 8, 0, 0, 8, 8, bits=006e5be6, bmi=006e57be, DIB_RGB_COLORS, SRCCOPY)`가 처리되지 않았다. 앞선 호출들은 `bits - bmi = 0x28`로 팔레트가 없는 DIB였고, 이 호출은 0x428이다.
  *User's log: call 85,127, `StretchDIBits(dc, 0, 0, 8, 8, 0, 0, 8, 8, bits=006e5be6, bmi=006e57be, DIB_RGB_COLORS, SRCCOPY)`, went unhandled. The earlier calls had `bits - bmi = 0x28` (no palette); this one has 0x428.*
- Windows 11 측정 결과는 설계에 적었다.
  *The Windows 11 measurements are in the design.*
- 테스트 결과, 실패 0:
  - Windows x86: CTest 6개 통과, 단위 5360 checks.
  - Linux x64·x86: CTest 4개 통과, 단위 5357 checks.

  *Test results, no failures:*
  - *Windows x86: all 6 CTest tests pass, 5360 unit checks.*
  - *Linux x64 and x86: all 4 CTest tests pass, 5357 unit checks.*
- Linux 5th: 두 폭 모두 90초 시간 제한으로 창이 닫힐 때까지 멈추지 않는다.
  - x64: 1,581,829호출.
  - x86: 1,369,273호출. 타이틀 화면("PLATINUM LIMITED EDITION")이 캡처에서 바르게 보인다.

  *Linux 5th: on both widths it runs without stopping until the 90-second timeout closes the window.*
  - *x64: 1,581,829 calls.*
  - *x86: 1,369,273 calls; the title screen ("PLATINUM LIMITED EDITION") shows correctly in the capture.*
- Windows 제품은 GDI를 Windows가 직접 처리하므로 바뀐 것이 없다.
  *The Windows product has GDI handled by Windows itself and is unchanged.*
