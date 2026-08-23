# 종료 귀속과 식재 패턴 추적 작업 로그

관련 작업 지시: [종료 귀속과 식재 패턴 추적 작업 지시](../work-orders/20260823-046-teardown-attribution.md)  
관련 설계: [종료 귀속과 식재 패턴 추적](../design/20260823-046-teardown-attribution.md)

## 구현 결과

1. `syscall_resume_hit`에서 stack 상단 64 word를 기록하고(`resume_stack`), MEM_IMAGE 범위 word마다 nearest-export 심볼을 붙여 `resume_stack_symbol`로 남긴다(최대 24개, 샘플 주석과 캐시 공유). 심볼 포맷을 `format_remote_symbol` 람다로 추출해 두 경로가 재사용한다.
2. first-chance illegal-instruction에서 committed private/image 메모리를 64 KB 블록 순회로 훑어 entry VA dword 참조를 찾고, 앞뒤 dword와 연속 실행 여부를 `fault_entry_reference`로 남긴다(상한 48, `fault_entry_summary` 요약). `ScanFaultReferences`와 같은 구조로 다른 일치 조건이다.
3. 검증 중 확장: 복귀 breakpoint를 재무장 가능하게 바꿨다. 소비될 때마다 감지기가 다시 활성화되어 이후 syscall 스탠자 꼬리에도 복귀 breakpoint가 설치되며, 화재 예산(`kMaxResumeBreakpointFires = 8`)으로 상한을 둔다.

작업 지시 밖의 확장(3번)은 두 번째 WOW64 게이트 너머를 포착하기 위한 것이며, 그 결과가 곧 본 단위의 핵심 발견으로 이어졌다.

## 검증 결과

Windows x86 Debug build 성공, CTest 2/2 통과. canonical 실행 두 번(logs/…/114546-290.jsonl, …/115009-800.jsonl)에서 다음을 확인했다.

### 확인됨: 복귀 시점 stack에 ws2_32/wsock32 프레임이 없다

```text
index 0  ntdll 내부 복귀      index 4  ntdll!LdrUnloadDll+0x15d
index 10 KERNELBASE!FreeLibrary+0x16
index 33/35 kernel32 스레드 진입점 잔재
```

ZwSetEvent·힙 파괴 구간은 ws2_32 detach 코드가 아니라 **LdrUnloadDll 자체의 마무리 처리** 안에서 실행된다.

### 확인됨: fault 서명 값은 스텁이 심은 stack 블록이다

entry 참조 탐색: 20 match, 15 run. 대표 블록 `0x001aff08~`: `{LdrUnloadDll+0x166, entry ×5, page base}` — fault 레지스터 배치(ECX=EDX=ESI=EDI=entry, EBX=page base)와 동일하다. raw resume stack에도 `{entry ×6, page base ×5, wsock32 base, 0x001affcc, FreeLibrary caller 0x01ed25c9}` 잔재가 그대로 보였다.

### 확인됨: 최종 전송은 게스트 코드 자신이 수행한다

복귀 breakpoint를 4회 모두 포착(ZwSetEvent, ZwProtectVirtualMemory ×2, 미식별 ntdll 스텁 — 전부 EAX=0 성공 복귀)하자 이번 실행은 teardown을 통과해 `.gtide`로 돌아왔고(100,618 step), 마지막 구간은 다음과 같다.

```text
0x01ed2730  pop eax            ; eax=0x001affcc
0x01ed2731  pop ebx            ; ebx=0x002b1000 (page base)
0x01ed2732  pop ecx            ; ecx=entry
0x01ed2733  pop edx            ; edx=entry
0x01ed2734  pop edi            ; edi=entry
0x01ed2735  pop esi            ; esi=entry
0x01ed2736  leave              ; ebp←kernel32 주소
0x01ed2737  jmp dword [0x01ed7010]   ; .gdata 포인터 → 0x01ed3806
0x01ed3806~ cmp dword [0x01ed7074], 0 …
0x01ed3833  ret                ; → 0x002b1000 private RW page
0x002b1000  add [eax],al (우연 실행) → 0x002b1004 ff ff → 0xC000001D
```

식재 블록을 레지스터에 복원하는 것부터 점프까지 전부 보호 스텁 자신의 코드다.

## 결론

종료는 우연한 손상이 아니라 **보호 스텁의 계획된 복귀 경로**다. 스텁은 자신의 continuation buffer(힙 페이지)로 점프하지만, 이 호스트에서는 버퍼가 복호화되지 않아 힙 메타데이터만 남아 있었고 DEP 부재 환경에서 가비지 명령 2개를 실행한 뒤 죽는다. 선행 원인으로 `\\.\LPTDI1` 병렬포트 프로브 실패가 가장 유력하다(추정). debugger perturbation에 따라 같은 안무가 teardown 중 시스템 코드 사망 변주로 나타나기도 했다.

---

# Teardown Attribution and Planted-Pattern Tracing Work Log

Related work order: [Teardown Attribution and Planted-Pattern Tracing Work Order](../work-orders/20260823-046-teardown-attribution.md)  
Related design: [Teardown Attribution and Planted-Pattern Tracing](../design/20260823-046-teardown-attribution.md)

## Implementation result

Added a 64-word symbol-annotated dump at `syscall_resume_hit` (`resume_stack` plus up to 24 `resume_stack_symbol` events, sharing the sample annotation cache through an extracted `format_remote_symbol` lambda), and a bounded block walk matching the entry VA dword with neighbor context (`fault_entry_reference`, capped at 48 with a summary). During verification the resume breakpoint became rearmable — each consumed hit re-enables detection for later syscall stanza tails, bounded by a fire budget of eight.

Extension three sits outside the original work-order text; it was added to catch the second WOW64 gate and directly produced this unit's central discovery.

## Verification result

Build succeeded, CTest passed 2/2. Two canonical runs confirmed:

* The resume-time stack holds only `KERNELBASE!FreeLibrary+0x16` and `ntdll!LdrUnloadDll+0x15d` frames beyond ntdll internals — the event-signal and heap-destroy stretch executes inside LdrUnloadDll's own finalization, not ws2_32 detach code.
* The entry scan found 20 references with 15 in runs, including the planted block `{LdrUnloadDll+0x166, entry×5, page-base}` at `0x001aff08`, exactly matching the register signature layout.
* With all four resume breakpoints caught (ZwSetEvent, ZwProtectVirtualMemory twice, one unidentified ntdll stub — each returning success), the run survived teardown back into `.gtide` (100,618 steps): `pop eax/ebx/ecx/edx/edi/esi; leave` at `0x01ed2730` restores precisely the run-invariant signature from the planted block, `jmp dword [0x01ed7010]` reaches `0x01ed3806` through the .gdata pointer, and `ret` at `0x01ed3833` jumps onto the private RW page, where two accidental instructions execute before `ff ff` raises #UD.

## Conclusion

The termination is the protection's own planned return path, not accidental damage: the stub restores registers from a planted stack block and jumps to its continuation buffer, which on this host still holds heap metadata instead of decrypted code. The most plausible upstream cause is the failed `\\.\LPTDI1` parallel-port probe (inferred). Debugger perturbation sometimes rerouted the same choreography into mid-teardown system-code deaths across runs.
