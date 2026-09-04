# Task 173: EZ2DJ 4th `DDCAPS2` 보고와 게이트 개방 확인

## 작업 목표

`Dd7GetCaps`가 `dwCaps2`를 보고하도록 고쳐 드라이버 단계의 게이트 조건 첫 항을 만족시키고, 게이트가 실제로 열리는지 관측합니다.

## 선행 문서

- [Task 173 설계](../design/20260904-173-ez2dj4th-ddcaps2-windowed.md)
- [Task 172 작업 로그](../work-logs/20260904-172-ez2dj4th-driver-stage-gate-condition.md)
- [Task 171 작업 로그](../work-logs/20260904-171-ez2dj4th-device-record-gate-writer.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)

## 구현 범위

1. **`dwCaps2` 보고.** `src/platform/windows/directdraw7_com_facade.cpp`의 `Dd7GetCaps`가 `DDCAPS2_CERTIFIED | DDCAPS2_NOPAGELOCKREQUIRED | DDCAPS2_WIDESURFACES | DDCAPS2_CANRENDERWINDOWED`를 채웁니다.
2. **드라이버 GUID 추적.** `Re2djHleDirectDrawCreateEx`가 받은 `driver_guid`가 NULL인지, NULL이 아니면 그 값을 그래픽 추적에 남깁니다.
3. **진단 실행과 판정.** 참조 스캔과 진입 추적을 실행해 레코드의 `+0x4c8`과 guard 1의 진행을 확인하고 설계 4절 기준으로 판정합니다.
4. **문서 갱신.** 작업 로그와 `docs/analysis/ez2dj4th-hardlock-runtime.md`를 갱신하고 확인됨 / 추정 / 미확정을 구분해 표기합니다.

## 비범위

- `DirectDrawEnumerateEx` HLE 도입.
- 게스트 코드 patch 또는 게이트 필드 직접 주입.
- `direct3d3_com_facade`(DirectX 6 경로) 변경.
- `dwCaps` 또는 `ddsCaps` 변경.

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
.\build\windows-x86\bin\Debug\re2dj_windows_product_loader_probe.exe
```

실제 CHD가 있으면 두 진단을 확장 idle 경계로 실행합니다.

```powershell
$common = '--hdd', "$env:TEMP\re2dj\chd\ez2dj4th", '--target', 'ez2dj4th',
  '--chd', 'roms\ez2dj4th\4thTrax.chd',
  '--target-executable', 'EZ2DJ/EZ2DJ.EXE',
  '--hle-vfs', '--hle-dynamic-vfs', '--hle-d3d3', '--hle-io-ports',
  '--device-mock-lptdi', '--device-mock-lptdi-path-prefix', '\\.\FEnteDev',
  '--device-mock-wts-console-session', '--diagnostic-idle-timeout', '60000'
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe @common --null-context-object-reference-scan
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe @common --null-context-entry-trace
```

## 자기 검증 기준

- 레코드 창의 `+0x128`(`DDCAPS.dwCaps2`)이 0이 아니면 새 caps가 게스트 레코드까지 도달한 것입니다.
- `guard1_helper_call_0` 또는 `guard1_helper_call_1`이 한 건이라도 기록되면 게이트가 열려 GUID 비교에 도달한 것입니다.
- 로그에는 원본 자산 내용과 Hardlock secret 값을 기록하지 않습니다.

---

# Task 173: EZ2DJ 4th `DDCAPS2` Reporting and Gate Verification

## Goal

Make `Dd7GetCaps` report `dwCaps2` so the driver stage's first gate term is satisfied, and observe whether the gate actually opens.

## Scope

1. Fill `dwCaps2` with `DDCAPS2_CERTIFIED | DDCAPS2_NOPAGELOCKREQUIRED | DDCAPS2_WIDESURFACES | DDCAPS2_CANRENDERWINDOWED` in `Dd7GetCaps`.
2. Trace whether the `driver_guid` handed to `Re2djHleDirectDrawCreateEx` is NULL, and its value when it is not.
3. Run the reference scan and the entry trace, then apply the design's decision criteria.
4. Update the work log and the analysis topic, marking each statement confirmed, inferred, or unresolved.

## Out of Scope

`DirectDrawEnumerateEx` HLE, guest patching or gate injection, DirectX 6 path changes, and `dwCaps` or `ddsCaps` changes.

## Minimum Verification

Build, unit tests, and the product loader probe, then the reference scan and entry trace against the real CHD.

## Self-Check

A nonzero `+0x128` in the record window means the new caps reached the guest's record. Any recorded `guard1_helper_call_0` or `guard1_helper_call_1` means the gate opened and the GUID comparison was reached. Logs never record original asset contents or Hardlock secrets.
