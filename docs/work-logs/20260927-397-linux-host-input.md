# 작업 397 작업 로그 — Linux host 입력 / Task 397 work log — Linux host input

설계: [20260927-397-linux-host-input.md](../design/20260927-397-linux-host-input.md)
작업 지시서: [20260927-397-linux-host-input.md](../work-orders/20260927-397-linux-host-input.md)

## 진행 / Progress

합성 SDL 이벤트 프로브(scratchpad의 `inprobe397`, x64)로 입력 상태의 대응을 확인했다.

*A probe feeding synthetic SDL events (`inprobe397` in the scratchpad, x64) checked how events map into the input state:*

| 입력 / Input | VK | DIK | 기타 / Other |
| --- | --- | --- | --- |
| Z + 왼쪽 Shift / left Shift | 10 5A A0 | 2A 2C | |
| 오른쪽 Shift, 왼쪽 뗌 / right Shift, left released | 10 5A A1 | 2C 36 | |
| 숫자패드 Enter + F5 + 숫자패드 . / keypad Enter + F5 + keypad . | 0D 5A 6E 74 | 2C 3F 53 9C | |
| 창 (640,480) 이동 + 오른쪽 버튼 / move to (640,480) + right button | 02 추가 / *02 added* | | 버튼 010, 커서 (320,240) / *buttons 010, pointer (320,240)* |
| 포커스 잃음 / focus lost | 없음 / *none* | 없음 / *none* | 커서만 남음 / *only the pointer kept* |

실제 실행에서는 확인 도중 사용자가 Linux 창에서 직접 키를 눌러 코인을 넣고 게임에 들어갔다. 실행은 약 29초 동안 이어졌다. 게임은 스타일 선택 화면의 자원(`STYLE_SELECT.str`, `style_mask.abm`, `style_streetmix1.abm`)을 읽은 뒤 호출 298,866번째의 `kernel32!FindFirstFileA`에서 멈췄다. 가만히 둔 Windows 실행(60초)도 데모 중 `FindFirstFileA`를 쓴다. 따라서 이 API는 입력과 무관한 다음 경계다.

XTest(`SDL_VIDEO_DRIVER=x11`)로 키를 보내는 스크립트는 창을 찾기 전에 실행이 끝나서 키를 보내지 않았다. 입력 확인은 위의 사용자 조작이다.

*In the real run, a user pressed keys in the Linux window during the check, inserted coins, and entered the game. The run went on for about 29 seconds: the game read the style select screen's resources (`STYLE_SELECT.str`, `style_mask.abm`, `style_streetmix1.abm`) and then stopped at call 298,866, `kernel32!FindFirstFileA`. An untouched Windows run (60 seconds) also uses `FindFirstFileA` during its demo, so this API is the next boundary, unrelated to input.*

*A script sending keys through XTest (`SDL_VIDEO_DRIVER=x11`) sent none, because the run had ended before it found the window; the input check is the user's own presses above.*

## 변경 / Changes

- **core**:
  - `re2dj/input/virtual_keys.h`(.cpp): VK 상수, `ParseKeyName`.
  - `re2dj/input/ez2dj_keyboard_map.h`(.cpp): 바인딩 표, `Ez2DjTurntables`.
  - `re2dj/hle/host_input.h`.
  - `graphics::FitPresentation`. backend의 `Present`도 이것을 쓴다.

  ***core:***
  - *`re2dj/input/virtual_keys.h` (.cpp): VK constants and `ParseKeyName`.*
  - *`re2dj/input/ez2dj_keyboard_map.h` (.cpp): the binding table and `Ez2DjTurntables`.*
  - *`re2dj/hle/host_input.h`.*
  - *`graphics::FitPresentation`, which the backend's `Present` also uses.*
- **Windows**:
  - `keyboard_input_common.cpp`: VK static_assert, `ParseKey`가 core를 부른다.
  - `Ez2DjKeyboardInput`: core의 표와 턴테이블을 쓴다.

  ***Windows:***
  - *`keyboard_input_common.cpp`: VK static_asserts, with `ParseKey` calling the core.*
  - *`Ez2DjKeyboardInput`: uses the core's table and turntables.*
- **HLE**:
  - `HostPresentation::Input()`.
  - `GetAsyncKeyState`는 0x8000을 돌려준다.
  - `GetCursorPos`는 host 커서를 쓴다.
  - DirectInput `GetDeviceState`는 host 상태를 쓴다.

  ***HLE:***
  - *`HostPresentation::Input()`.*
  - *`GetAsyncKeyState` returns 0x8000 for a held key.*
  - *`GetCursorPos` uses the host pointer.*
  - *DirectInput `GetDeviceState` uses the host state.*
- **Linux**:
  - `host_keyboard.cpp`: SDL scancode를 VK와 DIK로 바꾼다.
  - `LinuxHostPresentation`: 키·마우스·커서·포커스 이벤트를 처리한다.
  - `native_legacy_io`: 읽기마다 기본 키 배치로 폴링한다.

  ***Linux:***
  - *`host_keyboard.cpp`: SDL scancodes to VKs and DIKs.*
  - *`LinuxHostPresentation`: key, mouse, pointer, and focus events.*
  - *`native_legacy_io`: a poll with the default key map on every read.*
- **테스트 / tests**:
  - `ez2dj_keyboard_map_test.cpp`: 키 이름, 기본값과 예제 INI의 일치, 턴테이블.
  - user32(`GetAsyncKeyState`, `GetCursorPos`)와 dinput(키보드·마우스 상태) host 입력 사례.
  - `InputPresentation` 가짜.
  - `RE2DJ_TEST_SOURCE_DIR` 정의.

  ***Tests:***
  - *`ez2dj_keyboard_map_test.cpp`: key names, the defaults against the example INI, and the turntables.*
  - *Host-input cases for user32 (`GetAsyncKeyState`, `GetCursorPos`) and dinput (keyboard and mouse state).*
  - *The `InputPresentation` fake.*
  - *The `RE2DJ_TEST_SOURCE_DIR` definition.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Windows x86 build, CTest | exit 0, 6/6 |
| Windows 실제 4th, 기준 `e621d9b`와 30초씩 / real 4th vs base `e621d9b`, 30 s each | 정규화한 ddraw 기록 1,410줄과 IO 포트·키 설정 기록이 같다 / *the normalized ddraw logs (1,410 lines) and the I/O port and key configuration lines match* |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3(unit checks 4,087) / *no warnings or errors, 3/3 each (4,087 unit checks)* |
| Linux `--call-limit 32768`, 두 폭 / both widths | 호출 32,768번에서 멈춤, hardlock 83, IO 읽기 3,777·쓰기 2,516 / *stops at 32,768 calls; hardlock 83; IO reads 3,777, writes 2,516* |
| 실제 입력 / real input | 사용자 조작으로 코인 투입 → 스타일 선택 화면 / *coins inserted by the user → the style select screen* |

## 다음 / Next

`kernel32!FindFirstFileA`다(곡 폴더 목록). 그 뒤 Linux 소리 출력과 `--io-config`로 이어진다.

*Next is `kernel32!FindFirstFileA` (song folder listing), then Linux sound output and `--io-config`.*
