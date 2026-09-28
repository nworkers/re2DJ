# 작업 426 설계 — StretchDIBits의 8비트 팔레트 DIB / Task 426 design — 8-bit palettized DIBs in StretchDIBits

선행: [작업 425 설계](20260929-425-render-target-lock.md)

## 배경 / Background

사용자가 Linux에서 5th를 실행했을 때 85,127번째 호출 `gdi32!StretchDIBits`에서 멈췄다. 게임이 `system\modeselect\fadeblack.abm`(8×8)을 표면 DC에 그리던 중이었다. 이 파일의 비트는 BITMAPINFO 뒤 0x428바이트에서 시작한다(헤더 40 + 팔레트 1024). 곧 8비트 팔레트 DIB다. 지금까지 facade는 16비트와 24비트만 받았다.

*When the user ran 5th on Linux, it stopped at call 85,127, `gdi32!StretchDIBits`, while the game drew `system\modeselect\fadeblack.abm` (8×8) into a surface's DC. Its bits start 0x428 bytes after the BITMAPINFO (a 40-byte header plus a 1024-byte palette), so it is an 8-bit palettized DIB; the facade took only 16- and 24-bit DIBs so far.*

## Windows 측정 / Windows measurements

Windows 11에서 32비트 probe로 측정했다(`scratchpad/dib426`). 8비트 DIB를 RGB565 DIB section에 `StretchDIBits`로 그렸다.

- 팔레트 색은 같은 색의 24비트 DIB와 똑같이 변환된다. 예: (0x7F, 0x80, 0x81) → `7c10`, (0x07, 0x08, 0x03) → `0040`, (0x12, 0x34, 0x56) → `11aa`.
- `biClrUsed` 0은 256색이다. 팔레트보다 큰 인덱스(8색 팔레트에서 200)는 검정(`0000`)이다.
- 반환값은 16·24비트와 같은 규칙이다(ySrc + SrcHeight). last error는 바뀌지 않는다.

*Measured with a 32-bit probe on Windows 11 (`scratchpad/dib426`), drawing an 8-bit DIB into an RGB565 DIB section with `StretchDIBits`:*
- *Palette colours convert exactly as the same colours in a 24-bit DIB: (0x7F, 0x80, 0x81) → `7c10`, (0x07, 0x08, 0x03) → `0040`, (0x12, 0x34, 0x56) → `11aa`.*
- *A `biClrUsed` of 0 means 256 entries. An index past the palette (200 with an 8-entry palette) is black (`0000`).*
- *The return value follows the 16- and 24-bit rule (ySrc + SrcHeight), and the last error does not change.*

## 결정 / Decisions

1. `StretchDIBits`는 `BI_RGB` 8비트 DIB를 받는다.
   - 팔레트는 `biClrUsed`개(0이면 256)를 읽어 24비트 색으로 둔다.
   - 각 픽셀은 인덱스로 색을 찾아, 24비트 DIB와 같은 변환(`ConvertGdiPixel`, `kGdiBgr888`)을 거친다.
   - 팔레트 밖 인덱스는 검정이다.
   - 늘이기, 잘라내기, 행 순서는 기존 규칙을 그대로 쓴다.

   *`StretchDIBits` takes 8-bit `BI_RGB` DIBs:*
   - *the palette is read as `biClrUsed` entries (256 when 0) and kept as 24-bit colours;*
   - *each pixel looks its colour up by index and goes through the same conversion as a 24-bit DIB (`ConvertGdiPixel`, `kGdiBgr888`);*
   - *an index past the palette is black;*
   - *stretching, clipping, and row order keep the existing rules.*
2. `DIB_PAL_COLORS`와 4비트·1비트 DIB는 측정하지 않았다. 추측하지 않고 멈춘다.
   *`DIB_PAL_COLORS` and 4-bit or 1-bit DIBs were not measured; they stop rather than guess.*
