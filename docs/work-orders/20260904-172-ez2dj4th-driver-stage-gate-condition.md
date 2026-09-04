# Task 172: EZ2DJ 4th 드라이버 단계 게이트 조건 관측

## 작업 목표

`EnumDevices` 콜백에 전달되는 컨텍스트의 `+0x4c8`을 무엇이 결정하는지 드라이버 단계 코드에서 읽어 확정합니다.

## 선행 문서

- [Task 172 설계](../design/20260904-172-ez2dj4th-driver-stage-gate-condition.md)
- [Task 171 작업 로그](../work-logs/20260904-171-ez2dj4th-device-record-gate-writer.md)
- [Task 170 작업 로그](../work-logs/20260904-170-ez2dj4th-device-selection-inputs.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)

## 구현 범위

1. **코드 범위 덤프.** 참조 스캔이 이미 읽어 둔 `.text` 버퍼에서 지정 RVA 범위를 64바이트 단위로 잘라 `device_enum_code_chunk` 이벤트로 기록하는 블록을 추가합니다. 대상은 `driver_stage`(`0x0000f700`, 길이 `0x680`)입니다.
2. **앵커 추가.** 앵커 표에 `driver_stage_context_zero`(`0x0000f93e`)와 `device_enum_callback`(`0x0000fc57`)을 추가해 prologue와 호출자를 함께 얻습니다.
3. **진단 실행과 판정.** 참조 스캔을 실행하고 설계 4절의 판정 기준으로 게이트 조건을 확정합니다.
4. **문서 갱신.** 작업 로그와 `docs/analysis/ez2dj4th-hardlock-runtime.md`를 관측 결과로 갱신하고 확인됨 / 추정 / 미확정을 구분해 표기합니다.

## 비범위

- 관측 전 HLE facade 수정.
- 게이트 필드 직접 주입 또는 게스트 코드 patch.
- 새 CLI 옵션 추가.
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
  '--device-mock-lptdi', '--device-mock-lptdi-path-prefix', '\\.\FEnteDev',
  '--device-mock-wts-console-session', '--diagnostic-idle-timeout', '60000'
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe @common --null-context-object-reference-scan
```

## 자기 검증 기준

- 덤프한 범위 안에서 `0x0000fc95`의 `add edx, 0x00946d50`과 `0x0000fce7`의 `mov [eax+0x4c8], edx`가 그대로 보이면 범위와 정렬이 맞는 것입니다.
- `device_enum_callback` 앵커의 prologue RVA가 `0x0000fc57` 이하이면 콜백 함수 시작을 옳게 찾은 것입니다.
- 로그에는 원본 자산 내용과 Hardlock secret 값을 기록하지 않습니다. 기록 대상은 실행 파일 안의 코드 바이트와 프로그램 상수에 한정합니다.

---

# Task 172: EZ2DJ 4th Driver Stage Gate Condition Observation

## Goal

Read the driver-stage code to establish what decides the `+0x4c8` field of the context passed to the `EnumDevices` callback.

## Scope

1. Add a block that slices a requested RVA range out of the `.text` buffer the reference scan already holds, recording 64-byte chunks as `device_enum_code_chunk` events, covering `0x0000f700` for `0x680` bytes.
2. Add the `driver_stage_context_zero` (`0x0000f93e`) and `device_enum_callback` (`0x0000fc57`) anchors so the existing prologue search and branch scan also report each function's start and callers.
3. Run the reference scan and apply the design's decision criteria.
4. Update the work log and `docs/analysis/ez2dj4th-hardlock-runtime.md`, marking each statement confirmed, inferred, or unresolved.

## Out of Scope

HLE facade changes before observation, gate injection or guest patching, new CLI options, and any change to the DirectX 6 path.

## Minimum Verification

Build, unit tests, and the product loader probe, then the reference scan against the real CHD with an extended idle boundary.

## Self-Check

Seeing `add edx, 0x00946d50` at `0x0000fc95` and `mov [eax+0x4c8], edx` at `0x0000fce7` inside the dump confirms the range and alignment. A `device_enum_callback` prologue at or below `0x0000fc57` confirms the function start was found. Logs record code bytes and program constants only — never original asset contents or Hardlock secrets.
