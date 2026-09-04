# Task 169: EZ2DJ 4th guard 1 실패 원인 추적

## 작업 목표

guard 1(`RVA 0x00011738`)에서 이탈하게 만드는 호출의 반환값을 실행으로 관측하고, 그 실패 코드가 생성되는 지점과 직전 연산을 특정해 초기화가 중단되는 직접 원인을 확정합니다.

## 선행 문서

- [Task 169 설계](../design/20260904-169-ez2dj4th-guard1-failure-source.md)
- [Task 168 작업 로그](../work-logs/20260904-168-ez2dj4th-d3d7-init-abort.md)
- [Task 163 작업 로그](../work-logs/20260903-163-ez2dj4th-guard-failure-source.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)

## 구현 범위

### 1단계

1. `kNullContextEntryPoints`를 `guard0_return`(`0x00011706`), `guard1_call_site`(`0x00011725`), `guard1_return`(`0x0001172a`), `slot2_early_exit_1`(`0x00011738`)로 재조준합니다.
2. 참조 스캔 `bodies` 목록에 `{"guard1_thunk", 0x00003913, 0x00000008, 0}`을 추가해 thunk의 최종 대상을 같은 실행에서 얻습니다.
3. 진단 실행으로 guard 1 반환값과 thunk 대상 RVA를 기록합니다.

### 2단계

4. 1단계에서 얻은 대상 함수 RVA를 `bodies`에 `guard1_target`으로 추가합니다.
5. 1단계에서 얻은 실패 코드를 참조 스캔 값 목록에 추가해 `.text`의 생성 지점을 모두 찾습니다.
6. 생성 지점과 그 주변을 앵커 목록에 추가해 코드 창을 수집합니다.
7. 설계·작업 로그와 `docs/analysis/ez2dj4th-hardlock-runtime.md`를 관측 결과로 갱신하고, 확인됨 / 추정 / 미확정을 구분해 표기합니다.

## 비범위

- 실패하는 연산의 동작 변경.
- `0x00acd708 + 0x11c` field 직접 주입 또는 게스트 코드 patch.
- Hardlock 응답 material 변경.
- 열거 데이터의 추가 확장.
- `direct3d3_com_facade`(DirectX 6 경로) 변경.
- 새 CLI 옵션 추가. 기존 `--null-context-entry-trace`와 `--null-context-object-reference-scan`을 재사용합니다.

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
.\build\windows-x86\bin\Debug\re2dj_windows_product_loader_probe.exe
```

실제 CHD가 있으면 두 진단을 확장 idle 경계로 실행합니다.

```powershell
$common = '--hdd', "$env:TEMP\re2dj\chd\ez2dj4th", '--target', 'ez2dj4th',
  '--chd', 'E:\MYWORK\Projects\re2DJ\roms\ez2dj4th\4thTrax.chd',
  '--target-executable', 'EZ2DJ/EZ2DJ.EXE',
  '--hle-vfs', '--hle-dynamic-vfs', '--hle-d3d3', '--hle-io-ports',
  '--device-mock-lptdi', '--device-mock-lptdi-path-prefix', '\\.\FEnteDev',
  '--device-mock-wts-console-session', '--diagnostic-idle-timeout', '60000'
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe @common --null-context-entry-trace
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe @common --null-context-object-reference-scan
```

## 자기 검증 기준

- `guard0_return`의 `EAX`가 성공 값이고 `guard1_return`의 `EAX`가 실패 값이어야 Task 168의 guard 1 판정과 일치합니다.
- thunk `0x00003913`의 분기 목록 항목 수는 guard 2 thunk(`0x0000317f`, 2건)와 같은 형태여야 합니다.
- 실패 코드의 생성 지점이 `.text`에서 유일하면 그 자체가 판정 근거가 됩니다.
- 로그에는 원본 자산 내용과 Hardlock secret 값을 기록하지 않습니다.

---

# Task 169: EZ2DJ 4th Guard 1 Failure Source

## Objective

Observe at runtime the return value of the call that makes guard 1 (`RVA 0x00011738`) exit, then identify where that failure code is produced and which operation precedes it, establishing the direct cause of the halted initialization.

## Preceding documents

- [Task 169 design](../design/20260904-169-ez2dj4th-guard1-failure-source.md)
- [Task 168 work log](../work-logs/20260904-168-ez2dj4th-d3d7-init-abort.md)
- [Task 163 work log](../work-logs/20260903-163-ez2dj4th-guard-failure-source.md)
- [4th Hardlock runtime analysis](../analysis/ez2dj4th-hardlock-runtime.md)

## Implementation scope

### Stage 1

1. Retarget `kNullContextEntryPoints` to `guard0_return` (`0x00011706`), `guard1_call_site` (`0x00011725`), `guard1_return` (`0x0001172a`), and `slot2_early_exit_1` (`0x00011738`).
2. Add `{"guard1_thunk", 0x00003913, 0x00000008, 0}` to the reference scan's `bodies` list so the thunk's final target comes from the same run.
3. Run the diagnostics and record guard 1's return value and the thunk target RVA.

### Stage 2

4. Add the target function RVA from stage 1 to `bodies` as `guard1_target`.
5. Add the failure code from stage 1 to the reference scan's value list to find every site in `.text` that produces it.
6. Add the producing site and its surroundings to the anchor list to collect code windows.
7. Update the design, the work log, and `docs/analysis/ez2dj4th-hardlock-runtime.md` with the observations, marking each statement confirmed, inferred, or unresolved.

## Out of scope

- Changing the behavior of the failing operation.
- Direct injection into `0x00acd708 + 0x11c`, or patching guest code.
- Changing Hardlock response material.
- Further enumeration expansion.
- Changing `direct3d3_com_facade` (the DirectX 6 path).
- Adding a new CLI option; reuse `--null-context-entry-trace` and `--null-context-object-reference-scan`.

## Minimum verification

Build, unit tests, and the product loader probe as in the PowerShell block above, then run both diagnostics with the extended idle boundary when the real CHD is available.

## Self-check criteria

- `EAX` at `guard0_return` must be a success value and `EAX` at `guard1_return` a failure value, matching Task 168's guard 1 verdict.
- The branch listing for thunk `0x00003913` must have the same shape as the guard 2 thunk (`0x0000317f`, two entries).
- If the failure code's producing site is unique in `.text`, that uniqueness is itself the evidence.
- The logs must not contain original asset contents or Hardlock secret values.
