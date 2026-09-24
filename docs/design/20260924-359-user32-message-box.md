# 작업 359 설계 — `user32!MessageBoxA` facade export / Task 359 design — `user32!MessageBoxA` facade export

선행: [작업 358 작업 로그](../work-logs/20260924-358-kernel32-exit-process.md), [작업 352 `user32` facade](20260923-352-user32-facade-module.md)

## 배경 / Background

실제 4th CHD의 in-process 실행은 두 host 폭 모두 `#0019 user32.dll!MessageBoxA`에서 멈춘다. 이 호출은 image의 정적 import인데 `user32` facade에 export가 없어서, IAT slot이 facade thunk로 재결합되지 않고 미처리로 남는다([분석](../analysis/ez2dj4th-linux-inprocess-first-import.md)). 이 호출의 문구와 그 뒤 동작(작업 358에서 추가한 `ExitProcess` 호출 여부)을 보려면 facade가 `MessageBoxA`를 제공해야 한다.

*On the real 4th CHD, the in-process run stops on both host widths at `#0019 user32.dll!MessageBoxA`. It is a static import of the image; with no export in the `user32` facade, its IAT slot is not rebound to a facade thunk and stays unhandled ([analysis](../analysis/ez2dj4th-linux-inprocess-first-import.md)). The facade must provide `MessageBoxA` to observe its text and what follows, including whether the `ExitProcess` added in Task 358 is called.*

## 결정 / Decision

1. **export.** `MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)`는 stdcall, 인자 4개다([Microsoft Learn](https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-messageboxa)).
   ***Export.** `MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)`, stdcall with four arguments ([Microsoft Learn](https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-messageboxa)).*
2. **반환값.** 아직 창을 보여 줄 platform 서비스가 없다. 그래서 사용자가 기본 버튼을 누른 것으로 보고, 그 버튼의 ID를 돌려준다. 버튼 종류는 `uType & MB_TYPEMASK(0xF)`로, 기본 버튼 위치는 `uType & MB_DEFMASK(0xF00)`로 정한다. 알 수 없는 종류와 범위를 벗어난 기본 버튼 위치는 첫 버튼으로 본다. `MB_OK`에서는 `IDOK`(1)이 되어, Windows runtime의 `message_box_boundary`가 돌려주는 값과 같다.
   ***Return value.** No platform service can show the box yet, so the result is the ID of the default button, as if the user accepted it: the button set comes from `uType & MB_TYPEMASK (0xF)` and the default position from `uType & MB_DEFMASK (0xF00)`, with unknown sets and out-of-range positions taking the first button. For `MB_OK` this is `IDOK` (1), matching what the Windows runtime's `message_box_boundary` returns.*

   | `uType & 0xF` | 버튼 / Buttons |
   | --- | --- |
   | 0 `MB_OK` | `IDOK` |
   | 1 `MB_OKCANCEL` | `IDOK`, `IDCANCEL` |
   | 2 `MB_ABORTRETRYIGNORE` | `IDABORT`, `IDRETRY`, `IDIGNORE` |
   | 3 `MB_YESNOCANCEL` | `IDYES`, `IDNO`, `IDCANCEL` |
   | 4 `MB_YESNO` | `IDYES`, `IDNO` |
   | 5 `MB_RETRYCANCEL` | `IDRETRY`, `IDCANCEL` |
   | 6 `MB_CANCELTRYCONTINUE` | `IDCANCEL`, `IDTRYAGAIN`, `IDCONTINUE` |

3. **관찰.** handler는 문자열을 읽지 않는다. continuation은 facade export의 ANSI 문자열 인자를 **두 개까지** 기록한다. `MessageBoxA`에서는 문구(인자 1)와 제목(인자 2)이다. 게스트 문자열은 CP949일 수 있다. [설계 307](20260918-307-linux-x86-x64-wsl.md)의 방침대로 무조건 UTF-8로 해석하지 않는다. CLI는 출력할 때 비ASCII byte를 `\xNN`으로 표기해 원래 byte를 그대로 남긴다.
   ***Observation.** The handler reads no strings; the continuation records **up to two** ANSI string arguments of a facade export — for `MessageBoxA`, the text (argument 1) and caption (argument 2). Guest strings may be CP949, so per [design 307](20260918-307-linux-x86-x64-wsl.md) they are not assumed to be UTF-8; the CLI prints non-ASCII bytes as `\xNN`, keeping the original bytes.*

## 검증 / Validation

- 단위 테스트: descriptor에 `GetActiveWindow`, `MessageBoxA`가 있다. 각 버튼 종류의 기본 버튼, `MB_DEFBUTTON2/3`, 범위 밖 기본 버튼, 인자 형태 거절을 검사한다.
  *Unit tests: the descriptor has `GetActiveWindow` and `MessageBoxA`; check each button set's default, `MB_DEFBUTTON2/3`, an out-of-range default, and argument-shape rejection.*
- 실제 4th CHD(두 폭): `#0019`가 처리된다. 문구·제목·`uType`을 기록하고 다음 경계를 확인한다. 기존 진단과 합성 probe에 회귀가 없어야 한다.
  *Real 4th CHD (both widths): `#0019` is handled, its text, caption, and `uType` are recorded, and the next boundary is observed; existing diagnostics and synthetic probes do not regress.*
