# SDL3 게임패드 입력 / SDL3 gamepad input

이 문서는 SDL3의 gamepad 서브시스템을 일반 배경 지식으로 정리한다. 프로젝트의 연결 방식은 [작업 444 설계](../design/20261003-444-gamepad-input.md)에 둔다.

## 조이스틱과 게임패드 / Joystick and gamepad

SDL3는 입력 장치를 두 층으로 본다. **joystick**은 장치의 버튼·축·hat을 번호로만 노출한다. **gamepad**는 그 위에 Xbox 모양의 공통 배치(face 버튼 south/east/west/north, 숄더, 트리거, 스틱, 십자키, 패들)를 올린 것으로, 장치별 매핑 데이터베이스가 번호를 배치로 바꾼다. `SDL_INIT_GAMEPAD`는 `SDL_INIT_JOYSTICK`을 포함한다. 비디오 서브시스템 없이도 초기화되므로 창이 없는 테스트에서도 쓸 수 있다.

- `SDL_GetGamepads(&count)`: 지금 꽂힌 게임패드의 instance id 배열(`SDL_free`로 해제).
- `SDL_OpenGamepad(id)` / `SDL_CloseGamepad(pad)`.
- `SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_*)`: 눌림 여부. `SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_*)`: `Sint16`. 스틱은 -32768..32767, 트리거는 0..32767.
- 상태는 이벤트 펌프(`SDL_PollEvent`/`SDL_PumpEvents`) 또는 `SDL_UpdateGamepads()`가 갱신한다.
- 장치 이벤트: `SDL_EVENT_GAMEPAD_ADDED`·`SDL_EVENT_GAMEPAD_REMOVED`의 `event.gdevice.which`가 instance id다. 서브시스템 초기화 때 이미 꽂힌 장치에도 ADDED 이벤트가 온다.

*SDL3 sees input devices in two layers. A **joystick** exposes a device's buttons, axes and hats by number only; a **gamepad** lays the common Xbox-shaped layout (south/east/west/north face buttons, shoulders, triggers, sticks, d-pad, paddles) over it through a per-device mapping database. `SDL_INIT_GAMEPAD` implies `SDL_INIT_JOYSTICK` and needs no video subsystem, so it works in windowless tests. The calls are `SDL_GetGamepads` (instance ids, freed with `SDL_free`), `SDL_OpenGamepad`/`SDL_CloseGamepad`, `SDL_GetGamepadButton` and `SDL_GetGamepadAxis` (`Sint16`: sticks -32768..32767, triggers 0..32767); state is refreshed by the event pump or `SDL_UpdateGamepads()`; `SDL_EVENT_GAMEPAD_ADDED`/`REMOVED` carry the instance id in `event.gdevice.which`, and devices already present when the subsystem starts get an ADDED event too.*

## 가상 조이스틱 / Virtual joysticks

`SDL_AttachVirtualJoystick(&desc)`는 하드웨어 없이 조이스틱을 만든다. `SDL_VirtualJoystickDesc`를 `SDL_INIT_INTERFACE`로 초기화하고 `type = SDL_JOYSTICK_TYPE_GAMEPAD`, `nbuttons = SDL_GAMEPAD_BUTTON_COUNT`, `naxes = SDL_GAMEPAD_AXIS_COUNT`로 두면 버튼·축 번호가 gamepad 배치와 1:1로 대응한다. `SDL_GetJoystickFromID(id)`로 얻은 조이스틱에 `SDL_SetJoystickVirtualButton`·`SDL_SetJoystickVirtualAxis`로 값을 넣고 이벤트를 펌프하면 gamepad 쪽에서 읽힌다. `SDL_DetachVirtualJoystick(id)`는 REMOVED 이벤트를 낸다. 이 프로젝트의 `re2dj_sdl3_gamepad_test`가 이 방법으로 reader를 검사한다.

*`SDL_AttachVirtualJoystick(&desc)` makes a joystick without hardware. With the descriptor initialised by `SDL_INIT_INTERFACE` and `type = SDL_JOYSTICK_TYPE_GAMEPAD`, `nbuttons = SDL_GAMEPAD_BUTTON_COUNT`, `naxes = SDL_GAMEPAD_AXIS_COUNT`, its button and axis numbers map one to one onto the gamepad layout; values set through `SDL_SetJoystickVirtualButton`/`SDL_SetJoystickVirtualAxis` on the joystick from `SDL_GetJoystickFromID` show on the gamepad side after a pump, and `SDL_DetachVirtualJoystick` raises REMOVED. This project's `re2dj_sdl3_gamepad_test` checks the reader this way.*

## Linux의 장치 경로 / Device paths on Linux

- **evdev** (`/dev/input/event*`): 커널의 조이스틱 장치. 열거는 libudev(실행 시 `dlopen`; 빌드에 `libudev-dev` 헤더 필요) 또는 inotify로 한다. Steam Input이 만드는 가상 패드(uinput)도 이 경로로 보인다.
- **HIDAPI** (`/dev/hidraw*`): SDL이 특정 패드(PS4/PS5, Switch, Steam Deck 내장 등)를 직접 다루는 드라이버. `SDL_HIDAPI`로 켜고, `SDL_HIDAPI_LIBUSB`는 libusb를 추가로 쓰는 선택지다. udev 규칙이 hidraw 접근을 허용해야 한다.
- 두 경로 모두 실행 파일의 NEEDED에 라이브러리를 더하지 않는다.

*On Linux the kernel's joystick devices are **evdev** (`/dev/input/event*`), enumerated through libudev (`dlopen`ed at run time; the `libudev-dev` header is needed at build time) or inotify; Steam Input's virtual pads (uinput) appear here too. **HIDAPI** (`/dev/hidraw*`) is SDL's set of drivers that speak to particular pads directly (PS4/PS5, Switch, the Steam Deck's built-in controller and more), enabled by `SDL_HIDAPI`, with `SDL_HIDAPI_LIBUSB` an optional libusb path; udev rules must grant hidraw access. Neither path adds a library to the executable's NEEDED.*

## 출처 / Sources

- SDL3 gamepad API: <https://wiki.libsdl.org/SDL3/CategoryGamepad>
- SDL3 joystick API와 가상 조이스틱: <https://wiki.libsdl.org/SDL3/CategoryJoystick>, <https://wiki.libsdl.org/SDL3/SDL_AttachVirtualJoystick>
- SDL 라이선스는 zlib이며 [THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md)에 적혀 있다.
