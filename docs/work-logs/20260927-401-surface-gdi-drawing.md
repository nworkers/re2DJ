# 작업 401 작업 로그 — 표면 DC의 GDI 그리기 / Task 401 work log — GDI drawing into surface DCs

설계: [20260927-401-surface-gdi-drawing.md](../design/20260927-401-surface-gdi-drawing.md)
작업 지시서: [20260927-401-surface-gdi-drawing.md](../work-orders/20260927-401-surface-gdi-drawing.md)

## 진행 / Progress

함수를 하나씩 더하며 코인·시작(XTest) 실행을 되풀이했다. 게임은 `CreateSolidBrush`, `FillRect`, `DeleteObject`, `SetTextColor`, `SetBkMode`, `DrawTextA("temp")` 순서로 지나갔다. 이어서 다음 텍스처들(`StretchDIBits` 32×32 등)을 올리고, 호출 823,925번째의 `kernel32!GetFileAttributesA`에서 멈췄다.

*Adding the functions one at a time and repeating the coins-and-start run (XTest), the game went through `CreateSolidBrush`, `FillRect`, `DeleteObject`, `SetTextColor`, `SetBkMode`, and `DrawTextA("temp")`, then loaded further textures (a 32×32 `StretchDIBits` and others) and stopped at call 823,925, `kernel32!GetFileAttributesA`.*

측정 프로그램 작성 중 두 가지를 겪었다.

- heredoc이 C 문자열의 `\n`을 실제 줄바꿈으로 바꿔 컴파일이 깨졌다(알려진 함정). 편집 도구로 고쳤다.
- NULL 사각형의 `FillRect`는 측정 프로세스를 access violation으로 끝냈다. 그 결과 자체를 측정값으로 기록했다.

*Writing the measuring programs, a heredoc turned a C string's `\n` into a real newline and broke compilation (a known pitfall; fixed with the edit tool), and `FillRect` with a null rectangle ended the measuring process with an access violation, which is recorded as a measurement.*

## 변경 / Changes

- **HLE**:
  - `GuestGdi::AddBrush/FindBrush`. `Delete`가 브러시도 지운다.
  - `gdi_raster`: `GdiRect`, `FillArea`, `kGdiColorref`.

  ***HLE:***
  - *`GuestGdi::AddBrush/FindBrush`, with `Delete` covering brushes.*
  - *`gdi_raster`: `GdiRect`, `FillArea`, and `kGdiColorref`.*
- **gdi32**: `CreateSolidBrush`, `DeleteObject`, `SetTextColor`, `SetBkMode`(해석 전용 목록에서 옮김). / ***gdi32:** `CreateSolidBrush`, `DeleteObject`, `SetTextColor`, and `SetBkMode` (moved out of the resolve-only list).*
- **user32**: `FillRect`, `DrawTextA`(해석 전용 목록에서 옮김, 글자 픽셀은 없음). / ***user32:** `FillRect` and `DrawTextA` (moved out of the resolve-only list; no glyph pixels).*
- **Linux continuation**: `DrawTextA`의 글자를 기록한다. / ***Linux continuation:** records `DrawTextA`'s text.*
- **테스트 / tests**:
  - 표면 DC의 채우기: 색 버림, 정렬·잘림, 빈 사각형, 브러시 아닌 handle, 없는 DC, 멈추는 두 경우.
  - `DeleteObject`, stock 브러시, 글자 색·배경 모드, `DrawTextA`의 offset과 오류.
  - `FillArea`와 COLORREF 변환.
  - user32 커서 테스트가 export를 번호가 아니라 이름으로 찾도록 바꿨다.

  ***Tests:***
  - *Surface DC fills: color truncation, ordering and clipping, an empty rectangle, a handle that is no brush, a DC that is none, and the two stopping cases.*
  - *`DeleteObject`, a stock brush, text color and background mode, and `DrawTextA`'s offsets and errors.*
  - *`FillArea` and the COLORREF conversion.*
  - *The user32 cursor test now finds exports by name rather than by position.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Windows x86 build, CTest | exit 0, 6/6 |
| Windows 실제 4th | 생략했다. `src/platform/windows`에 바뀐 것이 없다. / *Skipped; nothing under `src/platform/windows` changed.* |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 4/4(unit checks 4,368) / *no warnings or errors, 4/4 each (4,368 unit checks)* |
| Linux `--call-limit 32768`, 두 폭 / both widths | 호출 32,768번에서 멈춤, hardlock 83 / *stops at 32,768 calls, hardlock 83* |
| 실제 4th, 코인·시작 / real 4th, coins and start | 대체 텍스처 그리기를 지나 `GetFileAttributesA`에서 멈춤 / *past the stand-in texture, stopping at `GetFileAttributesA`* |

## 다음 / Next

`kernel32!GetFileAttributesA`다. 글자 그리기(글꼴)는 TODO에 남긴다.

*Next is `kernel32!GetFileAttributesA`. Glyph drawing (fonts) is left in the TODO.*
