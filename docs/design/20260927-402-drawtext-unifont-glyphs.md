# 작업 402 설계 — DrawTextA 글자를 Unifont로 / Task 402 design — DrawTextA glyphs from Unifont

선행: [작업 401 설계](20260927-401-surface-gdi-drawing.md)

## 배경 / Background

작업 401의 Linux `DrawTextA`는 반환값만 측정대로 돌려주고 글자는 그리지 않았다. Windows는 메모리 DC의 기본 글꼴인 한글 `System` 비트맵 글꼴(높이 16)로 그린다. 그 글리프는 Microsoft 자산이라 저장소에 넣을 수 없다. 사용자는 "비슷한 공개 글꼴 비트맵으로 그리자"고 결정했다(2026-09-27).

*Task 401's Linux `DrawTextA` returned the measured result but drew no glyphs. Windows draws with the memory DC's default font, the Korean `System` bitmap font (16 high), whose glyphs are Microsoft's and cannot go into the repository. The user decided to draw with a similar open bitmap font (2026-09-27).*

### 측정 / Measurements

Windows 11의 32비트 프로그램으로 5-6-5 DIB section에 그려 측정했다.

*Measured by a 32-bit program on Windows 11, drawing into a 5-6-5 DIB section.*

| 항목 / Item | 결과 / Result |
| --- | --- |
| 가운데 배치 / centred | 사각형 0,0,64,40의 `temp`(32×16): 셀 16,12. 63×39에서는 15,11 (남는 폭·높이의 절반을 내림) / *cell at 16,12; at 63×39 at 15,11 (half the room, rounded down)* |
| 다른 배치 / other placements | 사각형 3,5,60,36: 가운데 15,12, `DT_TOP` 3,5, `DT_RIGHT\|DT_BOTTOM` 28,20 / *centred 15,12; `DT_TOP` 3,5; right and bottom 28,20* |
| 잘라내기 / clipping | 사각형 밖은 그리지 않음. `DT_NOCLIP`이면 bitmap 경계까지만 / *nothing outside the rectangle; with `DT_NOCLIP`, only the bitmap bounds* |
| OPAQUE | 글자 셀(글자 폭 × 16)을 배경색(기본 흰색)으로 칠함 / *paints the text cell (text width × 16) in the background color, white by default* |
| 글자 폭 / widths | `System`은 가변 폭: `iW` 18px, `temp` 32px / *`System` is variable width: `iW` 18 px, `temp` 32 px* |

## 결정 / Decisions

1. **글꼴.** GNU Unifont 15.1.05의 U+0020–U+007E 8×16 글리프를 쓴다. 높이가 `System`과 같고 모양도 비슷한 고정 폭 비트맵이다. Unifont 글꼴 파일은 OFL 1.1과 GPL 2.0+ 이중 라이선스다. 프로젝트 정책에 따라 OFL 1.1을 택한다. 글리프 데이터만 가져오고 Unifont의 프로그램 소스는 쓰지 않는다.
   - `third_party/unifont/`: 원본 hex의 해당 줄, 라이선스 원문, 출처와 SHA-256.
   - `scripts/generate_unifont_glyphs.py`가 `src/hle/unifont_ascii_glyphs.inc`를 만든다.
   - `hle/guest_font.h`의 `GuestFontGlyph`가 한 글자의 16줄을 돌려준다.

   ***The font.** GNU Unifont 15.1.05's 8×16 glyphs for U+0020–U+007E: a fixed-width bitmap font as high as `System` and similar in shape. Unifont's font files are dual-licensed OFL 1.1 / GPL 2.0+; per project policy the OFL 1.1 is selected. Only glyph data is taken, none of Unifont's program sources.*
   - *`third_party/unifont/`: the upstream hex lines, the license text, and the source and SHA-256.*
   - *`scripts/generate_unifont_glyphs.py` generates `src/hle/unifont_ascii_glyphs.inc`.*
   - *`GuestFontGlyph` in `hle/guest_font.h` gives one character's 16 rows.*
2. **배치.** 셀 폭은 글자 수 × 8이다. 가로는 왼쪽, 왼쪽 + (남는 폭 / 2 내림), 오른쪽 − 셀 폭으로 놓고, 세로도 높이 16으로 같게 놓는다(측정한 규칙).
   ***Placement.** The cell is 8 pixels per character, placed left, left + half the room rounded down, or right − its width, and likewise vertically with height 16 (the measured rules).*
3. **그리기.** 사각형(`DT_NOCLIP`이면 bitmap)으로 자른다. OPAQUE면 셀을 배경색으로 먼저 칠하고, 글리프의 켜진 픽셀을 글자 색으로 쓴다. 색은 `ConvertGdiPixel`로 bitmap 배치에 맞춘다. `GuestDc`에 배경색(기본 흰색)을 둔다.
   ***Drawing.** Clipped to the rectangle (the bitmap with `DT_NOCLIP`). In OPAQUE mode the cell is first painted in the background color; the glyphs' set pixels take the text color. Colors convert through `ConvertGdiPixel`. `GuestDc` gains a background color, white by default.*
4. **모델 밖.** 출력 가능한 ASCII가 아닌 바이트(CP949 2바이트 한글 포함), 팔레트 색, 16비트가 아닌 bitmap은 멈춘다.
   ***Unmodelled.** Bytes outside printable ASCII (including CP949 double-byte Hangul), palette colors, and bitmaps other than 16-bit stop.*

## Windows와 다른 점 / Differences from Windows

- 글리프 모양이 `System`과 다르다. / *The glyph shapes differ from `System`'s.*
- `System`은 가변 폭이라 `i`·`W` 같은 글자가 섞이면 셀 폭과 위치가 달라진다. `temp`처럼 폭이 고른 글자는 같은 32px이다. / *`System` is variable width, so text mixing narrow and wide letters is laid out differently; even-width text such as `temp` keeps the same 32 px.*

## 범위 밖 / Out of scope

- 한글 글리프, 가변 폭, `SelectObject`·`CreateFont`, `SetBkColor`. / *Hangul glyphs, variable widths, `SelectObject`/`CreateFont`, and `SetBkColor`.*
