# 작업 402 작업 로그 — DrawTextA 글자를 Unifont로 / Task 402 work log — DrawTextA glyphs from Unifont

설계: [20260927-402-drawtext-unifont-glyphs.md](../design/20260927-402-drawtext-unifont-glyphs.md) · 지시서: [20260927-402-drawtext-unifont-glyphs.md](../work-orders/20260927-402-drawtext-unifont-glyphs.md)

## 2026-09-27

- 측정: 설계의 표와 같다. 가운데 배치는 남는 폭·높이의 절반을 내림한다(63×39에서 15,11).
  *Measured as in the design's table; centring rounds half the room down (15,11 at 63×39).*
- Unifont 15.1.05 `unifont-15.1.05.hex.gz`, SHA-256 `e2b2e2c3c85a26e76afec499d27be66f2ebb356be6634cc2f3339e6a41026eeb`. U+0020–U+007E 95줄을 그대로 옮기고 `.inc`를 생성했다.
  *Took the 95 lines for U+0020–U+007E unchanged and generated the `.inc`.*
- 단위 테스트: `t`의 4–7행이 `DT_NOCLIP`로 bitmap 위쪽에서 그려지고(`0x10` → x 3, `0x7C` → x 1–5, 텍스트 색 `0xF800`), TRANSPARENT는 다른 픽셀을 그대로 둔다. `DT_NOCLIP`이 없으면 사각형 밖이라 그리지 않는다. OPAQUE는 셀을 흰색으로 칠한다. 0xB0 바이트와 팔레트 색은 멈춘다.
  *Unit tests: `t`'s rows 4–7 drawn from above the bitmap with `DT_NOCLIP`, TRANSPARENT leaving other pixels; without `DT_NOCLIP` nothing lands; OPAQUE paints the cell white; byte 0xB0 and a palette color stop.*
- 결과: Windows x86 4397 checks, Linux x64·x86 4394 checks, 실패 0. Windows 제품은 진짜 GDI로 그리므로 이 코드를 쓰지 않는다. 그래서 실제 실행 비교는 생략했다.
  *Windows x86 4397 checks and Linux x64 and x86 4394 checks, no failures. The Windows product draws with real GDI and does not use this code, so no Windows real-run comparison was made.*
- Linux x64 실제 실행(코인·시작 조작 스크립트): 대체 텍스처의 `DrawTextA`를 지나 작업 401과 같이 `kernel32!GetFileAttributesA`에서 정지(호출 829,060번).
  *Linux x64 real run (coin/start script): past the stand-in texture's `DrawTextA`, stopping as in Task 401 at `kernel32!GetFileAttributesA` (call 829,060).*
