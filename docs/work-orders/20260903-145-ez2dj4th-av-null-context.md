# ez2dj4th AV null-context 원인 분리 작업 지시서

관련 설계: [ez2dj4th AV null-context 원인 분리 설계](../design/20260903-145-ez2dj4th-av-null-context.md)

## 목표

`ez2dj4th`가 `0x00434137`에서 발생시키는 null dereference와 직전 Hardlock `DeviceIoControl` 흐름의 인과관계를 제한된 실행 계측으로 판정합니다.

## 작업

1. 기존 AV 로그와 명령 바이트를 기준선으로 유지합니다.
2. 기존 `--lptdi-post-ioctl-trace`를 사용해 `0x9c40244c` 반환 직후 명령을 최대 48 step 수집합니다.
3. 필요할 때 `0x9c402450`도 같은 방식으로 수집하되, 각 실행은 별도 로그로 보존합니다.
4. post-IOCTL sample, `device_io_control_return`, `av_registers`, `av_stack_code_window`의 thread와 순서를 비교합니다.
5. raw I/O trap의 전후 EAX와 return-site code window를 기록해 `0x0103`·`0x0104`·`0x0105` 응답이 호출자에 전달되는 경로를 확인합니다.
6. AV thread와 stack-based direct-call target의 runtime code window를 기록합니다.
7. AV saved EBP를 한 단계 unwind해 caller `[EBP-8]`, 인접 local, caller return address를 기록합니다.
8. stack direct-call 후보 전후 runtime code window를 기록해 caller local 생성 명령을 확인합니다.
9. caller의 caller return-site runtime code window를 기록해 최초 null 전달자를 추가로 추적합니다.
10. 새로운 원본 코드 우회나 응답값 추정 없이 분석 문서와 작업 로그를 갱신합니다.

## 제외 범위

- `0x00434137` 주변의 branch를 강제로 통과시키지 않습니다.
- `EAX` 또는 output buffer를 임의로 non-null로 패치하지 않습니다.
- 4th 물리 I/O 보드 응답으로 shared `0x80` 값을 확정하지 않습니다.
- 제품 기본 실행 정책을 변경하지 않습니다.

## 최소 실행 절차

```powershell
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe --hdd C:\Users\nworkers\AppData\Local\Temp\re2dj\chd\ez2dj4th --chd E:\MYWORK\Projects\re2DJ\roms\ez2dj4th\4thTrax.chd --target ez2dj4th --target-executable EZ2DJ\EZ2DJ.EXE --hle-io-ports --device-mock-lptdi --device-mock-wts-console-session --device-mock-lptdi-ioctl-success --lptdi-post-ioctl-trace 48 --lptdi-post-ioctl-code 0x9c40244c
```

`--device-mock-lptdi-ioctl-success`는 bounded trace 옵션의 policy 요구를 만족시키기 위한 모드이며, configured Hardlock `0x450`/`0x44c` material과 transform map은 기존 target 설정에 따라 계속 주입됩니다. 이 옵션이 실행 경로를 바꾸면 동일한 trace를 `0x9c402450` 대상으로 반복합니다.

## 완료 조건

- trace 로그와 AV 로그의 순서·thread·EAX를 비교합니다.
- 직접 연관, 간접 연관, 미확정 중 하나로 판정하고 근거 주소를 남깁니다.
- 관련 `docs/analysis/ez2dj4th-hardlock-runtime.md`와 대응 작업 로그를 갱신합니다.
- Windows x86 빌드 또는 변경이 없을 경우 기존 진단 바이너리의 실행 증거를 남깁니다.

---

# ez2dj4th AV Null-Context Causality Work Order

Related design: [ez2dj4th AV Null-Context Causality Design](../design/20260903-145-ez2dj4th-av-null-context.md)

## Objective

Use bounded execution instrumentation to decide whether the null dereference at `0x00434137` is causally connected to the immediately preceding Hardlock `DeviceIoControl` flow.

## Tasks

1. Preserve the current AV log and instruction-byte interpretation as the baseline.
2. Collect up to 48 instructions after the `0x9c40244c` return with the existing `--lptdi-post-ioctl-trace` option.
3. If necessary, collect the same trace for `0x9c402450` in a separate log.
4. Compare the post-IOCTL samples, `device_io_control_return`, `av_registers`, and `av_stack_code_window` by thread and order.
5. Record raw-I/O pre/post EAX and the return-site code window to follow the `0x0103`, `0x0104`, and `0x0105` values into the caller.
6. Record the AV thread and the runtime code window of any stack-based direct-call target.
7. Unwind one saved EBP frame at the AV and record caller `[EBP-8]`, adjacent locals, and the caller return address.
8. Record a runtime code window before and after the stack direct-call candidate to identify the caller-local construction.
9. Record the runtime code window at the caller's caller return site to continue tracing the first null producer.
10. Update the analysis and work log without guessing a response or bypassing original code.

## Exclusions

- Do not force the branch around `0x00434137`.
- Do not patch `EAX` or the output buffer to a non-null value.
- Do not promote shared `0x80` to a confirmed 4th physical I/O-board response.
- Do not change the product default execution policy.

## Minimum run

Use the PowerShell command shown in the Korean section above, preserving the user-provided CHD and staging HDD paths.

The `--device-mock-lptdi-ioctl-success` flag only satisfies the bounded-trace policy requirement. Configured Hardlock `0x450`/`0x44c` material and the transform map remain injected by the existing target configuration. If this mode changes the path, repeat the trace with `0x9c402450` as the filter.

## Completion criteria

- Compare trace and AV order, thread, and EAX values.
- Classify the result as directly related, indirectly related, or unresolved, with address-based evidence.
- Update `docs/analysis/ez2dj4th-hardlock-runtime.md` and the corresponding work log.
- Leave Windows x86 build evidence when code changes occur, or execution evidence when this remains a diagnostic-only task.

## Outcome

이번 작업은 완료되었습니다. AV는 `0x00434137`의 null receiver dereference로 귀속되었고, caller의 caller가 공급한 `+0x11c` 필드가 0인 상태까지 확인했습니다. Hardlock 응답 실패로 단정하지 않으며, 해당 field writer/초기화 추적은 후속 작업으로 남깁니다.

This work order is complete. The AV is attributed to a null receiver at `0x00434137`, and the caller's caller is confirmed to supply a `+0x11c` field whose value is zero. The result is not classified as a Hardlock response failure; tracing the field writer/initializer remains follow-up work.
