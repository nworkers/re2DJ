# ez2dj4th 상위 객체 allocation 추적 작업 로그

## 결과

구현과 실제 CHD 검증을 완료했습니다. `--null-context-allocation-trace`는
`LocalAlloc`과 `VirtualAlloc`의 caller-return 경계를 추적하고,
`--null-context-field-access-trace`는 `[EBP-0x118]`의 객체 주소와 객체의
`+0x11c` field를 함께 기록합니다.

주 실행 로그는
`logs/windows_x86_launcher_probe/ez2dj4th/20260903-023341-871.jsonl`입니다.
첫 field access hit에서 다음을 확인했습니다.

- 객체 주소: `0x00acd708`
- 객체 field: `0x00acd824`, 값 `0x00000000`
- read site: `0x0041a699` (`mov ECX, [ECX+0x11c]`), post-access EIP `0x0041a69f`
- `allocation_return_match`: `false`
- 이후 원본 실행 파일은 기존과 같은 `0x00434137` null-receiver AV에 도달

관찰 범위에서 `LocalAlloc`·`VirtualAlloc` 반환 hit는 34,537건이었고,
상세 return 이벤트 기록은 256건으로 제한되었습니다. 반환값 비교용 주소
집합은 상세 로그 cap과 별도로 계속 갱신되어 field hit와 비교했습니다.
`HeapAlloc`은 실행 중 export가 forwarded 상태여서 watch가 설치되지 않았고,
따라서 해당 allocator에 대해서는 결론을 내리지 않았습니다.

초기 구현에서는 `kernel32`와 `kernelbase`의 동일 allocator alias 및 한
스레드의 중첩 `LocalAlloc`을 오류로 처리했습니다. 검증 중 이를 확인하고
kernelbase 우선 단일 watch, 스레드별 pending stack, 공유 return breakpoint
카운트와 single-step 재무장으로 수정했습니다.

## 변경 사항

- Windows x86 launcher probe에 `--null-context-allocation-trace` 추가
- allocator API entry/return trace와 bounded diagnostic events 추가
- 기존 field access event에 object/field/stack-return context 추가
- allocator trace에서 불필요한 전체 system API watch를 설치하지 않도록 제한
- `HeapAlloc` forwarded export 및 no-match를 unresolved 범위로 기록
- guest memory, original executable bytes, branch, return value, IO 응답은 수정하지 않음

## 검증

- Windows x86 build: 통과
- 단위 테스트: `checks: 1184, failures: 0`
- 실제 4thTrax CHD + staging HDD read-only run: launcher handoff와 trace boundary 기록
- `--slot-writer-trace`를 추가한 별도 실행에서도 기존 slot writer hit가 기록되어
  allocator trace와 debug-register watch가 충돌하지 않음을 확인

## 다음 단계

`0x00acd708`은 관찰 allocator 반환값이 아니며 image-resident 주소입니다.
다음 작업은 allocator를 더 추측하는 것이 아니라, 실행 중 정적 객체 주소가
`[EBP-0x118]`에 공급되는 stack-local assignment 또는 그 caller를 추적하는
것입니다.

---

# ez2dj4th Upper-Object Allocation Trace Work Log

## Result

Implementation and real-CHD verification are complete. The new
`--null-context-allocation-trace` follows caller-return boundaries for
`LocalAlloc` and `VirtualAlloc`; `--null-context-field-access-trace` records
the object address at `[EBP-0x118]` and the object's `+0x11c` field.

The primary log is
`logs/windows_x86_launcher_probe/ez2dj4th/20260903-023341-871.jsonl`.
At the first field-access hit it recorded:

- object address `0x00acd708`
- object field `0x00acd824`, value `0x00000000`
- read site `0x0041a699` (`mov ECX, [ECX+0x11c]`), post-access EIP `0x0041a69f`
- `allocation_return_match=false`
- the original executable then reached the existing null-receiver AV at `0x00434137`

The observed `LocalAlloc` and `VirtualAlloc` return hits totaled 34,537; detailed
return events were bounded at 256. The address set used for comparison continued
to update independently of the detailed-event cap. `HeapAlloc` was not watched
because its available export was forwarded, so no conclusion is made for that
allocator.

The initial implementation exposed both allocator export aliases and same-thread
nested `LocalAlloc` calls as tracing errors. Verification found both cases; the
trace now uses a kernelbase-first single watch, per-thread pending stacks, shared
return-breakpoint counts, and single-step rearming.

## Changes

- Added `--null-context-allocation-trace` to the Windows x86 launcher probe.
- Added bounded allocator entry/return diagnostics.
- Added object, field, and stack-return context to field-access events.
- Limited allocation mode to allocator watches instead of all system API watches.
- Recorded forwarded `HeapAlloc` and no-match as unresolved scope.
- Did not modify guest memory, original executable bytes, branches, return values, or IO responses.

## Verification

- Windows x86 build: passed.
- Unit tests: `checks: 1184, failures: 0`.
- Real 4thTrax CHD plus staging-HDD read-only run: handoff and trace boundary recorded.
- A separate run with `--slot-writer-trace` also recorded the existing slot-writer
  hit, confirming coexistence with the allocator trace and debug-register watches.

## Next step

`0x00acd708` is image-resident and did not match the observed allocator returns.
The next diagnostic should trace the stack-local assignment or caller that supplies
this static object address to `[EBP-0x118]`, rather than guessing another allocator
or injecting a field value.
