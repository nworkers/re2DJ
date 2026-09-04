# Task 171: EZ2DJ 4th 장치 레코드 게이트 writer 추적

## 작업 목표

Task 170이 남긴 미확정 항목 — `record + 0x4c8`을 채우는 코드 경로 — 의 후보를 실행 중인 자식 프로세스의 `.text`에서 특정합니다. 함께 레코드 0·1의 실제 내용을 훑어 열거 콜백이 어디까지 채웠는지 확정합니다.

## 선행 문서

- [Task 171 설계](../design/20260904-171-ez2dj4th-device-record-gate-writer.md)
- [Task 170 작업 로그](../work-logs/20260904-170-ez2dj4th-device-selection-inputs.md)
- [Task 169 작업 로그](../work-logs/20260904-169-ez2dj4th-guard1-failure-source.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)

## 구현 범위

1. **레코드 상수 정의.** 장치 레코드 테이블 RVA(`0x00546d50`), 개수 전역 RVA(`0x0054cd9c`), stride(`0x4d0`), 게이트 오프셋(`0x4c8`)을 명명된 상수로 둡니다.
2. **상수 스캔 확장.** 참조 스캔의 `scans[]` 표에 `device_table_base`, `device_count_global`, `device_record_stride`, `device_gate_displacement` 네 항목을 추가합니다.
3. **레코드 창 스캔.** 기존 `ScanObjectWindow` 헬퍼로 레코드 0과 1의 `0x4d0` 바이트를 훑어 0이 아닌 dword를 `device_record_window` / `device_record_field` 이벤트로 기록합니다.
4. **진단 실행과 판정.** 참조 스캔을 실행하고 설계 5절의 판정 기준으로 게이트 writer 후보를 좁힙니다.
5. **문서 갱신.** 작업 로그와 `docs/analysis/ez2dj4th-hardlock-runtime.md`를 관측 결과로 갱신하고 확인됨 / 추정 / 미확정을 구분해 표기합니다.

## 비범위

- 관측 전 HLE facade 수정.
- 게이트 필드 직접 주입 또는 게스트 코드 patch.
- 새 CLI 옵션 추가. 기존 `--null-context-object-reference-scan`을 재사용합니다.
- `direct3d3_com_facade`(DirectX 6 경로) 변경.

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
.\build\windows-x86\bin\Debug\re2dj_windows_product_loader_probe.exe
```

실제 CHD가 있으면 참조 스캔을 확장 idle 경계로 실행합니다.

```powershell
$common = '--hdd', "$env:TEMP\re2dj\chd\ez2dj4th", '--target', 'ez2dj4th',
  '--chd', 'roms\ez2dj4th\4thTrax.chd',
  '--target-executable', 'EZ2DJ/EZ2DJ.EXE',
  '--hle-vfs', '--hle-dynamic-vfs', '--hle-d3d3', '--hle-io-ports',
  '--device-mock-lptdi', '--device-mock-lptdi-path-prefix', '\.\FEnteDev',
  '--device-mock-wts-console-session', '--diagnostic-idle-timeout', '60000'
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe @common --null-context-object-reference-scan
```

## 자기 검증 기준

- `device_table_base` 일치가 하나도 없으면 스캔 시점의 image base 계산이 잘못된 것입니다. Task 170이 읽은 레코드 내용과 대조합니다.
- 레코드 0 창의 `+0x2c` 값이 `0x0008af51`이면 Task 170과 같은 상태를 읽고 있다는 뜻입니다.
- 로그에는 원본 자산 내용과 Hardlock secret 값을 기록하지 않습니다. 기록 대상은 실행 파일 안의 프로그램 상수와 관측된 필드 값에 한정합니다.

---

# Task 171: EZ2DJ 4th Device Record Gate Writer Tracing

## Goal

Identify, from the running child's `.text`, the candidate code path that fills `record + 0x4c8` — the item Task 170 left unresolved — and record how much of records 0 and 1 the enumeration callback actually filled.

## Scope

1. Name the device record table RVA (`0x00546d50`), the count global RVA (`0x0054cd9c`), the stride (`0x4d0`), and the gate offset (`0x4c8`) as constants.
2. Add `device_table_base`, `device_count_global`, `device_record_stride`, and `device_gate_displacement` to the reference scan's immediate scan table.
3. Walk `0x4d0` bytes of records 0 and 1 with the existing `ScanObjectWindow` helper and record every nonzero dword.
4. Run the reference scan and apply the design's decision criteria.
5. Update the work log and `docs/analysis/ez2dj4th-hardlock-runtime.md`, marking each statement confirmed, inferred, or unresolved.

## Out of Scope

HLE facade changes before observation, gate injection or guest patching, new CLI options, and any change to the DirectX 6 path.

## Minimum Verification

Build, unit tests, and the product loader probe as above; then the reference scan against the real CHD with an extended idle boundary.

## Self-Check

No `device_table_base` match means the image base used by the scan is wrong. A record 0 window whose `+0x2c` reads `0x0008af51` confirms the same state Task 170 observed. Logs record program constants and observed field values only — never original asset contents or Hardlock secrets.
