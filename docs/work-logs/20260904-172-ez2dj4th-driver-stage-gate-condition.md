# 20260904-172 EZ2DJ 4th 드라이버 단계 게이트 조건 관측 결과
# 20260904-172 EZ2DJ 4th Driver Stage Gate Condition Observation Results

## 1. 개요 (Overview)

Task 171이 남긴 미확정 항목 — 컨텍스트의 `+0x4c8`을 무엇이 결정하는가 — 에 답했다.

**결론: 드라이버 단계(`RVA 0x0000f880`)는 `IDirectDraw7::GetCaps`가 채운 driver `DDCAPS`의 `dwCaps2`에서 `DDCAPS2_CANRENDERWINDOWED`(`0x00080000`) 비트를 검사하고, 동시에 드라이버 GUID 포인터가 NULL일 때만 게이트를 1로 쓴다. re2DJ의 `Dd7GetCaps`는 `dwCaps`만 채우고 `dwCaps2`를 0으로 남기므로 이 조건은 어떤 드라이버에서도 성립하지 않는다. 이것이 guard 1이 `0x81000004`를 반환하는 근본 원인이다.**

This task answered what decides the context's `+0x4c8`. The driver stage at `RVA 0x0000f880` sets the gate to 1 only when the driver `DDCAPS` filled by `IDirectDraw7::GetCaps` has `DDCAPS2_CANRENDERWINDOWED` (`0x00080000`) in `dwCaps2` **and** the driver GUID pointer is NULL. re2DJ's `Dd7GetCaps` fills `dwCaps` but leaves `dwCaps2` zero, so the condition never holds for any driver. That is the root cause of guard 1 returning `0x81000004`.

---

## 2. 변경 내용 (Changes Implemented)

`src/tools/windows_x86_launcher_probe/main.cpp`만 변경했다. 새 CLI 옵션은 없다.

1. **코드 범위 덤프.** 참조 스캔이 이미 읽어 둔 `.text` 버퍼에서 지정 RVA 범위를 64바이트 단위로 잘라 `device_enum_code_range`와 `device_enum_code_chunk` 이벤트로 기록한다. 이번 대상은 `driver_stage`(`0x0000f700`, `0x680` 바이트)다.
2. **앵커 추가.** `driver_stage_context_zero`(`0x0000f93e`)와 `device_enum_callback`(`0x0000fc57`).

---

## 3. 검증 결과 (Verification Results)

- `scripts/build_win32.bat`: 빌드 성공, 경고와 에러 0.
- `re2dj_unit_tests.exe`: 1,253 checks, 0 failures.
- `re2dj_windows_product_loader_probe.exe`: 전체 통과.
- 진단 실행: `logs/windows_x86_launcher_probe/ez2dj4th/20260904-140612-217.jsonl`.

### 3.1 드라이버 콜백의 구조 (확인됨)

`RVA 0x0000f880`에서 시작하는 함수가 드라이버 한 개를 처리한다. 인자는 `[ebp+0x08]` 드라이버 GUID 포인터, `[ebp+0x0c]` 설명 문자열이다. 지역 `[ebp-0x4d4]`가 `0x4d0` 바이트 컨텍스트이고, 게이트는 `[ebp-0x0c]`이다(`-0x4d4 + 0x4c8 = -0x0c`).

```
0x0000f883  sub  esp, 0x4e0
0x0000f8a4  lea  eax, [ebp-0x04]              ; &IDirectDraw7*
0x0000f8ac  call DirectDrawCreateEx 경로
0x0000f8e0  push 0x004e4e10                   ; IID_IDirect3D7
0x0000f8ee  call [ecx]                        ; QueryInterface
0x0000f93a  push 0x4d0 / push 0 / lea eax,[ebp-0x4d4] / call memset
0x0000f95f  call [0x00ad1724]                 ; lstrcpynA(context, 설명, 0x27)
0x0000f96c  mov  dword [ebp-0x3b4], 0x17c     ; driver DDCAPS.dwSize
0x0000f976  mov  dword [ebp-0x238], 0x17c     ; HEL DDCAPS.dwSize
0x0000f999  call [eax+0x2c]                   ; IDirectDraw7::GetCaps
0x0000f9a4  cmp  dword [ebp+0x08], 0          ; 드라이버 GUID 포인터
0x0000f9a8  je   0x0000f9cd                   ; NULL이면 GUID 복사를 건너뜀
0x0000f9aa  ...  GUID 16바이트를 [ebp-0x28]에 복사
0x0000f9c7  mov  [ebp-0x3b8], edx             ; context+0x11c = &복사본
0x0000f9cd  mov  eax, [ebp-0x3ac]             ; driver DDCAPS.dwCaps2
0x0000f9d3  and  eax, 0x00080000              ; DDCAPS2_CANRENDERWINDOWED
0x0000f9d8  test eax, eax
0x0000f9da  je   0x0000f9ec
0x0000f9dc  cmp  dword [ebp-0x3b8], 0         ; 드라이버 GUID 포인터가 NULL인가
0x0000f9e3  jne  0x0000f9ec
0x0000f9e5  mov  dword [ebp-0x0c], 1          ; ***** 게이트 = 1 *****
0x0000f9ee  push 0x0040fb5e                   ; 모드 열거 콜백
0x0000fa07  call [eax+0x20]                   ; IDirectDraw7::EnumDisplayModes
0x0000fa3c  push 0x0040fc57                   ; 장치 열거 콜백
0x0000fa48  call [eax+0x0c]                   ; IDirect3D7::EnumDevices
```

- **확인됨 — 게이트는 `[ebp-0x0c]`에 쓰인다.** 컨텍스트 base `[ebp-0x4d4]`에서 `+0x4c8`이며, Task 171이 확정한 복사 경로의 원본이다.
- **확인됨 — 조건은 두 개의 AND다.** `dwCaps2 & 0x00080000`이 참이고 컨텍스트의 `+0x11c`(드라이버 GUID 포인터)가 NULL이어야 한다.
- **확인됨 — `[ebp-0x3b4]`는 `GetCaps`가 채운 driver `DDCAPS`다.** `dwSize`에 `0x17c`를 넣고 `GetCaps`의 첫 인자로 넘긴 바로 그 버퍼이며, `[ebp-0x3ac]`는 그 `+0x8`, 즉 `dwCaps2`다.
- **확인됨 — 컨텍스트의 `+0x11c`는 드라이버 GUID 포인터다.** `[ebp+0x08]`이 NULL이 아닐 때만 채워지고, 그 값은 지역 GUID 사본을 가리킨다.
- **확인됨 — 이 컨텍스트가 그대로 두 열거 콜백에 전달된다.** 모드 콜백은 `0x0040fb5e`, 장치 콜백은 `0x0040fc57`이며 후자는 Task 171이 확인한 값과 같다.
- **확인됨 — `+0x118`은 `bHardware`다.** 장치 콜백의 `0x0000fcb0`이 `mov edx, [ecx]; and edx, 0x00080000; mov [eax+0x118], edx`를 수행한다. 여기서 `[ecx]`는 `D3DDEVICEDESC7.dwDevCaps`이고 `0x00080000`은 `D3DDEVCAPS_HWRASTERIZATION`이다. Task 171의 판정과 같다.

### 3.2 re2DJ의 `GetCaps`가 조건을 만족시키지 못한다 (확인됨)

`Dd7GetCaps`는 `dwSize`, `dwCaps`, `ddsCaps.dwCaps`만 채우고 `dwCaps2`는 `memset`으로 0인 채 남긴다. 레코드 창 관측과도 맞는다. 레코드 `+0x120`의 `DDCAPS` 블록에서 `+0x124`(`dwCaps`)는 `0x00400041`로 나타나지만 `+0x128`(`dwCaps2`)은 0이라 0이 아닌 dword 목록에 없다.

따라서 `0x0000f9da`의 `je`가 항상 성립해 게이트는 어떤 드라이버에서도 0으로 남는다.

### 3.3 드라이버 열거는 아직 HLE 경계 밖이다 (확인됨)

`injected_runtime`의 동적 resolver는 `DirectDrawCreate`와 `DirectDrawCreateEx`만 대체한다. `DirectDrawEnumerateExA`는 호스트의 실제 `ddraw.dll`로 가므로 드라이버 목록과 각 GUID는 호스트가 정한다. 조건의 두 번째 항(`GUID == NULL`)은 호스트가 주 표시 드라이버를 NULL GUID로 열거하는 관례에 의존한다.

- **미확정 — 이번 실행의 두 드라이버 중 NULL GUID가 있는지.** `DirectDrawCreateEx`의 `driver_guid` 인자를 아직 기록하지 않는다.

---

## 4. 가설 판정 (Hypothesis Verdicts)

| 가설 | 판정 |
| - | - |
| 게이트를 쓰는 코드가 스택 변위로 접혀 있다 | **확인.** `mov dword [ebp-0x0c], 1` |
| 조건이 caps 질의 결과다 | **확인.** driver `DDCAPS.dwCaps2` |
| 조건이 드라이버 GUID 검사다 | **확인.** 두 조건의 AND다 |
| 조건이 표시 모드 질의 결과다 | **반증.** `GetDisplayMode`는 호출되지 않는다 |

---

## 5. 다음 작업 (Next Task)

`Dd7GetCaps`가 `dwCaps2`를 채우도록 고친다. 최소한 `DDCAPS2_CANRENDERWINDOWED`가 필요하다. 같은 작업에서 `Re2djHleDirectDrawCreateEx`가 받은 `driver_guid`가 NULL인지 그래픽 추적에 남겨 조건의 두 번째 항을 관측 가능하게 만든다. 그 뒤 guard 1이 통과하는지, 다음 중단 지점이 어디인지 확인한다.

Fix `Dd7GetCaps` to fill `dwCaps2`, at minimum with `DDCAPS2_CANRENDERWINDOWED`. In the same task, record whether the `driver_guid` handed to `Re2djHleDirectDrawCreateEx` is NULL so the condition's second term becomes observable, then check whether guard 1 passes and where execution stops next.

---

## 6. 관련 문서 (Related Documents)

- [Task 172 설계](../design/20260904-172-ez2dj4th-driver-stage-gate-condition.md)
- [Task 172 작업 지시서](../work-orders/20260904-172-ez2dj4th-driver-stage-gate-condition.md)
- [Task 171 작업 로그](../work-logs/20260904-171-ez2dj4th-device-record-gate-writer.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)
