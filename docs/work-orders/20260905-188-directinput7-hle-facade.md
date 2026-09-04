# Task 188: DirectInput 7 HLE 경계 구현

## 작업 목표

DirectInput 7 HLE 인터페이스(`IDirectInputA`, `IDirectInputDeviceA`) Facade를 구현하고, 게스트의 `DirectInputCreateA` 호출을 가로채어 키보드 및 마우스 장치 상태를 제공함으로써 초기화 실패 및 첫 프레임 입력 폴링 크래시를 해결합니다.

## 선행 문서

- [Task 188 설계](../design/20260905-188-directinput7-hle-facade.md)
- [Task 187 작업 로그](../work-logs/20260905-187-derived-input-vtable-analysis.md)
- [4th 그래픽 경로 분석](../analysis/ez2dj4th-graphics-path.md)

## 구현 범위

1. **DirectInput 7 COM Facade 구현**:
   - `src/platform/windows/directinput7_com_facade.h`
   - `src/platform/windows/directinput7_com_facade.cpp`
   - 진입점 `Re2djHleDirectInputCreateA` 구현:
     `IDirectInputA` 및 `IDirectInputDeviceA` (Keyboard, Mouse) 인터페이스 제공.
   - 키보드 `GetDeviceState`: `GetAsyncKeyState` + `MapVirtualKeyA` 기반 256바이트 DirectInput Scan Code 배열 생성.

2. **런타임 및 프로브 연동**:
   - `src/platform/windows/injected_runtime.cpp`:
     `Re2djHookedGetProcAddress`에 `"DirectInputCreateA"` 분기 추가 및 진입점 export.
   - `src/tools/windows_x86_launcher_probe/main.cpp`:
     언팩 완료 후 IAT 슬롯(`0x00ad1634`)에 `Re2djHleDirectInputCreateA` 주소 기록 지원.
   - `CMakeLists.txt`:
     `re2dj_windows_injected_runtime` 타깃에 `src/platform/windows/directinput7_com_facade.cpp` 추가.

3. **빌드 및 검증**:
   - `scripts/build_win32.bat` 빌드.
   - `re2dj_unit_tests.exe` 및 `re2dj_windows_product_loader_probe.exe` 실행.
   - `re2dj_windows_x86_launcher_probe.exe`로 EZ2DJ 4th 실행하여 DirectInput 생성 성공 및 첫 프레임 진입 검증.

## 비범위

- DirectInput 포스 피드백(Force Feedback) 구현.
- DirectInput 조이스틱(Joystick) 장치 구현 (현재 4th 게스트는 Keyboard와 Mouse만 요청함이 확인됨).
- Linux / Web 플랫폼의 DirectInput 추상화 (본 태스크는 Windows x86 네이티브 주입 런타임 대상).

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
.\build\windows-x86\bin\Debug\re2dj_windows_product_loader_probe.exe
```

```powershell
$common = '--hdd', "$env:TEMP\re2dj\chd\ez2dj4th", '--target', 'ez2dj4th',
  '--chd', 'roms\ez2dj4th\4thTrax.chd',
  '--target-executable', 'EZ2DJ/EZ2DJ.EXE',
  '--hle-vfs', '--hle-dynamic-vfs', '--hle-d3d3', '--hle-io-ports',
  '--device-mock-lptdi', '--device-mock-lptdi-path-prefix', '\\.\FEnteDev',
  '--device-mock-wts-console-session', '--diagnostic-idle-timeout', '60000'
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe @common
```

## 자기 검증 기준

- `0x004227d0` 실행 중 `"DirectInput Initialize Error"`가 발생하지 않아야 합니다.
- 전역 객체의 `+0xa0c`, `+0xb14`, `+0xa10` 필드가 0이 아닌 HLE Facade 포인터로 채워져야 합니다.
- `0x00422b3a`의 `GetDeviceState` 호출이 접근 위반 없이 정상 복귀해야 합니다.

---

# Task 188: DirectInput 7 HLE Boundary Implementation

## Goal

Implement the DirectInput 7 HLE COM facade (`IDirectInputA`, `IDirectInputDeviceA`), intercept guest calls to `DirectInputCreateA`, and provide keyboard and mouse device states to resolve the initialization failure and the first-frame input poll crash.

## Scope

1. Create `directinput7_com_facade.h` and `directinput7_com_facade.cpp` implementing `IDirectInputA` and `IDirectInputDeviceA` (Keyboard, Mouse).
2. Wire `Re2djHleDirectInputCreateA` into `injected_runtime.cpp`'s `GetProcAddress` hook and export list.
3. Patch `0x00ad1634` in `windows_x86_launcher_probe` post-unpacking.
4. Add the new source to `CMakeLists.txt`.
5. Build and verify with unit tests and launcher probe runs.

## Out Of Scope

Force feedback, joystick devices (not requested by 4th), and Linux/Web platforms.

## Verification

Clean build, pass unit tests and product loader probe, run launcher probe on EZ2DJ 4th, and confirm non-zero device pointers and successful `GetDeviceState` calls.
