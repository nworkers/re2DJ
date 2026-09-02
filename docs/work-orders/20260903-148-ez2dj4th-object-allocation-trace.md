# ez2dj4th 상위 객체 allocation 추적 작업 지시서

관련 설계: [ez2dj4th 상위 객체 allocation 추적 설계](../design/20260903-148-ez2dj4th-object-allocation-trace.md)

## 목표

`0x00acd708` 객체 주소가 `LocalAlloc`, `HeapAlloc`, `VirtualAlloc` 중 어느
allocator 반환과 일치하는지 확인하고, field read 시점의 stack/local 관계를
기록합니다.

## 상태

구현과 검증을 완료했습니다. `LocalAlloc`·`VirtualAlloc` 반환값은
`0x00acd708`과 일치하지 않았고, `HeapAlloc`은 forwarded export로 인해
추적 범위 밖에 남았습니다. 다음 작업은 allocator가 아니라
`[EBP-0x118]`에 image-resident 객체 주소를 공급하는 경계를 추적해야 합니다.

## 작업

1. `--null-context-allocation-trace` 옵션을 추가합니다.
2. allocator API entry에서 caller return address breakpoint를 설치합니다.
3. return hit에서 API·caller·인자·EAX·code window를 기록하고 API breakpoint를 재무장합니다.
4. field access hit에 `[EBP-0x118]`, 객체 `+0x11c`, stack return 정보를 추가합니다.
5. 기존 `--null-context-field-access-trace` 및 `--slot-writer-trace`와 결합 실행합니다.
6. 반환 EAX와 객체 주소의 일치 여부를 allocator-origin candidate, pre-existing/other origin, unresolved 중 하나로 판정합니다.
7. guest memory, 원본 code, branch, return value, IO 응답은 수정하지 않습니다.
8. 분석 문서와 작업 로그를 갱신합니다.

## 제외 범위

- field 또는 객체에 직접 값을 쓰지 않습니다.
- `0x00434137`을 우회하지 않습니다.
- `ez2dj1stse` IO를 4th의 객체 초기화 응답으로 확정하지 않습니다.
- 제품 기본 실행 정책을 변경하지 않습니다.

## 최소 실행 절차

```powershell
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe --hdd C:\Users\nworkers\AppData\Local\Temp\re2dj\chd\ez2dj4th --chd E:\MYWORK\Projects\re2DJ\roms\ez2dj4th\4thTrax.chd --target ez2dj4th --target-executable EZ2DJ\EZ2DJ.EXE --hle-io-ports --device-mock-lptdi --device-mock-wts-console-session --null-context-field-access-trace --null-context-allocation-trace
```

## 완료 조건

- 설계·작업지시서가 구현 전에 작성됩니다.
- Windows x86 build와 단위 테스트가 통과합니다.
- 실제 CHD 실행에서 allocator trace ready/return/boundary 및 field object context가 기록됩니다.
- allocator 반환값과 `0x00acd708`의 관계를 주소·thread·순서로 판정합니다.
- 분석 문서와 대응 work log를 갱신합니다.

---

# ez2dj4th Upper-Object Allocation Trace Work Order

Related design: [ez2dj4th Upper-Object Allocation Trace Design](../design/20260903-148-ez2dj4th-object-allocation-trace.md)

## Objective

Determine whether object address `0x00acd708` matches a return from
`LocalAlloc`, `HeapAlloc`, or `VirtualAlloc`, and record its stack/local context
at the field read.

## Status

Implementation and verification are complete. No observed `LocalAlloc` or
`VirtualAlloc` return matched `0x00acd708`; `HeapAlloc` remains outside the
trace scope because its exports were forwarded. The next task should trace the
boundary that supplies the image-resident object address to `[EBP-0x118]`.

## Tasks

1. Add the `--null-context-allocation-trace` option.
2. Install a caller-return breakpoint at allocator API entry.
3. Record API, caller, arguments, EAX, and a code window at return, then rearm the API breakpoint.
4. Add `[EBP-0x118]`, object `+0x11c`, and stack-return data to field-access hits.
5. Run it together with `--null-context-field-access-trace` and `--slot-writer-trace`.
6. Classify the EAX/object-address relationship as allocator-origin candidate, pre-existing/other origin, or unresolved.
7. Do not modify guest memory, original code, branches, return values, or I/O responses.
8. Update the analysis and work-log documents.

## Exclusions

- Do not write directly to the field or object.
- Do not bypass `0x00434137`.
- Do not classify the reused `ez2dj1stse` I/O as a 4th object-initialization response.
- Do not change product default execution policy.

## Minimum run

Use the PowerShell command in the Korean section above.

## Completion criteria

- Write the design and work order before implementation.
- Pass the Windows x86 build and unit tests.
- Record allocator trace ready/return/boundary events and field object context in a real-CHD run.
- Classify the relation between allocator returns and `0x00acd708` by address, thread, and order.
- Update the analysis and corresponding work log.
