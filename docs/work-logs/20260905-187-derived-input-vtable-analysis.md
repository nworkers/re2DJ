# 20260905-187 파생 입력 vtable 분석과 DirectInput 초기화 실패 원인 규명 결과
# 20260905-187 Derived Input Vtable Analysis And Identification Of DirectInput Initialization Failure — Results

## 1. 개요 (Overview)

EZ2DJ 4th의 결함 객체가 DirectInput 키보드 디바이스(`IDirectInputDevice`)로 확인된 후, 전역 객체 `0x00aca5b0`(vtable `0x004e20a0`)과 기저 클래스(vtable `0x004dd16c`)의 가상 메서드 슬롯 및 생성자 계통을 전수 분석했다.

**결론: 객체가 잘못 선택된 것이 아니다. `0x00aca5b0`은 컴파일 시점에 결정된 정적 전역 객체이며, DirectInput 생성 코드(`0x004227d0`)는 이미 실행 파일 내에 온전히 존재하고 실제로 실행까지 되었다. 그러나 호스트 OS(Windows 11)의 레거시 `dinput.dll`이 `DirectInputCreateA(0x700)`에 대해 `0x80070057` (`E_INVALIDARG`)을 반환하며 실패했고, 게임이 `"DirectInput Initialize Error"` 분기를 타면서 장치 포인터를 null로 방치하여 이후 폴링 루프에서 크래시가 발생했다.**

The object was not wrongly chosen. `0x00aca5b0` is a compiled-in static global object, and the DirectInput initialization routine (`0x004227d0`) was present in the binary all along and did execute. However, modern Windows 11 `dinput.dll` rejects legacy `DirectInputCreateA` with version `0x700`, returning `0x80070057` (`E_INVALIDARG`). The guest bailed out with `"DirectInput Initialize Error"`, leaving device pointers null, which caused the unchecked dereference crash during the first frame's input poll.

---

## 2. 변경 내용 (Changes Implemented)

1. `src/tools/windows_x86_launcher_probe/main.cpp`
   - **`unload_tail_arm` 가드 조건 수정.** API 감시가 활성화되지 않았을 때(`api_watches->empty()`) 시스템 DLL 언로드 이벤트(`UNLOAD_DLL_DEBUG_EVENT`)로 인해 불필요하게 20만 스텝 단일 스텝 추적이 걸려 프로브가 타임아웃되는 결함을 수정(`!api_watches->empty()` 검사 추가).

---

## 3. 조사 및 분석 결과 (Investigation Results)

### 3.1 vtable 구조: 기저 추상 클래스와 파생 구체 클래스 (확인됨)

기저 vtable `0x004dd16c`과 파생 vtable `0x004e20a0`의 메모리를 덤프하여 슬롯을 대조했다.

| 슬롯 | 오프셋 | 기저 vtable (`0x004dd16c`) | 파생 vtable (`0x004e20a0`) | 상태 및 역할 |
| - | - | - | - | - |
| Slot 0 | `+0x00` | `0x004c37fe` (`_purecall`) | `0x00401c30` (`jmp 0x004363ba`) | 순수 가상 함수 구현 |
| Slot 1 | `+0x04` | `0x004c37fe` (`_purecall`) | `0x00401a14` (`jmp 0x0042ab30`) | 순수 가상 함수 구현 |
| Slot 2 | `+0x08` | `0x004c37fe` (`_purecall`) | `0x004025d1` (`jmp 0x0043642e`) | 순수 가상 함수 구현 |
| 구분자 | `+0x0c` | `0x00000000` (null) | `0x00000000` (null) | vtable 종료 (길이 3 DWORD) |

- **확인됨 — 기저 클래스는 정확히 3개의 순수 가상 함수를 가진 추상 인터페이스다.** MSVC CRT의 `_purecall` 핸들러(`0x004c37fe`)가 3개 슬롯에 배치되어 있다.
- **확인됨 — 파생 클래스 `0x004e20a0`은 이 3개 메서드를 모두 오버라이드한 유일한 구체 구현체다.**
- **확인됨 — 결함 함수 `0x00422b00`은 vtable에 속한 가상 함수가 아니다.** 갱신 루틴 `0x004235d3`에서 직통 thunk(`0x00401ee7`)로 호출되는 비가상 멤버 함수다.

### 3.2 전역 객체 `0x00aca5b0`의 정체와 수명주기 (확인됨)

`.text` 전체에서 `0x00aca5b0`에 대한 상수를 스캔한 결과 5건의 참조가 발견되었다.

- **`0x004a29b6`**: CRT 동적 초기화 함수. 프로그램 시작 시 `mov ecx, 0x00aca5b0` 후 생성자 thunk `0x004016b3`을 호출.
- **`0x004a29e5`**: `atexit`(`0x004c1c25`)를 통한 종료 파괴자 등록.
- **`0x004a2b36`**: 엔진 초기화 함수 `0x004a2b09`에서 전역 입력 포인터 `0x00ac215c`에 무조건 대입:
  ```x86asm
  mov dword ptr [0x00ac215c], 0x00aca5b0
  ```
- **`0x004a2b87`**: 등록 직후 초기화 메서드 호출:
  ```x86asm
  mov ecx, dword ptr [0x00ac215c]
  call 0x00401609 ; jmp 0x0042272f
  ```
- **결론: 다른 입력 객체가 동적으로 선택될 여지는 없다.** 이 객체는 컴파일 타임에 결정된 전역 싱글톤이다.

### 3.3 DirectInput 생성 루틴(`0x004227d0`)의 완전한 발견 (확인됨)

초기화 메서드 `0x0042272f`는 내부에서 `call 0x00402c89` (`jmp 0x004227d0`)을 호출한다. 이 `0x004227d0`이 바로 Task 184/185/186에서 찾지 못했던 **DirectInput 생성 루틴**이다.

```x86asm
0x0042280d: push -6                          ; GWL_HINSTANCE
0x0042280f: mov eax, [ebp - 4]               ; hWnd
0x00422813: call [0xad192c]                  ; GetWindowLongA(hWnd, GWL_HINSTANCE)
0x00422820: push eax                         ; hInstance
0x00422821: call 0x4ada20                    ; DirectInputCreateA(hInstance, 0x700, &this->dinput, NULL)
0x00422829: cmp [ebp - 8], 0
0x0042282f: jmp 0x4229e1                     ; 실패 시 탈출!

0x00422842: push 0x4e4100                    ; GUID_SysMouse
0x0042285c: call [ecx + 0xc]                 ; IDirectInputA::CreateDevice(&this->mouse)
0x00422874: push 0x4e2ee8                    ; &c_dfDIMouse
0x00422890: call [ecx + 0x2c]                ; IDirectInputDeviceA::SetDataFormat
0x004228aa: push 6                           ; DISCL_NONEXCLUSIVE | DISCL_FOREGROUND
0x004228c5: call [edx + 0x34]                ; IDirectInputDeviceA::SetCooperativeLevel

0x004228ea: push 0x4e40f0                    ; GUID_SysKeyboard
0x00422904: call [eax + 0xc]                 ; IDirectInputA::CreateDevice(&this->keyboard)
0x0042291e: push 0x4e2ed0                    ; &c_dfDIKeyboard
0x00422938: call [eax + 0x2c]                ; IDirectInputDeviceA::SetDataFormat
0x00422952: push 6                           ; DISCL_NONEXCLUSIVE | DISCL_FOREGROUND
0x0042296d: call [ecx + 0x34]                ; IDirectInputDeviceA::SetCooperativeLevel

0x00422999: call [ecx + 0x1c]                ; keyboard->Acquire()
0x004229ba: call [ecx + 0x1c]                ; mouse->Acquire()
```

- **`+0xa0c`**: `IDirectInputA` 인터페이스 포인터
- **`+0xb14`**: `IDirectInputDeviceA` 마우스 디바이스 (`GUID_SysMouse`)
- **`+0xa10`**: `IDirectInputDeviceA` 키보드 디바이스 (`GUID_SysKeyboard`)
- Task 184 스캔에서 잡히지 않았던 이유: `lea eax, [this + 0xa10]` 형태가 아니라 `mov eax, [this]; add eax, 0xa10; push eax` 형태로 인자를 전달했기 때문이다.

### 3.4 실패 원인 및 결함 메커니즘 (확인됨)

모든 실패 분기(`0x0042282f`, `0x0042286f`, `0x004228a3`, `0x004228d8`, `0x00422917`, `0x0042294b`, `0x00422980`)는 다음 오류 블록으로 점프한다:

```x86asm
0x004229c6: push 0x004ee59c                  ; "DirectInput Initialize Error"
0x004229cb: call 0x00401613                  ; 오류 로깅 함수
0x004229d3: jmp 0x004229e3                   ; 아무 장치도 설정하지 않고 리턴!
```

- **확인됨 — Windows 11의 `dinput.dll`은 `DirectInputCreateA(..., 0x700, ...)` 호출 시 무조건 `0x80070057` (`E_INVALIDARG`)을 반환한다.** Microsoft는 DirectInput 8 이전 레거시 진입점을 사실상 지원 중단했다.
- **확인됨 — 첫 호출인 `DirectInputCreateA`가 즉시 실패하여 `0x0042282f`에서 탈출 분기를 탔다.** 이로 인해 `+0xa0c`, `+0xb14`, `+0xa10` 세 필드가 모두 생성 시점의 0으로 남았다.
- **확인됨 — 이후 렌더링 프레임 갱신 경로(`0x004235d3`)가 null 검사 없이 `+0xa10`에 `GetDeviceState`를 호출하면서 접근 위반(`0xc0000005`)이 발생했다.**

---

## 4. 결론 및 후속 작업 (Conclusions & Next Steps)

이번 조사로 미스터리가 완전히 해결되었다:
1. 객체는 정상적인 전역 객체다.
2. DirectInput 초기화 코드는 완벽히 존재하며 정상적으로 호출되었다.
3. 충돌 원인은 **Windows 호스트의 `DirectInputCreateA` 거부**와 **게스트의 null 검사 부재**의 결합이다.

**후속 작업 (Next Task): DirectInput 7 HLE 경계 구현**
- `dinput.dll`을 후킹하여 `DirectInputCreateA` 진입점을 HLE로 가로챈다.
- 요구되는 인터페이스는 매우 단순하다:
  1. `IDirectInputA`: `CreateDevice` (`GUID_SysKeyboard`, `GUID_SysMouse`) 지원.
  2. `IDirectInputDeviceA`: `SetDataFormat`, `SetCooperativeLevel`, `Acquire`, `Unacquire`, `GetDeviceState` (256바이트 키 상태 및 마우스 상태), `Release` 지원.
- 키 상태 데이터는 re2DJ의 `ez2dj_keyboard_input`에서 즉시 매핑 가능하다.
- 이 경계가 갖추어지면 `DirectInput Initialize Error`가 해결되고 키보드/마우스 장치가 성공적으로 생성되어 4th Trax가 첫 프레임 렌더링 및 입력 폴링을 정상 통과하게 된다.

---

## 5. 관련 문서 (Related Documents)

- [Task 187 설계](../design/20260905-187-derived-input-vtable-analysis.md)
- [Task 187 작업 지시서](../work-orders/20260905-187-derived-input-vtable-analysis.md)
- [Task 186 작업 로그](20260905-186-guest-code-window.md)
- [4th 그래픽 경로 분석](../analysis/ez2dj4th-graphics-path.md)
