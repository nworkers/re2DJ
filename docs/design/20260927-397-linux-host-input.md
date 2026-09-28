# 작업 397 설계 — Linux host 입력 / Task 397 design — Linux host input

선행: [작업 384 설계](20260926-384-directinput-entry.md), [작업 385 설계](20260926-385-linux-legacy-io-ports.md), [작업 388 설계](20260926-388-input-queries.md), [작업 396 설계](20260927-396-linux-window-policy.md)

## 배경 / Background

작업 396 뒤 Linux의 4th는 타이틀 화면까지 돌았지만 입력을 받지 못했다. `GetAsyncKeyState`, DirectInput, IO 보드 모두 "아무것도 안 눌림"이었다. Windows 제품은 입력을 네 경로로 넣는다.

| 경로 / Path | Windows 제품 / Windows product |
| --- | --- |
| `GetAsyncKeyState` | 진짜 Win32 / *the real Win32* |
| DirectInput 키보드 / keyboard | `GetAsyncKeyState(vk)`로 모든 키를 보고 `MapVirtualKey`로 DIK scan code / *every key via `GetAsyncKeyState(vk)`, as a DIK scan code via `MapVirtualKey`* |
| DirectInput 마우스 / mouse | 왼쪽·오른쪽·가운데 버튼 / *left, right, middle buttons* |
| IO 보드 / I/O board | 포트를 읽을 때마다 키 이름 표(기본값과 INI 재정의)로 버튼과 턴테이블 갱신 / *on every port read, buttons and turntables from a key-name table (defaults plus INI overrides)* |

키 이름 해석(`ParseKey`), 바인딩 표, 턴테이블 계산은 `src/platform/windows` 안에만 있었다.

*After Task 396 the 4th on Linux ran to its title screen but received no input: `GetAsyncKeyState`, DirectInput, and the I/O board all read "nothing held". The Windows product feeds input along the four paths above. Its key-name parsing (`ParseKey`), binding table, and turntable arithmetic lived only under `src/platform/windows`.*

## 결정 / Decisions

1. **키 식별은 Win32 VK 번호.** `re2dj/input/virtual_keys.h`에 VK 상수와 `ParseKeyName`을 둔다. VK 번호는 Windows 헤더 없이 쓸 수 있는 숫자다. Windows는 `winuser.h` 값과 static_assert로 맞추고, `ParseKey`는 core를 부른다. `F1X`를 F1로 읽는 `atoi` 동작까지 그대로다.
   ***Keys are known by their Win32 VK numbers.** `re2dj/input/virtual_keys.h` holds the VK constants and `ParseKeyName`; the numbers need no Windows headers. Windows checks them against `winuser.h` by static_assert and its `ParseKey` calls the core, down to the `atoi` behaviour that reads `F1X` as F1.*
2. **EZ2DJ 키 배치는 core로.** `re2dj/input/ez2dj_keyboard_map.h`에 다음을 둔다. Windows `Ez2DjKeyboardInput`은 이것을 쓰고, INI 읽기(`GetPrivateProfileString`)만 Windows 쪽에 남는다.
   - 버튼 21개와 턴테이블 방향 4개의 이름과 기본 키. `config/ez2dj-io.example.ini`와 같다는 것을 두 host 공통 테스트로 확인한다.
   - `Ez2DjTurntables`: 8ms마다 step만큼 움직이고, 8비트로 되감긴다.

   ***The EZ2DJ key map moves to the core.** `re2dj/input/ez2dj_keyboard_map.h` holds the following; Windows' `Ez2DjKeyboardInput` uses them and keeps only the INI reading (`GetPrivateProfileString`):*
   - *The names and default keys of the 21 buttons and 4 turntable directions, checked against `config/ez2dj-io.example.ini` by a test on both hosts.*
   - *`Ez2DjTurntables`: a step every 8 ms, wrapping in 8 bits.*
3. **host 입력 상태.** `hle::HostInputState`는 다음을 담는다. `HostPresentation::Input()`으로 읽는다.
   - 눌린 VK. 마우스 버튼은 `VK_LBUTTON`/`RBUTTON`/`MBUTTON` 자리에, 좌우 Shift·Ctrl·Alt는 일반 키에도 표시한다. Windows `GetAsyncKeyState`와 같다.
   - 눌린 DIK scan code.
   - 마우스 버튼.
   - guest 창 위의 커서(게임 해상도 좌표).

   ***The host input state.** `hle::HostInputState`, read through `HostPresentation::Input()`, holds:*
   - *Held VKs. Mouse buttons sit at `VK_LBUTTON`/`RBUTTON`/`MBUTTON`, and left or right Shift, Ctrl and Alt also hold the generic key, as in Windows' `GetAsyncKeyState`.*
   - *Held DIK scan codes.*
   - *The mouse buttons.*
   - *The pointer over the guest window, in the display's coordinates.*
4. **Linux의 입력 원천.** `LinuxHostPresentation`이 SDL 이벤트로 상태를 채운다.
   - SDL scancode를 US 배열·Num Lock 켬 기준의 VK와 DIK로 바꾼다(`host_keyboard.cpp`). 숫자패드 Enter는 VK_RETURN과 DIK 0x9C다.
   - 커서는 공용 `FitPresentation`(backend가 그리는 비율 유지 영역)으로 게임 좌표가 된다. 검은 띠 위에서는 화면 밖 좌표가 된다.
   - 창 단축키(Alt+1 등)의 키도 guest에 보인다. Windows `GetAsyncKeyState`도 그 키를 본다.
   - 창이 포커스를 잃으면 모든 키와 버튼을 놓는다. Windows는 포커스와 상관없이 전역 키 상태를 보지만, Linux 창은 포커스가 있을 때만 키를 받으므로 키가 눌린 채 남지 않게 한다.

   ***Linux's input source.** `LinuxHostPresentation` fills the state from SDL events:*
   - *SDL scancodes become VKs and DIKs for a US layout with Num Lock on (`host_keyboard.cpp`); keypad Enter is VK_RETURN with DIK 0x9C.*
   - *The pointer maps to the display through the shared `FitPresentation`, the aspect-kept area the backend draws; over the bars it falls outside the display.*
   - *A window shortcut's keys (Alt+1 and so on) still reach the guest, as Windows' `GetAsyncKeyState` sees them too.*
   - *Losing focus releases every key and button. Windows reads the global key state regardless of focus, but a Linux window receives keys only while focused, so none are left stuck.*
5. **소비자.**
   - `GetAsyncKeyState`: 눌린 키는 0x8000이다. "마지막 조회 뒤 눌림" 낮은 비트는 모델링하지 않는다. Windows 문서도 믿을 수 없다고 적는다.
   - DirectInput `GetDeviceState`: scan code와 마우스 버튼을 core의 배치로 넘긴다.
   - `GetCursorPos`: 커서가 guest 창 위에 있었으면 그 창 client 원점에 더한 화면 좌표다.
   - IO 보드: Windows처럼 포트를 읽을 때마다 기본 키 배치로 버튼과 턴테이블을 갱신한다. signal handler 안에서 돌기 때문에 키 이름은 실행 전에 VK로 풀어 둔다.

   ***Consumers:***
   - *`GetAsyncKeyState`: a held key reads 0x8000; the "pressed since last query" low bit is not modelled, which Windows documents as unreliable.*
   - *DirectInput `GetDeviceState`: scan codes and mouse buttons through the core's layout.*
   - *`GetCursorPos`: once the pointer has been over the guest window, its position from that window's client origin on the screen.*
   - *The I/O board: as on Windows, buttons and turntables follow the default key map on every port read. This runs in the signal handler, so key names are resolved to VKs before the run.*

## 범위 밖 / Out of scope

- Linux의 `--io-config`(INI 재정의). 공용 INI 읽기가 필요하다. / *`--io-config` on Linux (INI overrides), which needs a shared INI reader.*
- EZ2Dancer의 word 폭 보드. / *EZ2Dancer's word-wide board.*
- 키·마우스 창 메시지(`WM_KEYDOWN` 등), 마우스 이동량(DirectInput `lX`/`lY`). / *Key and mouse window messages (`WM_KEYDOWN` and so on) and mouse movement (DirectInput `lX`/`lY`).*
- `kernel32!FindFirstFileA`: 스타일 선택 화면 뒤에 멈추는 곳이다. 다음 작업이다. / *`kernel32!FindFirstFileA`, where the run stops after the style select screen: the next task.*
