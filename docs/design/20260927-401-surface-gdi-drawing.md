# 작업 401 설계 — 표면 DC의 GDI 그리기 / Task 401 design — GDI drawing into surface DCs

선행: [작업 389 설계](20260926-389-surface-dc-and-stretchdibits.md), [작업 400 설계](20260927-400-vertex-buffers.md)

## 배경 / Background

작업 400 뒤 Linux의 4th는 곡 정보 화면에서 `gdi32!CreateSolidBrush`에 멈췄다. 곡 재킷 이미지 `temp.abm`은 CHD에 없다. Windows도 이 파일을 찾지 못한다. 그러면 게임은 대체 텍스처를 만든다.

1. 표면을 만들고 `GetDC`를 부른다.
2. `CreateSolidBrush(0x007F0000)`와 `FillRect`로 바탕을 칠한다.
3. `DeleteObject`로 브러시를 지운다.
4. `SetTextColor(흰색)`, `SetBkMode(TRANSPARENT)`를 부른다.
5. `DrawTextA(dc, "temp", 4, rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE)`로 글자를 쓴다.

Windows 제품에서는 이 DC가 진짜 GDI DIB라서 Windows GDI가 그린다.

*After Task 400 the 4th on Linux stopped at `gdi32!CreateSolidBrush` on the song info screen. The jacket image `temp.abm` is not on the CHD (Windows cannot find it either), so the game builds a stand-in texture in the five steps above. On the Windows product this DC is a real GDI DIB, so Windows GDI draws it.*

### 측정 / Measurements

32비트 프로그램(MSVC x86)과 PowerShell로 Windows 11에서 측정했다. 대상은 5-6-5 DIB section과 메모리 DC다.

| 함수 / Function | 결과 / Result |
| --- | --- |
| `CreateSolidBrush` | 어떤 COLORREF든 그대로 보관, last error 그대로 / *keeps any COLORREF as given; last error unchanged* |
| `DeleteObject` | 브러시·DC는 TRUE로 지움. stock 객체는 TRUE이지만 그대로 남음. 0·없는 handle은 FALSE. last error 그대로 / *brushes and DCs: TRUE, deleted; stock objects: TRUE but kept; 0 or unknown: FALSE; last error unchanged* |
| `FillRect` 색 / color | 채널별 버림(`0x7F0000` → `0x000F`, `0x848484` → `0x8430`). 팔레트 COLORREF는 기본 팔레트의 가장 가까운 색 / *each channel truncated; palette COLORREFs take the nearest default palette color* |
| `FillRect` 사각형 / rectangle | 정렬(뒤집힌 사각형도 같은 픽셀), bitmap에 맞게 잘림, 빈 사각형은 그대로. 1을 돌려주고 last error 그대로 / *ordered (an inverted one fills the same pixels), clipped, an empty one untouched; returns 1, last error unchanged* |
| `FillRect` 예외 / edge cases | 브러시 아닌 handle: 1, 칠하지 않음. 없는 DC: 0과 6. NULL 사각형: access violation. 시스템 색 index: 그 색 / *no brush: 1, nothing filled; no DC: 0 and 6; null rectangle: access violation; system color index: that color* |
| `SetTextColor` | 이전 색(처음 0), 새 값 그대로 보관. 없는 DC: `CLR_INVALID`와 6 / *previous color (0 at first), the new one kept; no DC: `CLR_INVALID` and 6* |
| `SetBkMode` | 이전 모드(처음 2), 새 값을 범위와 상관없이 보관. 없는 DC: 0과 6 / *previous mode (2 at first), the new one kept whatever its value; no DC: 0 and 6* |
| 메모리 DC 기본 글꼴 / default font | `System`, 높이 16, 한글 charset 129 / *`System`, 16 high, Hangul charset 129* |
| `DrawTextA` 반환 / result | 사각형 위에서 글자 아래까지: TOP 16, VCENTER (h−16)/2+16, BOTTOM h / *from the rectangle's top to the text's bottom* |
| `DrawTextA` last error | 글자를 그리면 0. 개수 0과 NULL 글자는 0을 돌려주고 그대로. 빈 문자열은 offset을 돌려주고 그대로. 없는 DC는 0과 87 / *0 when text is drawn; a zero count or null text: 0, unchanged; an empty string: the offset, unchanged; no DC: 0 and 87* |

## 결정 / Decisions

1. **GDI 모델.** `GuestGdi`에 solid brush(handle → COLORREF)를 더한다. `Delete`는 DC, bitmap, 브러시를 지운다.
   ***The GDI model.** `GuestGdi` gains solid brushes (handle → COLORREF), and `Delete` covers DCs, bitmaps, and brushes.*
2. **gdi32.** `CreateSolidBrush`, `DeleteObject`, `SetTextColor`, `SetBkMode`를 측정대로 구현한다.
   ***gdi32:** `CreateSolidBrush`, `DeleteObject`, `SetTextColor`, and `SetBkMode` as measured.*
3. **user32 `FillRect`.** `gdi_raster`에 `FillArea`(정렬과 잘림)와 COLORREF 배치(`kGdiColorref`)를 둔다. 색은 기존 `ConvertGdiPixel`로 좁힌다.
   - stock 브러시는 그 색을 쓰고, `NULL_BRUSH`는 칠하지 않는다.
   - 다음은 모델 밖이라 멈춘다: 시스템 색 index, 팔레트 COLORREF, 16비트가 아닌 bitmap, NULL 사각형(Windows에서는 fault).

   ***user32 `FillRect`.** `gdi_raster` gains `FillArea` (ordering and clipping) and the COLORREF layout (`kGdiColorref`); colors narrow through the existing `ConvertGdiPixel`.*
   - *Stock brushes fill with their colors; `NULL_BRUSH` fills nothing.*
   - *These are unmodelled and stop: a system color index, a palette COLORREF, a bitmap other than 16-bit, and a null rectangle (a fault on Windows).*
4. **user32 `DrawTextA`.** 기본 글꼴의 한 줄에 대해 반환값과 last error를 측정대로 돌려준다. 가로는 왼쪽·가운데·오른쪽, 세로는 위·가운데·아래를 받는다. (글자 그리기는 [작업 402](20260927-402-drawtext-unifont-glyphs.md)에서 Unifont로 더함)
   - **글자 픽셀은 그리지 않는다.** `System` 글꼴의 bitmap은 Microsoft 자산이라 저장소에 넣을 수 없고, 모델에는 자체 글꼴이 없다. 그래서 대체 텍스처는 파란 바탕만 있고 흰 글자 `temp`가 빠진다. Windows와 다른 이 점은 TODO에 남긴다.
   - `DT_CALCRECT`, 여러 줄, 단어 줄바꿈 등 다른 형식은 멈춘다.

   ***user32 `DrawTextA`.** For one line in the default font it returns the measured result and last error, taking left, centred, or right and top, centred, or bottom placement.*
   - ***The glyphs are not drawn.** The `System` font's bitmaps are Microsoft's and cannot go into the repository, and the model has no font of its own, so the stand-in texture has its blue background without the white `temp`. This departure from Windows is left in the TODO.*
   - *Other formats (`DT_CALCRECT`, several lines, word breaks, and so on) stop.*
5. **기록.** continuation이 `DrawTextA`의 글자를 API 기록에 남긴다. / ***Logging.** The continuation records `DrawTextA`'s text in the API log.*

## 범위 밖 / Out of scope

- 글꼴과 글자 그리기, `SelectObject`, `BitBlt`, `CreateCompatibleDC`, `CreateDIBSection`, 팔레트. / *Fonts and glyph drawing, `SelectObject`, `BitBlt`, `CreateCompatibleDC`, `CreateDIBSection`, and palettes.*
- `kernel32!GetFileAttributesA`: 다음 텍스처를 올린 뒤 멈추는 곳이다. 다음 작업이다. / *`kernel32!GetFileAttributesA`, where the run stops after loading the next textures: the next task.*
