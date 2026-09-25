# 작업 373 작업 로그 — DirectDraw 진입 / Task 373 work log — DirectDraw entry

설계: [20260926-373-directdraw-entry.md](../design/20260926-373-directdraw-entry.md)
작업 지시서: [20260926-373-directdraw-entry.md](../work-orders/20260926-373-directdraw-entry.md)

## 진행 / Progress

`DirectDrawEnumerateExA`를 측정해 구현했다. 그러자 게스트가 첫 callback(주 표시 장치) 안에서 `DirectDrawCreateEx(NULL, &dd, IID_IDirectDraw7, NULL)`을 불렀다. COM 객체의 vtable을 facade export thunk로 채우는 방식으로 `IDirectDraw7`을 만들었다. 다음 호출은 `IDirectDraw7::QueryInterface`였고, API log에 요청 IID `{F5049E77-4861-11D2-A407-00A0C90629A8}`(`IID_IDirect3D7`)가 찍혔다. Windows 경로 분석 §1과 같은 순서다.

*`DirectDrawEnumerateExA` was measured and implemented; inside its first callback (the primary display) the guest called `DirectDrawCreateEx(NULL, &dd, IID_IDirectDraw7, NULL)`. `IDirectDraw7` was built with its vtable filled from facade export thunks. The next call was `IDirectDraw7::QueryInterface`, and the API log shows the requested IID `{F5049E77-4861-11D2-A407-00A0C90629A8}` (`IID_IDirect3D7`), the order the Windows path's analysis §1 records.*

측정 script와 C++ 조각에 작은따옴표가 있어 shell heredoc이 두 번 깨졌다. 그 뒤로 조각은 모두 파일로 쓴 다음 넣었다.

*Apostrophes in a measurement script and a C++ snippet broke the shell heredoc twice; snippets were written as files from then on.*

## 변경 / Changes

- **`guest_com.h/.cpp`**(새 파일): `GuestComObjects`. `GuestProcess::com()`이 이것을 준다.
  ***`guest_com.h/.cpp`** (new): `GuestComObjects`, through `GuestProcess::com()`.*
- **`ddraw_module.h/.cpp`**(새 파일): `DirectDrawEnumerateExA`, `DirectDrawCreateEx`, `IDirectDraw7::*` 30개(`QueryInterface`, `AddRef`, `Release` 구현).
  ***`ddraw_module.h/.cpp`** (new): `DirectDrawEnumerateExA`, `DirectDrawCreateEx`, and the 30 `IDirectDraw7::*` exports (`QueryInterface`, `AddRef`, and `Release` implemented).*
- **`GuestUser::PrimaryMonitor()`**: 게스트가 보는 유일한 HMONITOR다.
  ***`GuestUser::PrimaryMonitor()`**: the guest's one HMONITOR.*
- **Linux**: ddraw module을 등록했고, 해석 전용 목록에서 ddraw를 뺐다.
  ***Linux:** the ddraw module is registered and ddraw leaves the resolve-only list.*
- **단위 테스트**(`ddraw_module_test.cpp`): `MemoryServices`에 두 번째 module과 그 export 표를 추가했다. 다음을 검사한다.
  ***Unit tests** (`ddraw_module_test.cpp`), with a second module and its export table added to `MemoryServices`:*
  - 열거의 callback 인자(주 드라이버의 CP949 설명, `DISPLAY1` GUID, 문자열, HMONITOR), 호출 뒤 heap block이 남지 않음 / *the enumeration's callback arguments (the primary's CP949 description, the `DISPLAY1` GUID, the strings, the HMONITOR), with no heap block left after the call;*
  - flags 0, FALSE로 멈춤, 잘못된 인자 / *flags 0, a FALSE stop, and invalid parameters;*
  - `IID_IDirectDraw4` 거부, vtable의 slot 순서 / *`IID_IDirectDraw4` refused, and the vtable's slot order;*
  - `QueryInterface` 자기 자신과 Direct3D 정지, 참조 수와 해제 / *`QueryInterface` returning the object itself and stopping on Direct3D, and reference counts with the free;*
  - 모델 밖 메서드가 이름을 대고 멈춤 / *a method outside the model stopping with its name.*

  해석 전용 module 테스트는 4개 DLL, 16개로 바꿨다.
  *The resolve-only module test now expects four DLLs and 16 exports.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 / no warnings or errors, 3/3 each |
| Linux helper, probe, 기존 진단 네 개 / diagnostics | 이전과 같음 / as before |
| Windows x86 build, CTest | 오류·경고 없음, 6/6 / no errors or warnings, 6/6 |
| 실제 4th, 두 폭 / real 4th, both widths | 호출 1,731번, 주소를 정규화하면 같음. 열거 callback 안의 `#1730 DirectDrawCreateEx`가 `DD_OK`를 주고, `#1731 IDirectDraw7::QueryInterface(IID_IDirect3D7)`에서 정지. Hardlock 요청 수는 이전과 같음 / 1,731 calls, identical after address normalization; `#1730 DirectDrawCreateEx` inside the enumeration callback returns `DD_OK`, then the run stops at `#1731 IDirectDraw7::QueryInterface(IID_IDirect3D7)`, with the Hardlock request totals unchanged |

## 다음 / Next

Direct3D 7이다. Windows 경로의 DirectX facade를 공용 core로 옮길지, Linux에서 따로 구현할지 먼저 정한다.

*Direct3D 7, after deciding whether the Windows path's DirectX facade moves into a shared core or Linux implements its own.*
