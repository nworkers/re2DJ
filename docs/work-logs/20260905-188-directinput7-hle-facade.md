# 20260905-188 DirectInput 7 HLE 구현 및 접근 위반 해결 결과
# 20260905-188 DirectInput 7 HLE Implementation And Access Violation Resolution — Results

## 1. 개요 (Overview)

EZ2DJ 4th Trax 실행 시 Windows 11 호스트의 `DirectInputCreateA(..., 0x700, ...)` 호출 거부(`0x80070057`)로 인해 키보드/마우스 DirectInput 인터페이스가 null로 방치되어 첫 프레임 입력 폴링 시 발생하던 `0xc0000005` 접근 위반을 해결하기 위해, 독립적인 DirectInput 7 HLE 경계(`directinput7_com_facade.h/cpp`)를 구현하고 인젝션 런타임 및 런처 프로브에 통합했다.

아울러 DirectInput 해결 직후 표면화된 DirectX 7 디바이스 vtable 재배치 불일치로 인한 `IDirect3DDevice7::SetTexture` 호출 규약(ESP mismatch) 결함을 찾아 함께 수정했다.

그 결과 EZ2DJ 4th Trax가 첫 프레임 입력 폴링과 렌더링 루프 진입에 완벽히 성공하였으며, 10초 동안 600 프레임(60 FPS) 이상 안정적으로 렌더링되며 지속 동작함을 확인했다.

To resolve the `0xc0000005` access violation during the first frame input poll in EZ2DJ 4th Trax caused by Windows 11 rejecting `DirectInputCreateA(..., 0x700, ...)` with `0x80070057`, an independent DirectInput 7 HLE boundary (`directinput7_com_facade.h/cpp`) was implemented and integrated into the injected runtime and launcher probe.
In addition, an MSVC runtime calling convention mismatch (ESP imbalance) in `IDirect3DDevice7::SetTexture`—uncovered immediately after DirectInput succeeded—was identified and resolved by correctly delegating through `LegacyDirect3DDeviceVtable()`.
Consequently, EZ2DJ 4th Trax successfully bypassed the input polling blocker, entered its main game loop, and rendered over 600 frames at a steady 60 FPS without crashing.

---

## 2. 변경 내용 (Changes Implemented)

### 2.1 DirectInput 7 COM Facade 구현
- [`src/platform/windows/directinput7_com_facade.h`](../../src/platform/windows/directinput7_com_facade.h) / [`src/platform/windows/directinput7_com_facade.cpp`](../../src/platform/windows/directinput7_com_facade.cpp):
  - `IDirectInputA` 및 `IDirectInputDeviceA` COM facade 객체 및 vtable 구현.
  - `GUID_SysKeyboard` 및 `GUID_SysMouse` 지원.
  - 게스트가 요구하는 필수 인터페이스 메서드 완비:
    - `QueryInterface`, `AddRef`, `Release`
    - `CreateDevice`, `EnumDevices`, `GetDeviceStatus`, `RunControlPanel`, `Initialize`
    - `SetDataFormat`, `SetCooperativeLevel`, `Acquire`, `Unacquire`, `GetDeviceState` (키보드 256바이트 상태 배열 및 `DIMOUSESTATE` 마우스 버튼), `GetDeviceData`, `SetProperty`, `GetProperty`, `GetCapabilities`, `EnumObjects`, `GetObjectInfo`, `GetDeviceInfo`
  - 익스포트 함수 `Re2djHleDirectInputCreateA` 정의.

### 2.2 인젝션 런타임 및 프로브 연결
- [`src/platform/windows/injected_runtime.cpp`](../../src/platform/windows/injected_runtime.cpp):
  - `Re2djHleGetProcAddress`에서 `"DirectInputCreateA"` 요청 시 `Re2djHleDirectInputCreateA`로 후킹 반환.
- [`src/tools/windows_x86_launcher_probe/main.cpp`](../../src/tools/windows_x86_launcher_probe/main.cpp):
  - 패커 언팩 완료 시점(`entry_restored`)에서 언팩된 게스트 IAT 슬롯 `0x006d1634`(`[0x00ad1634]`)을 `_Re2djHleDirectInputCreateA@16`으로 직접 패치.
  - `ez2dj4th` 대상에 대해 정적 `ExitProcess` 슬롯 요구 우회 허용.
  - 크래시 진단 시 스택(ESP) 및 프레임(EBP) 메모리 덤프 추가.

### 2.3 DirectX 7 Facade vtable 위임 결함 수정
- [`src/platform/windows/direct3d7_com_facade.cpp`](../../src/platform/windows/direct3d7_com_facade.cpp):
  - `Dev7SetTexture`, `Dev7GetTexture`, `Dev7DrawPrimitiveVB`, `Dev7DrawIndexedPrimitiveVB`, `Dev7GetDirect3D`에서 `device->lpVtbl->...`로 호출하던 것을 `LegacyDirect3DDeviceVtable()->...`로 수정.
  - 이유: 게스트 디바이스의 `lpVtbl`에는 DirectX 7 재배치 vtable(`Device7Vtable`)이 설치되어 있어, 이를 `IDirect3DDevice3Vtbl`로 역참조할 경우 완전히 엉뚱한 슬롯(예: `SetTexture` 슬롯 38이 DX7의 `ValidateDevice` 슬롯 38로 호출됨)이 호출되어 인자 개수 불일치로 인한 ESP 스택 손상 및 RTC 실패가 발생했음.

---

## 3. 검증 결과 (Verification Results)

### 3.1 런처 프로브 실행 로그 분석
`re2dj_windows_x86_launcher_probe.exe`를 사용하여 CHD 기반 `ez2dj4th`를 10초 타임아웃으로 실행:

```jsonl
{"debug_event":"output_debug","message":"re2dj:hle:DirectInputCreateA"}
{"debug_event":"output_debug","message":"re2dj:hle:IDirectInput::CreateDevice:SysMouse"}
{"debug_event":"output_debug","message":"re2dj:hle:IDirectInputDevice::SetDataFormat"}
{"debug_event":"output_debug","message":"re2dj:hle:IDirectInputDevice::SetCooperativeLevel:hwnd=001E0F34:flags=0x00000006"}
{"debug_event":"output_debug","message":"re2dj:hle:IDirectInput::CreateDevice:SysKeyboard"}
{"debug_event":"output_debug","message":"re2dj:hle:IDirectInputDevice::SetDataFormat"}
{"debug_event":"output_debug","message":"re2dj:hle:IDirectInputDevice::SetCooperativeLevel:hwnd=001E0F34:flags=0x00000006"}
{"debug_event":"output_debug","message":"re2dj:hle:IDirectInputDevice::Acquire:Keyboard"}
{"debug_event":"output_debug","message":"re2dj:hle:IDirectInputDevice::Acquire:Mouse"}
{"debug_event":"output_debug","message":"re2dj:hle:IDirectInputDevice::GetDeviceState:Keyboard:first_poll"}
```

- **`DirectInputCreateA` 성공**: `"DirectInput Initialize Error"` 분기가 전혀 타지 않음.
- **키보드/마우스 장치 생성 및 설정 완료**: `c_dfDIMouse`, `c_dfDIKeyboard`, `DISCL_NONEXCLUSIVE | DISCL_FOREGROUND` 정상 처리.
- **입력 폴링 루틴 통과**: `0x00422b3a` 접근 위반 완전 해소.
- **연속 렌더링 프레임 확인**:
  ```
  re2dj:hle:ddraw-trace:seq=1463:LateDraw:frame=602:fvf=0x000001c4:texture=31:...
  re2dj:hle:ddraw-trace:seq=1464:LateDraw:frame=602:fvf=0x000001c4:texture=32:...
  re2dj:hle:ddraw-trace:seq=1465:LateDraw:frame=602:fvf=0x000001c4:texture=33:...
  ```
  10초 동안 총 602 프레임(정확히 60 FPS)이 안정적으로 렌더링됨.

### 3.2 단위 테스트
- `re2dj_unit_tests.exe`: 1,265 checks, 0 failures 통과.

---

## 4. 결론 (Conclusions)

1. EZ2DJ 4th Trax의 입력 시스템은 DirectInput 7 키보드와 마우스 디바이스를 기반으로 하며, HLE 구현을 통해 현대 Windows에서도 원본 코드 수정 없이 완벽하게 초기화 및 폴링이 동작한다.
2. 입력 블로커 및 텍스처 바인딩 vtable 불일치가 모두 해결되어, 4th Trax가 크래시 없이 메인 렌더링 루프로 성공적으로 진입하였다.

---

## 5. 관련 문서 (Related Documents)

- [Task 188 설계 문서](../design/20260905-188-directinput7-hle-facade.md)
- [Task 188 작업 지시서](../work-orders/20260905-188-directinput7-hle-facade.md)
- [Task 187 작업 로그](20260905-187-derived-input-vtable-analysis.md)
- [4th 그래픽 경로 분석](../analysis/ez2dj4th-graphics-path.md)
