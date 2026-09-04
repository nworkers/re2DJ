# ez2dj4th 그래픽 경로 분석
# EZ2DJ 4th Graphics Path Analysis

EZ2DJ 4th Trax가 DirectX 7 경계에서 무엇을 요구하는지, 실제 실행에서 확인한 사실을 모은다. 일반 DirectX 배경은 [`docs/kb/`](../kb/README.md)에, 시간순 증거는 [`docs/work-logs/`](../work-logs/)에 둔다.

*What EZ2DJ 4th Trax asks of the DirectX 7 boundary, as verified in real runs.*

---

## 1. 인터페이스 선택 (확인됨) (Interface Selection — Confirmed)

- **확인됨 — 진입점은 `DirectDrawCreateEx`다.** 게스트는 `GetProcAddress`로 해석하며 `IID_IDirectDraw7`(`15e65ec0-3b9c-11d2-b92f-00609797ea5b`)을 요청한다.
- **확인됨 — Direct3D는 `QueryInterface(IID_IDirect3D7)`로 얻는다.** `f5049e77-4861-11d2-a407-00a0c90629a8`이다.
- **확인됨 — 드라이버를 네 번 열거한다.** null GUID로 한 번, `{67685559-…}`와 `{6768555a-…}`로 각각 한 번, 그리고 다시 null GUID로 한 번이다. 마지막 null GUID 인스턴스가 실제 렌더링에 쓰인다.
- **확인됨 — 게스트는 표면과 디바이스에 `QueryInterface`를 부르지 않는다.** 두 경계에 기록을 달아 세 번의 실행에서 한 줄도 관측되지 않았다. 텍스처를 표면 자체로 다루는 DirectX 7 방식과 일치한다.

## 2. 표면 요구 사항 (확인됨) (Surface Requirements — Confirmed)

| 표면 | `dwFlags` | `ddsCaps` | 크기 | 픽셀 형식 |
| - | - | - | - | - |
| primary | `0x00000021` (CAPS·BACKBUFFERCOUNT) | `0x00002218` (3DDEVICE·PRIMARYSURFACE·FLIP·COMPLEX) | 모드 크기 | 지정 없음 |
| depth | `0x00001007` (CAPS·HEIGHT·WIDTH·PIXELFORMAT) | `0x00024000` (ZBUFFER·VIDEOMEMORY) | 640×480 | `DDPF_ZBUFFER`, 16비트 |
| texture | `0x00001007` | `0x10005000` (ALLOCONLOAD·VIDEOMEMORY·TEXTURE) | 가변, 최대 1024×512 관측 | **RGB565** |

- **확인됨 — 텍스처 픽셀 형식은 RGB565다.** `dwRBitMask=0xf800`, `dwGBitMask=0x07e0`, `dwBBitMask=0x001f`, alpha mask 0이다. 1st SE와 같은 배치이므로 공용 표면 backing이 그대로 받는다.
- **확인됨 — primary는 back buffer 하나를 요구한다.** `dwBackBufferCount=1`이다.
- **확인됨 — 게스트는 primary 자체를 3D 렌더 타깃으로 요구한다.** primary의 caps에 `DDSCAPS_3DDEVICE`가 있다.
- **확인됨 — 깊이 버퍼는 렌더 타깃에 `AddAttachedSurface`로 붙인다.** 붙인 뒤 되읽는지는 관측하지 않았지만, 되읽기가 실패하던 동안 게스트가 진행하지 못한 구간이 있었다.
- **확인됨 — 텍스처 업로드는 `GetDC` / `ReleaseDC`다.** `Lock`은 관측되지 않았다. 1st SE와 같은 경로다.

## 3. 협조 수준과 표시 모드 (확인됨) (Cooperative Level And Display Mode — Confirmed)

- **확인됨 — `SetCooperativeLevel` 플래그는 `0x00000813`이다.** `DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT | DDSCL_EXCLUSIVE | DDSCL_FPUSETUP`이며, 전체 화면 독점을 요구한다.
- **확인됨 — `SetDisplayMode`는 640×480×16이다.**
- **확인됨 — 게스트 창은 `WS_POPUP`으로 만들어지고 스스로 표시된다.** 창 모드 정책이 적용되기 전 관측된 style은 `0x04cf0000`, 적용 후는 `0x14cf0000`이다.

## 4. 디바이스 요구 사항 (확인됨) (Device Requirements — Confirmed)

- **확인됨 — 드라이버가 `DDCAPS2_CANRENDERWINDOWED`를 게시해야 디바이스를 유지한다.** 이 비트가 없으면 게스트의 드라이버 단계가 디바이스를 버린다.
- **확인됨 — 게스트는 열거된 세 디바이스 가운데 하나를 고른다.** RGB emulation, HAL, T&L HAL을 게시한다.
- **확인됨 — 초기 렌더 상태 10건을 설정한다.** 상태 번호 139, 4, 41, 26, 2, 27, 29, 7, 137, 9 순서다.
- **확인됨 — `IDirect3DDevice7::SetMaterial`을 부른다.** 현재 구현되지 않은 유일한 슬롯이다.
- **확인됨 — 정점 버퍼는 `D3DFVF_VERTEX`다.** FVF `0x112`(XYZ·NORMAL·TEX1), stride 32바이트, 121개 정점, 3,872바이트다. 1st SE 경로가 이미 지원하는 형식과 같다.

## 5. 현재 차단 지점 (확인됨) (Current Blocker — Confirmed)

- **확인됨 — 첫 텍스처 업로드 직후 게스트가 null 객체를 역참조한다.** `RVA 0x00022b3a`에서 `mov eax,[ecx]`가 `ecx=0`으로 실행된다. `ecx`는 전역 객체 `0x00aca5b0`의 `+0xa10` 필드에서 읽은 값이다.
- **확인됨 — 세 번의 실행에서 레지스터까지 동일하게 재현된다.**
- **확인됨 — 이 호출은 COM이 아니다.** `this`를 스택에 밀지 않고 인자 하나만 올린 뒤 vtable 슬롯 9(`[eax+0x24]`)를 부르는 thiscall이므로, 대상은 HLE facade 객체가 아니라 게스트 자신의 C++ 객체다.
- **추정 — 이 분기는 `GetDC`가 성공하면서 새로 진입한 경로다.** 표면 스텁이 null 핸들을 돌려주던 동안에는 게스트가 다른 분기를 탔다. 그 분기가 요구하는 객체를 무엇이 만드는지는 확인하지 않았다.

### 5.1 `+0xa10` 필드를 다루는 코드 (Task 184에서 확인)

게임 `.text` 전체(`0x00001000`부터 `0x000db022`바이트, PE 섹션 표와 일치)를 상수 `0xa10`으로 검색한 결과다.

| RVA | 해독 | 역할 |
| - | - | - |
| `0x000225d1` | `mov dword [eax+0xa10], 0` | 생성자에서 0으로 초기화 |
| `0x00022a5f` | `cmp dword [edx+0xa10], 0` + `jz +0x4f` | null 검사, 정리 경로를 감쌈 |
| `0x00022aad` | `mov dword [eax+0xa10], 0` | `call 0x000c1ba0` 뒤 정리 |
| `0x00022b34` | `mov ecx,[eax+0xa10]` | **검사 없는 읽기, 결함 지점** |

- **확인됨 — 0이 아닌 값을 이 필드에 넣는 명령이 게임 `.text`에 없다.** 쓰기는 위 두 건뿐이고 둘 다 즉값 0이다.
- **확인됨 — 절대 주소 `0x00acafc0`으로 접근하는 명령도 없다.** 필드는 베이스 상대로만 주소지정된다.
- **확인됨 — 같은 필드를 한쪽은 검사하고 한쪽은 검사하지 않는다.** `0x00022a5f`는 검사하고 `0x00022b34`는 하지 않는다.
- **확인됨 — 실행 가능한 다른 섹션은 `.protect`(`0x006e0000`, `0x00039569`) 하나이며 이번 검색에 포함하지 않았다.**
- **확인됨 — 실행 한 번 동안 이 주소에 일어나는 쓰기는 생성자의 0 저장 하나뿐이다.** Task 185가 `0x00acafc0`에 하드웨어 쓰기 감시점을 걸어 실행 전체를 관측했고 적중은 1회, 값은 0이었다. 감시점은 명령의 부호화 방식이나 실행 섹션과 무관하게 트랩하므로, `.protect`에서의 설치도 `memcpy`나 계산된 주소를 통한 설치도 다른 변위로 같은 주소를 쓰는 설치도 모두 배제된다. **포인터를 설치하는 경로가 이 실행에서 실행되지 않는다.**
- **확인됨 — 정리 경로는 실행되지 않는다.** `0x00022aad`의 두 번째 쓰기가 한 번도 적중하지 않았다. 객체가 만들어졌다 해제된 뒤 쓰인 것이 아니라 처음부터 채워지지 않았다.
- **확인됨 — 생성과 결함이 같은 스레드에서 일어난다.** 둘 다 스레드 `44332`다.
- **확인됨 — 생성자의 호출자는 RVA `0x000a5c26`이다.** 감시점 적중의 복귀 주소로 얻었다.
- **확인됨 — `.text`는 디스크에서 암호문이다.** 파일 오프셋 `0x225d1`이 `c5 ce d8 df …`인데 실행 중 같은 위치는 `c7 80 10 0a 00 00 …`이다. 패커가 실행 중 복호화하므로 정적 파일 분석으로는 이 코드를 읽을 수 없다.
- **미확정 — 이 객체를 만들었어야 하는 코드와 그것이 실행되지 않는 이유.** 다음 조사의 대상이다.

### 5.2 결함 객체의 정체 (Task 186에서 확인)

결함 함수 `0x00422b00`을 실행 중 메모리에서 읽어 해독했다.

```
add  eax, 0xa14          ; &this->buffer
push eax                 ; lpvData
push 0x100               ; cbData = 256
mov  ecx, [eax + 0xa10]  ; 인터페이스
mov  eax, [ecx]          ; vtable        <-- 결함
push edx
call [eax + 0x24]        ; vtable slot 9
cmp  [ebp-4], 0x8007001e
```

Windows SDK `10.0.26100.0`의 `dinput.h`와 대조한 결과다.

| 관측 | SDK 확인 |
| --- | --- |
| vtable slot 9, 인자 `(DWORD, LPVOID)` | `IDirectInputDeviceA::GetDeviceState`가 정확히 9번째 |
| `cbData = 0x100`, 버퍼 `this+0xa14` | 256바이트 상태 배열은 DirectInput 키보드 |
| 반환값 비교 상수 `0x8007001e` | `DIERR_INPUTLOST` = `MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ERROR_READ_FAULT)` |
| 정리 함수의 `[eax+0x20]` 뒤 `[eax+8]` | slot 8 = `Unacquire`, slot 2 = `Release` |

- **확인됨 — `+0xa10`은 `IDirectInputDevice`다.** 슬롯 번호, 인자 형태, 버퍼 크기, 오류 상수가 모두 일치한다.
- **확인됨 — 같은 클래스가 DirectInput 객체 셋을 든다.** 정리 함수 `0x004229f2`가 `+0xb14`에 `Unacquire`+`Release`를, `+0xa10`과 `+0xa0c`에 `Release`를 부른다. `+0xa0c`는 `IDirectInput` 루트로 보인다.
- **확인됨 — 호출 사슬은 `0x004076ef` → `0x0043627e` → `0x004235d3` → thunk `0x00401ee7` → `0x00422b00`이다.** `0x004235d3`이 같은 객체에 세 메서드를 연속 호출하는 프레임 갱신 함수다.
- **확인됨 — 전역 객체의 vtable은 `0x004e20a0`이다.** 생성자 호출자 `0x004a5c10`이 기저 생성자 `0x004225a0`(vtable `0x004dd16c`)을 부른 뒤 파생 vtable로 덮어쓴다.
- **확인됨 — 실행 전체에서 `dinput.dll`이 적재되지 않는다.** `load_dll` 기록에도 동적 해석 이름 목록에도 없다.
- **확인됨 — 결함 클래스의 세 필드는 모두 0만 기록된다.** `0xa0c`, `0xa10`, `0xb14`를 각각 스캔했다. `0xa0c` 스캔이 찾은 유일한 레지스터 저장 `0x00464dd5`는 코드를 읽어보니 **다른 클래스**의 필드였다. 그 함수의 `this`는 `+0x478`, `+0x5c4`, `+0x7e8`을 쓰고, `+0xa0c`에 넣는 값은 전역 관리자 `[0x00ac2910]`에 문자열 `0x004f86fc`를 넘겨 받은 객체다. 같은 오프셋을 가진 별개의 클래스이며 DirectInput과 무관하다. 상수 하나로 검색하면 모든 클래스의 같은 오프셋이 걸리므로, 기록자를 찾을 때는 코드를 읽어 소속 클래스를 확인해야 한다.
- **확인됨 — `+0xa10`에는 `lea`도 없다.** 필드 주소를 out-parameter로 넘기는 `lea reg,[this+0xa10]` 형태도 `.text`에 없다. `IDirectInput::CreateDevice`에 필드 주소를 넘기는 통상적인 생성 형태가 이 실행 파일에 존재하지 않는다.

### 5.3 DirectInput 초기화 루틴과 실패 원인 (Task 187에서 확인)

Task 187에서 전역 인스턴스 `0x00aca5b0`과 vtable 계통, 그리고 초기화 경로를 전수 분석하여 결함의 전모를 확정했다.

- **확인됨 — 객체 선택 오류가 아니다.** `0x00aca5b0`은 CRT 시작 루틴(`0x004a29b6`)에서 정적으로 생성되는 단일 전역 객체다. 엔진 초기화 `0x004a2b36`에서 `mov [0x00ac215c], 0x00aca5b0`으로 전역 포인터에 무조건 대입된다.
- **확인됨 — 기저 클래스(`0x004dd16c`)는 3개 순수 가상 함수를 가진 추상 인터페이스다.** 파생 클래스(`0x004e20a0`)는 이 3개 메서드(`0x00401c30`, `0x00401a14`, `0x004025d1`)를 구현한 구체 클래스다. 결함 함수 `0x00422b00`은 가상 함수가 아니라 일반 멤버 함수다.
- **확인됨 — DirectInput 생성 루틴(`0x004227d0`)이 온전히 존재하며 실제로 호출되었다.** Task 184가 찾지 못했던 이유는 `lea eax,[this+0xa10]` 대신 `mov eax,[this]; add eax,0xa10; push eax` 형태로 포인터를 넘겼기 때문이다.
  1. `DirectInputCreateA(hInstance, 0x700, &this->+0xa0c, NULL)`
  2. `IDirectInputA::CreateDevice(GUID_SysMouse [0x004e4100], &this->+0xb14, NULL)`
  3. `IDirectInputDeviceA::SetDataFormat(&c_dfDIMouse)`
  4. `IDirectInputDeviceA::SetCooperativeLevel(hWnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND)`
  5. `IDirectInputA::CreateDevice(GUID_SysKeyboard [0x004e40f0], &this->+0xa10, NULL)`
  6. `IDirectInputDeviceA::SetDataFormat(&c_dfDIKeyboard)`
  7. `IDirectInputDeviceA::SetCooperativeLevel(hWnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND)`
  8. `keyboard->Acquire()`, `mouse->Acquire()`
- **확인됨 — 현대 Windows(Windows 11) 호스트의 `dinput.dll`이 `DirectInputCreateA(0x700)`을 거부한다.** `DirectInputCreateA`는 `0x80070057` (`E_INVALIDARG`)을 반환한다.
- **확인됨 — 게스트는 오류 발생 시 `"DirectInput Initialize Error"`(`0x004ee59c`)를 기록하고 객체 생성 없이 리턴한다.** 모든 장치 포인터(`+0xa0c`, `+0xb14`, `+0xa10`)가 null인 채로 유지된다.
- **확인됨 — 이후 프레임 갱신 루프(`0x004235d3`)가 null 검사 없이 `+0xa10`을 역참조하여 접근 위반(`0xc0000005`)이 발생했다.**

*Confirmed by Task 187: The object was not wrongly selected; `0x00aca5b0` is a static singleton initialized at CRT startup. The DirectInput creation routine `0x004227d0` exists in the binary and was called, passing arguments via `add eax, 0xa10; push eax`. However, Windows 11's legacy `dinput.dll` rejects `DirectInputCreateA(0x700)` with `0x80070057` (`E_INVALIDARG`), causing the guest to bail out with `"DirectInput Initialize Error"`. Because the device fields remain null and `0x004235d3` polls `+0xa10` unchecked, an access violation occurs. The definitive fix is implementing a DirectInput 7 HLE boundary.*

### 5.4 DirectInput 7 HLE 구현 및 접근 위반 해결 (Task 188에서 확인)

- **확인됨 — DirectInput 7 HLE facade 도입으로 초기화 실패 및 크래시가 완벽히 해결되었다.**
  - `DirectInputCreateA`가 가로채어져 유효한 `IDirectInputA` 및 `IDirectInputDeviceA`(`GUID_SysKeyboard`, `GUID_SysMouse`) 객체를 반환한다.
  - 마우스 및 키보드의 `SetDataFormat`, `SetCooperativeLevel`(0x06 = `DISCL_NONEXCLUSIVE | DISCL_FOREGROUND`), `Acquire`, `GetDeviceState`가 정상 처리된다.
  - 게스트의 `0x00422b3a` 접근 위반(`0xc0000005`)이 완전히 해소되었다.
- **확인됨 — 메인 렌더링 루프 진입 및 연속 60 FPS 렌더링 확인.**
  - 첫 프레임 이후 10초 동안 총 602 프레임이 중단 없이 정상 렌더링되었다.
  - `IDirect3DDevice7::Clear` (검정색 `0x00000000`, depth `1.0`), `SetTexture`, `DrawPrimitive` 정점 그리기 연산이 연속으로 수행되었다.

*Confirmed by Task 188: Implementing the DirectInput 7 HLE facade completely eliminated the initialization failure and null pointer dereference. Keyboard and mouse devices were successfully created and acquired, and the game proceeded to run smoothly in its main render loop, rendering 602 frames in 10 seconds (steady 60 FPS).*

---

## 6. 아직 확인하지 못한 것 (미확정) (Unresolved)

- **미확정 — 오디오 시스템과의 상호작용.** DirectSound 등 사운드 초기화 및 배경음/효과음 재생 연동 상태.
- **미확정 — 인게임 조작 및 실제 게임플레이 흐름.** 타이틀/모드 선택 화면 이후 곡 선택 및 노트 연주 루프의 동작.

---

## 관련 문서 (Related Documents)

- [Task 182 설계](../design/20260905-182-directx7-legacy-delegation.md)
- [Task 182 작업 로그](../work-logs/20260905-182-directx7-legacy-delegation.md)
- [Task 186 작업 로그](../work-logs/20260905-186-guest-code-window.md)
- [Task 187 작업 로그](../work-logs/20260905-187-derived-input-vtable-analysis.md)
- [Task 188 작업 로그](../work-logs/20260905-188-directinput7-hle-facade.md)
- [4th Hardlock 런타임 분석](ez2dj4th-hardlock-runtime.md)
- [4th CHD 파일 시스템 분석](ez2dj4th-chd-filesystem.md)

