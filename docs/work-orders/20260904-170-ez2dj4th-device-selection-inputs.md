# Task 170: EZ2DJ 4th 장치 선택 입력 관측

## 작업 목표

guard 1의 선택 루틴(`RVA 0x0001010f`)이 후보를 하나도 채우지 못하는 이유를 관측으로 확정합니다. 루프의 실제 반복 횟수, helper `0x00012820`의 반환값, 그리고 비교 대상인 두 `.rdata` 상수의 내용을 같은 실행에서 얻습니다.

## 선행 문서

- [Task 170 설계](../design/20260904-170-ez2dj4th-device-selection-inputs.md)
- [Task 169 작업 로그](../work-logs/20260904-169-ez2dj4th-guard1-failure-source.md)
- [Task 168 작업 로그](../work-logs/20260904-168-ez2dj4th-d3d7-init-abort.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)

## 구현 범위

1. **데이터 창 추가.** 참조 스캔에 `.rdata` 주소의 바이트 창을 읽는 최소 확장을 넣습니다. 대상은 `guard1_match_string_0`(`0x000e4da0`)과 `guard1_match_string_1`(`0x000e4dc0`)이며, 16진수와 출력 가능 문자로 함께 기록합니다. `.rdata`는 디스크에서 암호화되어 있어 자식 프로세스 메모리에서만 읽을 수 있습니다.
2. **helper 본문 추가.** `bodies` 목록에 `{"guard1_match_helper", 0x00012820, 0x00000100, 0}`을 추가합니다.
3. **진입 앵커 재조준.** `kNullContextEntryPoints`를 `guard1_loop_head`(`0x00010174`), `guard1_helper_call_0`(`0x000101cf`), `guard1_helper_call_1`(`0x00010217`), `guard1_decision_start`(`0x0001024c`)로 교체합니다.
4. **진단 실행과 판정.** 두 진단을 실행해 반복 횟수, helper 반환값, `stack_arg0` 포인터, 두 상수의 내용을 기록하고 설계 5절의 판정 기준으로 원인을 확정합니다.
5. **문서 갱신.** 설계·작업 로그와 `docs/analysis/ez2dj4th-hardlock-runtime.md`를 관측 결과로 갱신하고, 확인됨 / 추정 / 미확정을 구분해 표기합니다.

## 비범위

- 관측 전 HLE facade 수정.
- `0x00acd708 + 0x11c` field 직접 주입 또는 게스트 코드 patch.
- Hardlock 응답 material 변경.
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

- 데이터 창이 출력 가능한 문자열을 담으면 `.rdata`가 언패킹된 상태로 읽혔다는 뜻이고, 고엔트로피 바이트면 읽기 시점이 잘못된 것입니다.
- `guard1_loop_head`의 hit 수를 `.ddraw.log`가 기록한 열거 장치 수와 대조합니다.
- 로그에는 원본 자산 내용과 Hardlock secret 값을 기록하지 않습니다. 기록 대상은 실행 파일 안의 프로그램 상수와 관측된 레지스터 값에 한정합니다.

---

# Task 170: EZ2DJ 4th Device Selection Input Observation

## Objective

Establish by observation why guard 1's selection routine (`RVA 0x0001010f`) fills no candidate. Obtain the loop's actual iteration count, helper `0x00012820`'s return values, and the content of the two `.rdata` constants it compares against, all from the same run.

## Preceding documents

- [Task 170 design](../design/20260904-170-ez2dj4th-device-selection-inputs.md)
- [Task 169 work log](../work-logs/20260904-169-ez2dj4th-guard1-failure-source.md)
- [Task 168 work log](../work-logs/20260904-168-ez2dj4th-d3d7-init-abort.md)
- [4th Hardlock runtime analysis](../analysis/ez2dj4th-hardlock-runtime.md)

## Implementation scope

1. **Add data windows.** Extend the reference scan minimally to read byte windows at `.rdata` addresses, targeting `guard1_match_string_0` (`0x000e4da0`) and `guard1_match_string_1` (`0x000e4dc0`), recorded as hex alongside a printable rendering. `.rdata` is encrypted on disk, so these are readable only from the child process.
2. **Add the helper body.** Append `{"guard1_match_helper", 0x00012820, 0x00000100, 0}` to the `bodies` list.
3. **Retarget the entry anchors.** Replace `kNullContextEntryPoints` with `guard1_loop_head` (`0x00010174`), `guard1_helper_call_0` (`0x000101cf`), `guard1_helper_call_1` (`0x00010217`), and `guard1_decision_start` (`0x0001024c`).
4. **Run and decide.** Run both diagnostics, record the iteration count, helper return values, `stack_arg0` pointers, and constant content, and apply the decision criteria in section 5 of the design.
5. **Update documentation.** Update the design, the work log, and `docs/analysis/ez2dj4th-hardlock-runtime.md`, marking each statement confirmed, inferred, or unresolved.

## Out of scope

- Changing the HLE facade before the observation.
- Direct injection into `0x00acd708 + 0x11c`, or patching guest code.
- Changing Hardlock response material.
- Changing `direct3d3_com_facade` (the DirectX 6 path).
- Adding a new CLI option; reuse `--null-context-entry-trace` and `--null-context-object-reference-scan`.

## Minimum verification

Build, unit tests, and the product loader probe as in the PowerShell block above, then run both diagnostics with the extended idle boundary when the real CHD is available.

## Self-check criteria

- Printable text in the data windows confirms `.rdata` was read in its unpacked state; high-entropy bytes would mean the read happened at the wrong time.
- Cross-check `guard1_loop_head`'s hit count against the device count recorded in `.ddraw.log`.
- The logs must not contain original asset contents or Hardlock secret values; recording stays limited to program constants inside the executable and observed register values.
