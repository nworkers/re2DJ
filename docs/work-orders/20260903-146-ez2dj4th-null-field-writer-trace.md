# ez2dj4th null field 최초 writer 추적 작업 지시서

관련 설계: [ez2dj4th null field 최초 writer 추적 설계](../design/20260903-146-ez2dj4th-null-field-writer-trace.md)

## 목표

Task 145에서 확인한 `0x00acd708 + 0x11c` null field를 쓰는 원본 runtime instruction과 최초 writer 값을 확인하고, raw I/O와의 직접·간접 연관성을 분리합니다.

## 작업

1. field 주소를 `image_base + 0x006cd824`로 계산하는 `ez2dj4th` 전용 diagnostic option을 추가합니다.
2. x86 `DR3`에 4-byte write hardware breakpoint를 설치합니다.
3. primary thread 및 이후 생성 thread에 breakpoint를 적용합니다.
4. data breakpoint hit에서 thread, post-store EIP, 주변 runtime bytes, EAX, ECX, DR6, field 값을 기록합니다.
5. 기존 `--slot-writer-trace`와 동시 사용해도 DR0–DR2와 DR3가 충돌하지 않도록 합니다.
6. hit event와 bounded trace boundary를 기록하고 실제 CHD로 실행합니다.
7. field writer hit와 AV 순서, thread, field 값, 앞선 raw-I/O 이벤트를 비교합니다.
8. 임의의 field 값 주입, branch bypass, Hardlock 응답 추정은 하지 않습니다.
9. 분석 문서와 작업 로그를 갱신합니다.

## 제외 범위

- `0x00acd824`에 non-zero 값을 직접 기록하지 않습니다.
- `0x00434137` 주변 branch를 우회하지 않습니다.
- shared `0x80` raw-I/O 값을 4th 물리 보드 응답으로 확정하지 않습니다.
- 제품 기본 실행 정책을 변경하지 않습니다.

## 최소 실행 절차

```powershell
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe --hdd C:\Users\nworkers\AppData\Local\Temp\re2dj\chd\ez2dj4th --chd E:\MYWORK\Projects\re2DJ\roms\ez2dj4th\4thTrax.chd --target ez2dj4th --target-executable EZ2DJ\EZ2DJ.EXE --hle-io-ports --device-mock-lptdi --device-mock-wts-console-session --null-context-field-writer-trace
```

## 완료 조건

- Windows x86 build가 warning/error 없이 완료됩니다.
- 실제 CHD run에서 field writer ready/hit/boundary event를 확인합니다.
- writer 확인, 관찰 범위 내 null 유지, 또는 간접 연관 미확정 중 하나로 판정합니다.
- `docs/analysis/ez2dj4th-hardlock-runtime.md`와 대응 work log를 갱신합니다.

## 결과

작업을 완료했습니다. 실제 CHD 두 번의 실행에서 `DR3` field writer watch는 정상 준비됐지만 hit는 0회였고, field는 ready 시점부터 `0x00000000`으로 유지됐습니다. 고정 absolute reference scan도 `matches=0`이었습니다. 기존 slot-writer trace와의 공존은 정상 확인했습니다. field read/access 경로는 후속 작업으로 진행합니다.

---

# ez2dj4th Null-Field First-Writer Trace Work Order

Related design: [ez2dj4th Null-Field First-Writer Trace Design](../design/20260903-146-ez2dj4th-null-field-writer-trace.md)

## Objective

Identify the original runtime instruction and first value that writes the null field `0x00acd708 + 0x11c` confirmed by Task 145, and separate direct or indirect relation to raw I/O.

## Tasks

1. Add an `ez2dj4th` diagnostic option that computes the field as `image_base + 0x006cd824`.
2. Install a four-byte write hardware breakpoint in x86 `DR3`.
3. Apply the breakpoint to the primary and subsequently created threads.
4. Record thread, post-store EIP, nearby runtime bytes, EAX, ECX, DR6, and field values at each data-breakpoint hit.
5. Ensure it can coexist with `--slot-writer-trace` without colliding with DR0–DR2.
6. Record hit and bounded-trace boundary events and run against the real CHD.
7. Compare field-writer hits with AV order, thread, field value, and preceding raw-I/O events.
8. Do not inject an arbitrary field value, bypass a branch, or guess a Hardlock response.
9. Update the analysis and work log.

## Exclusions

- Do not write a non-zero value directly to `0x00acd824`.
- Do not bypass the branch around `0x00434137`.
- Do not promote the shared `0x80` raw-I/O value to a confirmed physical 4th-board response.
- Do not change product default execution policy.

## Minimum run

Use the PowerShell command in the Korean section above.

## Completion criteria

- The Windows x86 build completes without warnings or errors.
- A real CHD run produces field-writer ready/hit/boundary evidence.
- Classify the result as writer confirmed, field remaining null within the observed scope, or indirect relation unresolved.
- Update `docs/analysis/ez2dj4th-hardlock-runtime.md` and the corresponding work log.

## Outcome

This work order is complete. In two real-CHD runs the `DR3` field-writer watch was prepared successfully but produced zero hits, and the field remained `0x00000000` from the ready event through the AV. The fixed absolute reference scan also reported `matches=0`. Coexistence with the existing slot-writer trace was verified. The field read/access path remains follow-up work.
