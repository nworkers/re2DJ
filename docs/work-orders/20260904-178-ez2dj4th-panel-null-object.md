# Task 178: EZ2DJ 4th 패널 단계 null 객체 관측

## 작업 목표

`RVA 0x0001290e`의 접근 위반에서 null인 `arg1->[0x08]`이 어디서 채워졌어야 하는지 특정합니다.

## 선행 문서

- [Task 178 설계](../design/20260904-178-ez2dj4th-panel-null-object.md)
- [Task 177 작업 로그](../work-logs/20260904-177-vfs-guest-working-directory.md)
- [Task 175 작업 로그](../work-logs/20260904-175-ez2dj4th-uninitialized-out-parameter.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)

## 구현 범위

1. **코드 범위 교체.** 참조 스캔의 `code_ranges`를 `av2_callee`(`0x00012880`, `0x100`), `av2_caller`(`0x00038200`, `0x220`), `av2_outer`(`0x0003f780`, `0x120`)로 바꿉니다.
2. **앵커 추가.** faulting 명령 `0x0001290e`를 앵커 목록에 넣어 prologue와 분기 스캔을 함께 얻습니다.
3. **진단 실행과 판정.** 참조 스캔을 실행해 세 구간을 덤프하고 설계 5절 기준으로 판정합니다. 같은 실행의 `.vfs.log`와 대조해 직전에 적재된 자원을 확인합니다.
4. **문서 갱신.** 작업 로그와 `docs/analysis/ez2dj4th-hardlock-runtime.md`를 갱신하고 확인됨 / 추정 / 미확정을 구분해 표기합니다.

## 비범위

- 관측 전 HLE facade 수정.
- 게스트 코드 patch 또는 객체 주입.
- 새 CLI 옵션 추가.
- 오디오 경로 HLE 도입.

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
.\build\windows-x86\bin\Debug\re2dj_windows_product_loader_probe.exe
```

실제 CHD가 있으면 참조 스캔과 진입 추적을 확장 idle 경계로 실행합니다.

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

- `av2_callee` 덤프 안에서 `0x0001290e`의 `8b 0a 50 ff 51 0c`가 보이면 범위와 정렬이 맞는 것입니다.
- `av2_caller` 덤프가 `0x0003825a`와 `0x000383fd`를 모두 포함해야 두 프레임을 함께 읽을 수 있습니다.
- 로그에는 코드 바이트와 프로그램 상수만 남기고 원본 자산 내용은 남기지 않습니다.

---

# Task 178: Observing the EZ2DJ 4th Panel-Stage Null Object

## Goal

Establish where the null `arg1->[0x08]` behind the access violation at `RVA 0x0001290e` should have come from.

## Scope

1. Point the reference scan's code ranges at `av2_callee` (`0x00012880`, `0x100`), `av2_caller` (`0x00038200`, `0x220`), and `av2_outer` (`0x0003f780`, `0x120`).
2. Add the faulting instruction `0x0001290e` as an anchor for the prologue and branch scans.
3. Run the reference scan, apply the design's decision criteria, and cross-check the same run's `.vfs.log` for the resources loaded just before.
4. Update the work log and the analysis topic, marking each statement confirmed, inferred, or unresolved.

## Out of Scope

Facade changes before observation, guest patching, new CLI options, and routing the audio path through HLE.

## Minimum Verification

Build, unit tests, and the product loader probe, then the reference scan and entry trace against the real CHD.

## Self-Check

Seeing `8b 0a 50 ff 51 0c` at `0x0001290e` inside the dump confirms range and alignment, and the caller range must cover both `0x0003825a` and `0x000383fd`. Logs record code bytes and program constants only.
