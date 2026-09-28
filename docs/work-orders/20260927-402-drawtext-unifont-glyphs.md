# 작업 402 작업 지시서 — DrawTextA 글자를 Unifont로 / Task 402 work order — DrawTextA glyphs from Unifont

설계: [20260927-402-drawtext-unifont-glyphs.md](../design/20260927-402-drawtext-unifont-glyphs.md)

## 절차 / Steps

1. Windows 11에서 `DrawTextA`의 셀 위치, 잘라내기, OPAQUE 배경을 측정한다.
   *Measure `DrawTextA`'s cell placement, clipping, and OPAQUE background on Windows 11.*
2. Unifont 15.1.05 배포본을 내려받아 SHA-256을 기록하고, ASCII 줄과 OFL 원문을 `third_party/unifont/`에 둔다. 생성 스크립트로 `.inc`를 만든다.
   *Download the Unifont 15.1.05 distribution, record its SHA-256, and place the ASCII lines and the OFL text under `third_party/unifont/`; generate the `.inc` with the script.*
3. `GuestFontGlyph`와 `DrawTextA` 그리기를 구현하고 `THIRD_PARTY_NOTICES.md`에 고지를 더한다.
   *Implement `GuestFontGlyph` and `DrawTextA`'s drawing, and add the notice to `THIRD_PARTY_NOTICES.md`.*
4. 단위 테스트(글리프 픽셀, 잘라내기, OPAQUE, 모델 밖), 실제 실행, 문서.
   *Unit tests (glyph pixels, clipping, OPAQUE, unmodelled cases), a real run, and documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 단위 테스트를 통과한다.
  *Windows x86 and both Linux widths build and pass the unit tests.*
- Linux 실제 4th가 대체 텍스처의 `DrawTextA`를 지나 작업 401과 같은 경계에서 멈춘다.
  *On Linux the real 4th gets past the stand-in texture's `DrawTextA` and stops at the same boundary as Task 401.*
