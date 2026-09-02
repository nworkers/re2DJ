# ez2dj4th null field access 추적 작업 지시서

관련 설계: [ez2dj4th null field access 추적 설계](../design/20260903-147-ez2dj4th-null-field-access-trace.md)

## 목표

`0x00acd824` field의 최초 read/write access와 post-access runtime context를 확인해 `0x00434137` AV 직전의 null 전달 경로를 더 좁힙니다.

## 작업

1. `image_base + 0x006cd824`에 대한 4-byte read/write hardware breakpoint option을 추가합니다.
2. primary 및 생성 thread에 DR3 watch를 적용합니다.
3. 기존 `--slot-writer-trace`와 공존시키되, Task 146의 writer-only option과는 동시 사용을 거부합니다.
4. access hit에서 EIP, registers, DR6, field 값, code window, stack return address를 기록합니다.
5. 실제 CHD를 실행하고 최초 hit와 AV의 순서·thread·값을 비교합니다.
6. read site 확인, 초기 read 확인, 또는 미확정 중 하나로 판정합니다.
7. 원본 code, field, guest return value, branch, IAT, Hardlock 응답값을 수정하지 않습니다.
8. 분석 문서와 작업 로그를 갱신합니다.

## 제외 범위

- `0x00acd824`에 값을 직접 기록하지 않습니다.
- `0x00434137` branch를 우회하지 않습니다.
- shared raw-I/O 값을 4th 물리 보드 응답으로 확정하지 않습니다.
- 제품 기본 실행 정책을 변경하지 않습니다.

## 최소 실행 절차

```powershell
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe --hdd C:\Users\nworkers\AppData\Local\Temp\re2dj\chd\ez2dj4th --chd E:\MYWORK\Projects\re2DJ\roms\ez2dj4th\4thTrax.chd --target ez2dj4th --target-executable EZ2DJ\EZ2DJ.EXE --hle-io-ports --device-mock-lptdi --device-mock-wts-console-session --null-context-field-access-trace
```

## 완료 조건

- Windows x86 build가 warning/error 없이 완료됩니다.
- 실제 CHD run에서 access watch ready/hit/boundary event를 확인합니다.
- 최초 field access와 AV의 관계를 주소·thread·값으로 판정합니다.
- `docs/analysis/ez2dj4th-hardlock-runtime.md`와 대응 work log를 갱신합니다.

## 결과

작업을 완료했습니다. 첫 access hit는 `0x0041a69f` 직후 발생했고 field 값은 0, `ECX=0`이었습니다. 같은 thread에서 곧바로 `0x00434137` AV가 발생했으며, 기존 slot-writer trace와의 DR0–DR3 공존도 확인했습니다.

---

# ez2dj4th Null-Field Access Trace Work Order

Related design: [ez2dj4th Null-Field Access Trace Design](../design/20260903-147-ez2dj4th-null-field-access-trace.md)

## Objective

Identify the first read/write access and post-access runtime context for field `0x00acd824`, narrowing the null propagation path immediately before the `0x00434137` AV.

## Tasks

1. Add a four-byte read/write hardware-breakpoint option for `image_base + 0x006cd824`.
2. Apply the DR3 watch to the primary and created threads.
3. Keep it compatible with `--slot-writer-trace`, while rejecting simultaneous use with the Task 146 writer-only option.
4. Record EIP, registers, DR6, field value, code window, and stack return address at each access hit.
5. Run against the real CHD and compare the first hit with the AV by order, thread, and values.
6. Classify the result as read site confirmed, initial read confirmed, or unresolved.
7. Do not modify original code, the field, guest return values, branches, IAT, or Hardlock response values.
8. Update the analysis and work log.

## Exclusions

- Do not write a value directly to `0x00acd824`.
- Do not bypass the branch around `0x00434137`.
- Do not promote shared raw-I/O values to confirmed physical 4th-board responses.
- Do not change product default execution policy.

## Minimum run

Use the PowerShell command in the Korean section above.

## Completion criteria

- The Windows x86 build completes without warnings or errors.
- A real CHD run produces access-watch ready/hit/boundary evidence.
- Classify the first field access and its relation to the AV using addresses, thread, and values.
- Update `docs/analysis/ez2dj4th-hardlock-runtime.md` and the corresponding work log.

## Outcome

This work order is complete. The first access hit occurred immediately after `0x0041a69f` with field value zero and `ECX=0`. The same thread then produced the `0x00434137` AV, and coexistence of the existing slot-writer DR0–DR2 watches with access DR3 was verified.
