# 20260905-186 게스트 코드 창 기록과 결함 객체 식별 결과
# 20260905-186 Guest Code Window Capture And Identifying The Faulting Object — Results

## 1. 개요 (Overview)

실행 중인 게스트의 임의 주소에서 바이트 창을 읽는 진단을 만들고, 결함 지점과 그 호출 사슬을 해독했다.

**결론: EZ2DJ 4th의 검정화면은 그래픽 문제가 아니다. 결함 필드 `+0xa10`은 `IDirectInputDevice`이고, 게임은 첫 텍스처 업로드 직후 256바이트 키보드 상태를 `GetDeviceState`로 읽으려다 null 인터페이스를 역참조한다. 실행 내내 `dinput.dll`은 적재되지 않는다.**

The black screen is not a graphics problem. The faulting field holds an `IDirectInputDevice`, and the guest dereferences it while polling a 256-byte keyboard state through `GetDeviceState` right after its first texture upload. `dinput.dll` is never loaded in the run.

---

## 2. 변경 내용 (Changes Implemented)

`src/tools/windows_x86_launcher_probe/main.cpp`

1. **`--code-window <hex-address>[:<hex-length>]` 추가.** 반복 지정 가능하며 첫 접근 위반 시점에 지정한 주소들의 바이트를 기록한다. `.text`가 디스크에서 암호문이므로 실행 중 메모리에서만 읽을 수 있다.
2. **기준점 방식.** 지정 주소를 창의 시작이 아니라 중간으로 삼는다. 조사 대상 주소는 대부분 **복귀 주소**이고 그것은 `call`의 다음을 가리키므로, 앞쪽을 함께 읽지 않으면 정작 보고 싶은 호출이 창 밖으로 나간다.
3. **길이.** 기본 128바이트, 상한 512바이트. 읽지 못하면 실패를 명시한다. 빈 문자열을 남기면 "코드가 없다"와 "읽지 못했다"가 구분되지 않는다.
4. **정적 `ExitProcess` import 없는 대상 허용.** Task 185와 같은 이유로 bounded trace 목록에 추가했다.

---

## 3. 검증 결과 (Verification Results)

- `scripts/build_win32.bat`: 빌드 성공, 경고와 에러 0.
- `re2dj_unit_tests.exe`: 1,265 checks, 0 failures.
- `re2dj_windows_product_loader_probe.exe`: 전체 통과.
- 진단 실행: `20260905-015924-358`. 7개 창을 한 실행에서 모두 읽었다.

해독에는 Python `capstone` 5.0.7을 저장소 밖 임시 스크립트로 사용했다. 제품에 디스어셈블러를 들이지 않는다는 작업 지시서의 비범위를 지킨다.

### 3.1 자기 검증 (확인됨)

작업 지시서 기준대로 이미 아는 주소 `0x004225db`을 함께 지정했고, 창 안에서 Task 185가 기록한 저장이 그대로 나왔다.

```
004225a0  push ebp / mov ebp,esp / push ecx        ; thiscall 생성자
004225b1  mov dword ptr [eax], 0x4dd16c            ; vtable 설치
004225c4  mov dword ptr [edx + 0xa0c], 0
004225d1  mov dword ptr [eax + 0xa10], 0           ; Task 184·185가 지목한 저장
004225de  mov dword ptr [ecx + 0xb14], 0
```

- **확인됨 — 창 계산이 맞다.** 지정 주소 `0x004225db`가 창 중간에 오고 그 앞의 저장이 함께 들어왔다.
- **확인됨 — 기저 클래스 생성자의 vtable은 `0x004dd16c`다.**

### 3.2 객체의 정체 (확인됨)

결함 함수 `0x00422b00`의 해독이다.

```
00422b15  mov  [ebp-8], ecx                  ; this
00422b1b  add  eax, 0xa14                    ; &this->buffer
00422b22  push eax                           ; lpvData
00422b23  push 0x100                         ; cbData = 256
00422b2b  mov  edx, [ecx + 0xa10]            ; 인터페이스 포인터
00422b34  mov  ecx, [eax + 0xa10]
00422b3a  mov  eax, [ecx]                    ; vtable          <-- 결함, ecx = 0
00422b3c  push edx                           ; this
00422b3d  call [eax + 0x24]                  ; vtable slot 9
00422b4a  cmp  [ebp-4], 0x8007001e
00422b51  jne  0x422b77
```

Windows SDK `10.0.26100.0`의 `dinput.h`와 대조한 결과다.

| 관측 | SDK 확인 |
| --- | --- |
| vtable slot 9 (`[eax+0x24]`), 인자 `(DWORD, LPVOID)` | `IDirectInputDeviceA::GetDeviceState(THIS_ DWORD, LPVOID)`가 정확히 9번째 |
| `cbData = 0x100`, 버퍼가 `this+0xa14` | 256바이트 상태 배열은 DirectInput 키보드 |
| 반환값을 `0x8007001e`와 비교 | `DIERR_INPUTLOST = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ERROR_READ_FAULT)` = `0x8007001e` |
| 정리 함수가 `[eax+0x20]` 뒤 `[eax+8]` 호출 | slot 8 = `Unacquire`, slot 2 = `Release` |

- **확인됨 — `+0xa10`은 `IDirectInputDevice`다.** 슬롯 번호, 인자 형태, 버퍼 크기, 오류 상수가 모두 일치한다.
- **확인됨 — 게임은 lost 상태를 처리하도록 쓰였다.** `DIERR_INPUTLOST`면 다시 `+0xa10`을 읽어 다른 슬롯을 부른다. 정상 코드이지 방치된 코드가 아니다.
- **확인됨 — 같은 클래스가 DirectInput 객체 셋을 들고 있다.** `0x004229f2`의 정리 함수가 `+0xb14`에 `Unacquire`+`Release`를, `+0xa10`과 `+0xa0c`에 `Release`를 부른 뒤 각각 0으로 만든다. `+0xa0c`는 `IDirectInput` 루트, `+0xb14`는 두 번째 장치로 보인다.

### 3.3 호출 사슬 (확인됨)

```
0x004076ef  →  0x0043627e  →  0x004235d3  →  thunk 0x00401ee7  →  0x00422b00 (결함)
```

- **확인됨 — 결함 함수는 세 연속 호출 중 첫 번째다.** `0x004235d3`이 같은 객체에 `0x401ee7`, `0x402671`, `0x402e2d`를 차례로 부른 뒤 stride `0x14` 배열을 순회한다. 매 프레임 입력을 읽고 처리하는 갱신 함수의 모양이다.
- **확인됨 — 전역 객체의 vtable은 `0x004e20a0`이다.** 생성자 호출자 `0x004a5c10`이 기저 생성자를 부른 뒤 `mov [eax], 0x4e20a0`으로 파생 vtable을 덮어쓴다. Task 182의 접근 위반 기록이 `0x00aca5b0`에서 읽은 값과 같다.

### 3.4 `dinput.dll`이 적재되지 않는다 (확인됨)

- **확인됨 — 실행 전체에서 `dinput`이라는 이름이 한 번도 나타나지 않는다.** `load_dll` 기록에도, 동적 해석 이름 목록에도 없다.
- **확인됨 — 따라서 생성 경로가 실행되지 않는다는 Task 185의 결론과 일치한다.** 게임은 DirectInput을 만들려는 시도조차 하지 않는다.

### 3.5 형제 필드도 0만 기록된다 (확인됨)

`+0xa0c`와 `+0xb14`를 같은 방식으로 스캔했다.

| 상수 | 일치 | 레지스터 저장 | 판정 |
| - | - | - | - |
| `0xa0c` | 30 | 1건 (`0x00064dd5`) | **다른 클래스의 필드였다** |
| `0xb14` | 19 | 0건 | 0만 기록 |

`0x00464dd5`의 코드를 읽은 결과 그 함수의 `this`는 `+0x478`, `+0x5c4`, `+0x7e8`을 다루고, `+0xa0c`에 넣는 값은 전역 관리자 `[0x00ac2910]`에 문자열 `0x004f86fc`를 넘겨 받은 객체다. 같은 오프셋을 가진 **별개의 클래스**이며 DirectInput과 무관하다.

- **확인됨 — 결함 클래스의 세 DirectInput 필드는 모두 0만 기록된다.**
- **확인됨 — 상수 검색만으로 기록자를 판정하면 안 된다.** 오프셋은 클래스마다 재사용되므로, 후보를 찾은 뒤 반드시 코드를 읽어 소속 클래스를 확인해야 한다. 이번에 그 확인을 하지 않았다면 조사가 엉뚱한 자원 관리자 함수로 갔을 것이다.
- **확인됨 — `+0xa10`의 주소를 취하는 `lea`도 없다.** `IDirectInput::CreateDevice`에 필드 주소를 out-parameter로 넘기는 통상적인 생성 형태가 `.text`에 존재하지 않는다.

### 3.6 판정하지 않은 것 (미확정)

- **미확정 — 왜 DirectInput 초기화가 실행되지 않는가.** 초기화 함수가 아예 호출되지 않는지, 호출되었으나 그 앞에서 빠져나오는지 구분하지 못했다.
- **미확정 — `+0xb14`의 장치 종류.** `Unacquire`를 받으므로 장치인 것은 확실하나 어떤 장치인지는 확정하지 않았다.
- **미확정 — 4th가 DirectInput과 legacy I/O 포트를 각각 무엇에 쓰는가.** 같은 실행에서 포트 `0x103`–`0x105`도 폴링한다.

---

## 4. 이 결과가 뜻하는 것 (What This Means)

Task 182가 그래픽 경로를 실제 렌더링까지 열었고, Task 184와 185가 결함 필드를 좁혔으며, 이번에 그 필드가 무엇인지 확정되었다. **검정화면의 직접 원인은 그래픽이 아니라 입력이다.** 게임은 첫 프레임을 그리기 전에 키보드를 폴링하고, 그 장치가 없어 죽는다.

re2DJ에는 DirectDraw, Direct3D, DirectSound 경계는 있으나 **DirectInput 경계가 없다.** 1st SE는 legacy I/O 포트로만 입력을 받으므로 이 경계가 필요하지 않았다.

*The direct cause of the black screen is input, not graphics: the guest polls a keyboard before drawing its first frame and dies because the device does not exist. re2DJ has DirectDraw, Direct3D, and DirectSound boundaries but no DirectInput boundary, which 1st SE never needed because it takes input through legacy I/O ports alone.*

---

## 5. 다음 작업 (Next Task)

세 필드 모두 `.text`에 생성 코드가 없다는 것이 3.5에서 확인되었으므로, "생성 함수를 찾는다"는 방향은 닫혔다. 남은 방향은 두 가지다.

1. **객체가 잘못 선택되었을 가능성을 본다.** 전역 `0x00aca5b0`은 파생 클래스(vtable `0x004e20a0`)이고, 결함 메서드는 기저 클래스의 것이다. 게임이 입력 방식마다 다른 파생 클래스를 두고 있고 지금 잘못된 쪽이 만들어졌다면, 생성 코드가 없는 것이 아니라 **다른 클래스에 있는 것**이다. 파생 vtable `0x004e20a0`의 슬롯들을 읽어 이 클래스가 무엇인지 먼저 확정한다.
2. **DirectInput 경계를 제공한다.** 위 확인과 무관하게 re2DJ에는 DirectInput 경계가 없다. `ez2dj_keyboard_input`이 이미 키 상태를 제공하므로, `DirectInputCreateA`와 최소 장치 인터페이스를 HLE로 세우면 게스트가 요구하는 형태를 만족시킬 수 있다. 다만 게스트가 그 생성을 **시도조차 하지 않는** 현재 상태에서는 경계만으로 해결되지 않으므로 1번이 먼저다.

---

## 6. 관련 문서 (Related Documents)

- [Task 186 설계](../design/20260905-186-guest-code-window.md)
- [Task 186 작업 지시서](../work-orders/20260905-186-guest-code-window.md)
- [Task 185 작업 로그](20260905-185-field-write-watch.md)
- [Task 184 작업 로그](20260905-184-guest-field-reference-scan.md)
- [4th 그래픽 경로 분석](../analysis/ez2dj4th-graphics-path.md)
