# 20260904-173 EZ2DJ 4th `DDCAPS2` 보고와 게이트 개방 결과
# 20260904-173 EZ2DJ 4th `DDCAPS2` Reporting and Gate Opening Results

## 1. 개요 (Overview)

Task 172가 확정한 근본 원인을 고쳤다.

**결론: `Dd7GetCaps`가 `dwCaps2`를 보고하자 게이트가 열렸다. 레코드의 `+0x4c8`이 1이 되고, 선택 루프가 GUID 비교에 도달했으며, D3D7 초기화가 `SetCooperativeLevel` → `SetDisplayMode` → `CreateSurface` → `IDirect3D7::CreateDevice` → `EnumZBufferFormats` → z-buffer와 텍스처 surface 생성까지 진행했다. Task 165 이후 처음으로 초기화가 장치 생성 단계를 넘어섰다.**

**새 중단 지점: `RVA 0x000c384b`의 `out dx, al`이 처리되지 않아 프로세스가 `0xc0000096`으로 종료한다. ez2dj4th 프로필은 `legacy_io_in_byte_rva`만 설정하고 out 경로 RVA를 비워 두고 있다.**

Filling `dwCaps2` in `Dd7GetCaps` opened the gate: `record + 0x4c8` is now 1, the selection loop reaches the GUID comparison, and D3D7 initialization advances through `SetCooperativeLevel`, `SetDisplayMode`, `CreateSurface`, `IDirect3D7::CreateDevice`, `EnumZBufferFormats`, and z-buffer and texture surface creation. The run now stops at an untrapped `out dx, al` at `RVA 0x000c384b`, because the ez2dj4th profile sets only `legacy_io_in_byte_rva`.

---

## 2. 변경 내용 (Changes Implemented)

`src/platform/windows/directdraw7_com_facade.cpp`만 변경했다.

1. **`dwCaps2` 보고.** `Dd7GetCaps`가 `DDCAPS2_CERTIFIED | DDCAPS2_NOPAGELOCKREQUIRED | DDCAPS2_WIDESURFACES | DDCAPS2_CANRENDERWINDOWED`(`0x00081801`)을 채운다. `dwCaps`와 `ddsCaps`는 그대로 두었다.
2. **드라이버 GUID 추적.** `Re2djHleDirectDrawCreateEx`가 받은 `driver_guid`를 `driver=null` 또는 GUID 문자열로 그래픽 추적에 남긴다.

---

## 3. 검증 결과 (Verification Results)

- `scripts/build_win32.bat`: 빌드 성공, 경고와 에러 0.
- `re2dj_unit_tests.exe`: 1,253 checks, 0 failures.
- `re2dj_windows_product_loader_probe.exe`: 전체 통과.
- 참조 스캔: `logs/windows_x86_launcher_probe/ez2dj4th/20260904-141314-210.jsonl`.
- 진입 추적: `logs/windows_x86_launcher_probe/ez2dj4th/20260904-141410-225.jsonl`.

### 3.1 게이트가 열렸다 (확인됨)

레코드 창에서 두 레코드 모두 게이트가 1이다.

| 필드 | Task 172 이전 | 이번 실행 |
| - | - | - |
| `+0x128` (`DDCAPS.dwCaps2`) | 0 | `0x00081801` |
| `+0x2a4` (HEL `DDCAPS.dwCaps2`) | 0 | `0x00081801` |
| `+0x494` (게이트 사본) | 0 | `0x00000001` |
| `+0x4c8` (게이트) | 0 | `0x00000001` |

### 3.2 조건의 두 번째 항도 성립한다 (확인됨)

`.ddraw.log`가 드라이버 세 개를 기록했다.

```
DirectDrawCreateEx driver=null
DirectDrawCreateEx driver={67685559-3106-11d0-b971-00aa00342f9f}
DirectDrawCreateEx driver=null
```

호스트가 주 표시 드라이버를 NULL GUID로 열거하므로 `cmp dword [ebp-0x3b8], 0`이 성립하는 패스가 존재한다. Task 172의 미확정 항목이 해소되었다.

### 3.3 선택 루프가 GUID 비교에 도달한다 (확인됨)

진입 추적의 앵커 hit 분포다.

| 앵커 | Task 170 | 이번 실행 |
| - | - | - |
| `guard1_loop_head` | 8 | 7 |
| `guard1_helper_call_0` | 0 | **4** |
| `guard1_helper_call_1` | 0 | **1** |
| `guard1_decision_start` | 1 | 2 |

`guard1_helper_call_0`의 `stack_arg0` 위치에 놓인 레코드 GUID 포인터가 `0x009471ec`, `0x009476bc`, `0x00947b8c`로 레코드 0·1·2의 `+0x49c`와 정확히 맞는다.

### 3.4 초기화가 장치 생성까지 진행한다 (확인됨)

`.ddraw.log`의 마지막 구간이다.

```
IDirectDraw7::SetCooperativeLevel hwnd=0x002901d4 flags=0x00000813
IDirectDraw7::GetDisplayMode
IDirectDraw7::SetDisplayMode 640x480x16
IDirectDraw7::CreateSurface flags=0x00000021 caps=0x00002218
IDirectDraw7::QueryInterface iid={f5049e77-4861-11d2-a407-00a0c90629a8}
IDirect3D7::CreateDevice
IDirect3D7::EnumZBufferFormats
IDirect3D7::EnumZBufferFormats
IDirectDraw7::CreateSurface flags=0x00001007 caps=0x00024000
IDirectDraw7::CreateSurface flags=0x00001007 caps=0x10005000 128x128
```

- **확인됨 — guard 1을 통과했다.** 이전 실행에서는 `EnumDevices` 세 벌 뒤 곧바로 초기화가 중단되었고, 이번에는 협조 수준 설정과 표시 모드 전환을 거쳐 장치와 surface를 만든다.
- **확인됨 — `GetDisplayMode`가 이제 호출된다.** Task 172에서 한 번도 호출되지 않던 경로다.

### 3.5 새 중단 지점은 I/O out 경로다 (확인됨)

```
{"event":"privileged_instruction","first_chance":true,"address":"0x004c384b","kind":"out_dx_al",
 "bytes":"eec3668b542404668b44240866efc366","edx":"0x00000100","port":"0x0100"}
{"event":"privileged_instruction","first_chance":false,"address":"0x004c384b", ...}
{"debug_event":"exit_process","code":"0xc0000096"}
```

- **확인됨 — in 경로는 처리된다.** `RVA 0x000c3817`의 `in al, dx`는 포트 `0x0101`–`0x0106`에 대해 열 번 처리되었다.
- **확인됨 — out 경로는 트랩되지 않는다.** `src/target/target_profile.cpp`의 ez2dj4th 프로필은 `legacy_io_in_byte_rva = 0x000c3817`만 설정하고 `legacy_io_out_byte_rva`를 비워 둔다. 정책은 두 RVA를 각각 비교하므로 out helper는 통과하지 못한다.
- **확인됨 — out helper의 RVA는 `0x000c384b`다.** 관측된 바이트 `ee c3 66 8b 54 24 04 66 8b 44 24 08 66 ef c3`는 `out dx, al; ret`에 이어 워드 폭 out helper가 오는 배치다.

---

## 4. 가설 판정 (Hypothesis Verdicts)

| 가설 | 판정 |
| - | - |
| `dwCaps2` 누락이 게이트 0의 원인이다 | **확인.** 채우자 게이트가 1이 되었다 |
| 호스트가 NULL GUID 드라이버를 열거한다 | **확인.** 세 패스 중 두 패스가 `driver=null` |
| 게이트가 열리면 guard 1을 통과한다 | **확인.** GUID 비교에 도달하고 초기화가 계속된다 |

---

## 5. 다음 작업 (Next Task)

ez2dj4th 프로필에 `legacy_io_out_byte_rva = 0x000c384b`를 설정한다. in 경로와 대칭이며, 이번 실행이 그 RVA와 명령 형태를 직접 보여 주었다. 설정 뒤 다음 중단 지점을 다시 관측한다.

Set `legacy_io_out_byte_rva = 0x000c384b` in the ez2dj4th profile — symmetric with the in path, and this run showed both the RVA and the instruction form directly — then observe where execution stops next.

---

## 6. 관련 문서 (Related Documents)

- [Task 173 설계](../design/20260904-173-ez2dj4th-ddcaps2-windowed.md)
- [Task 173 작업 지시서](../work-orders/20260904-173-ez2dj4th-ddcaps2-windowed.md)
- [Task 172 작업 로그](../work-logs/20260904-172-ez2dj4th-driver-stage-gate-condition.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)
