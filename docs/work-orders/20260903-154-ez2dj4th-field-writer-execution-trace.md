# Task 154: EZ2DJ 4th field writer 실행 추적

## 작업 목표

복호화된 `ez2dj4th` 런타임의 `+0x11c` 직접 write 후보 네 개를 실제 실행 중 bounded hardware execution trace로 관찰하고, 각 후보의 receiver와 계산된 target이 `0x00acd824`인지 판정합니다. field 값·Hardlock 응답·VFS 경로는 변경하지 않습니다.

## 선행 문서

- [Task 154 설계](../design/20260903-154-ez2dj4th-field-writer-execution-trace.md)
- [Task 152 설계](../design/20260903-152-ez2dj4th-runtime-field-reference-scan.md)
- [Task 153 인계](../work-orders/20260903-153-v0.0.21-ez2dj4th-session-handoff.md)

## 구현 범위

1. `--null-context-field-reference-execution-trace` CLI 옵션과 사용법 문자열을 추가합니다.
2. 네 candidate RVA를 `DR0`–`DR3` execution breakpoint로 설정하는 helper를 추가합니다.
3. CREATE_THREAD event에서 새 thread에 candidate breakpoint를 설정합니다.
4. candidate hit에서 receiver, write value, 계산 target, immediate read 상태를 JSONL로 기록합니다.
5. 원본 instruction을 한 번 실행하기 위한 TF single-step 복구 경로를 추가합니다.
6. 기존 하드웨어 debug-register 추적 옵션과의 충돌, preparation 상태, bounded summary를 연결합니다.
7. 설계·작업 로그와 관련 누적 분석 문서를 갱신합니다.

## 비범위

- `0x00acd824` 직접 주입 또는 임시 patch
- Hardlock response material 변경
- 물리 4th I/O 응답을 product default로 승격
- 원본 CHD/HDD/EXE 또는 secret material 저장

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
```

가능한 경우 사용자가 제공한 실제 CHD를 다음 진단 옵션으로 실행합니다.

```powershell
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe --hdd <staging-hdd> --chd <4thTrax.chd> --target ez2dj4th --target-executable EZ2DJ\EZ2DJ.EXE --hle-io-ports --device-mock-lptdi --device-mock-wts-console-session --null-context-field-reference-execution-trace
```

실행 로그에는 원본 자산 경로나 Hardlock secret 값 자체를 기록하지 않습니다.

---

# Task 154: EZ2DJ 4th Field-Writer Execution Trace

## Objective

Observe the four direct `+0x11c` write candidates in the decrypted `ez2dj4th` runtime with a bounded hardware execution trace and determine whether each receiver and calculated target equals `0x00acd824`. Do not change the field value, Hardlock responses, or VFS path.

## Preceding documents

- [Task 154 design](../design/20260903-154-ez2dj4th-field-writer-execution-trace.md)
- [Task 152 design](../design/20260903-152-ez2dj4th-runtime-field-reference-scan.md)
- [Task 153 handoff](../work-orders/20260903-153-v0.0.21-ez2dj4th-session-handoff.md)

## Implementation scope

1. Add the `--null-context-field-reference-execution-trace` CLI option and usage text.
2. Add a helper that installs the four candidate RVAs as `DR0`–`DR3` execution breakpoints.
3. Install the candidate breakpoints for every thread at CREATE_THREAD events.
4. Record receiver, write value, calculated target, and immediate-read status as JSONL on candidate hits.
5. Add the TF single-step path needed to execute each original instruction once and restore the breakpoints.
6. Connect hardware-debug-register conflicts, preparation state, and bounded summaries.
7. Update the design, work log, and related cumulative analysis document.

## Out of scope

- Direct injection or temporary patching of `0x00acd824`
- Changing Hardlock response material
- Promoting physical 4th I/O responses to product defaults
- Storing the original CHD/HDD/EXE or secret material

## Minimum verification

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
```

When the user-supplied real CHD is available, run the diagnostic with the following options:

```powershell
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe --hdd <staging-hdd> --chd <4thTrax.chd> --target ez2dj4th --target-executable EZ2DJ\EZ2DJ.EXE --hle-io-ports --device-mock-lptdi --device-mock-wts-console-session --null-context-field-reference-execution-trace
```

The execution log must not contain the original asset contents or Hardlock secret values.
