# 게이트 이후 복귀 추적 작업 로그

관련 작업 지시: [게이트 이후 복귀 추적 작업 지시](../work-orders/20260823-045-post-gate-resume-trace.md)  
관련 설계: [게이트 이후 복귀 추적](../design/20260823-045-post-gate-resume-trace.md)

## 구현 결과

언로드 종반 수집기에 두 가지를 추가했다.

1. **스텁 복귀 breakpoint 설치.** 샘플이 `ff d2`(`call edx`)로 시작하고 직전 샘플이 주소 − 5에서 `ba`(`mov edx, imm32`)로 시작하면 표준 syscall 스탠자 꼬리로 인정해, 수집당 최초 1회 복귀 주소(샘플 주소 + 2)에 `SetSoftwareEntryBreakpoint`로 INT3를 설치한다. 설치 결과는 `resume_breakpoint_arm` event로 남긴다.
2. **복귀 hit 처리.** `EXCEPTION_BREAKPOINT`에서 watched API 조회보다 먼저 복귀 주소 일치를 검사한다. 일치하면 GP 레지스터와 stack 상단 4 word를 `syscall_resume_hit` event로 기록하고, 원래 byte 복원·EIP 복귀·TF 재무장 뒤 `DBG_CONTINUE`로 재개한다. hit은 1회만 처리한다.

DR0 하드웨어 방식 대신 software breakpoint를 채택했다. 기존 swallow-and-rearm 기계를 재사용하고, wow64 디버그 레지스터 마샬링 불확실성을 피하며, child 전용 copy-on-write 페이지만 건드린다.

## 검증 결과

Windows x86 Debug build 성공, CTest 2/2 통과. canonical 실행(logs/…/20260823-034711-677.jsonl, 6,328 step)에서 다음을 확인했다.

### 확인됨: 복귀 추적이 동작한다

```text
{"event":"resume_breakpoint_arm","stub":"0x77109b9a","return":"0x77109b9c"}
{"event":"syscall_resume_hit","address":"0x77109b9c","eax":"0x00000000",
 "ebx":"0x73190000","ecx":"0x77081f83","edx":"0x00000000","esi":"0x771c71e0",
 "edi":"0x73190000","ebp":"0x001aff18","esp":"0x001afef4",
 "stack":["0x770fdf4a","0x00000098","0x00000000","0x02067b28"]}
```

64비트 처리 후 정확히 복귀 주소에서 hit했고, TF 재무장으로 이후 약 380명령을 더 수집했다(총 step 5,934 → 6,328).

### 확인됨: ZwSetEvent는 성공으로 돌아오고 죽음은 힙 파괴 안에 있다

복귀 직후 경로는 다음과 같다.

```text
ZwSetEvent 복귀(EAX=0, EBX=EDI=0x73190000 wsock32 base 보존)
→ ntdll 내부 epilogue (mov [edx],eax; pop esi; ret)
→ RtlAcquireSRWLockExclusive
→ RtlDestroyHeap+0x357..0x3e2
   push eax / push [ebp-4] / lea+push &[ebp-8] / lea+push &[ebp-c] / push -1
→ call → ntdll.dll!ZwProtectVirtualMemory+0x0 (eax=0x50)
→ mov edx,&thunk; call edx → jmp [ntdll data]
→ gate jmp far 0x33: → 도착 직후 0x0027c004 illegal instruction
```

`NtProtectVirtualMemory(ProcessHandle=-1, BaseAddress*, RegionSize*, NewProtect, OldProtect*)` 다섯 인자와 정확히 일치하는 스택 구성이므로, `RtlDestroyHeap`이 파괴 중인 힙 region의 페이지 속성을 바꾸려다가 두 번째 게이트를 통과했고 그 직후 private RW page fetch가 실패했다. fault page의 allocation base `0x00200000` 예약이 이 힙과 겹친다는 것이 가장 강한 상관이다(추정).

### 해석

종료 경로는 이제 `FreeLibrary("WSOCK32.DLL") → 언로드 종반 detach 처리 → ZwSetEvent(성공) → SRW lock → RtlDestroyHeap → ZwProtectVirtualMemory → 게이트 #2 → #UD`로 확정됐다. 죽음의 주체는 보호 코드가 아니라 언로드 종반의 시스템 힙 정리다. 보호 stub이 자신이 만든 탐색용 FreeLibrary 하나로 이 경로에 들어섰다. 왜 두 번째 게이트의 64비트 처리가 private RW page 전송을 만드는지(힙 메타데이터 변조 소비인지 현대 환경 부정합인지)는 여전히 미확정이다.

## 결론 및 다음 단계

TF가 게이트에서 소멸해도 복귀 주소 software breakpoint로 관찰을 이어붙일 수 있음을 검증했다. 다음 증거 후보: `syscall_resume_hit`에서 stack 64 word dump로 `RtlDestroyHeap`의 소유(ws2_32 detach 여부) 확인, 힙 핸들–fault allocation 상관, fault 시점 memory에서 `{entry VA ×4}` 식재 패턴 탐색. HLE 관점에서는 stub의 `FreeLibrary` 후킹이 이 종료 자체를 피할 수 있는지 검토 가치가 있다.

---

# Post-Gate Resume Trace Work Log

Related work order: [Post-Gate Resume Trace Work Order](../work-orders/20260823-045-post-gate-resume-trace.md)  
Related design: [Post-Gate Resume Trace](../design/20260823-045-post-gate-resume-trace.md)

## Implementation result

The unload-tail collector now plants a one-shot software breakpoint on a detected syscall-stub return address (`ff d2` preceded five bytes earlier by `ba`, once per collection) and handles the matching `EXCEPTION_BREAKPOINT` before watched-API lookup: record registers and top stack words as `syscall_resume_hit`, restore the byte, rewind EIP, re-arm TF, and continue. Software breakpoints were chosen over DR0 to reuse the proven swallow-and-rearm machinery and avoid wow64 debug-register marshaling uncertainty.

## Verification result

Build succeeded, CTest passed 2/2, and the canonical run confirmed:

* The arm event fired at stub `0x77109b9a` with return `0x77109b9c`, and the resume hit landed exactly there after the 64-bit processing; re-tracing captured roughly 380 more instructions (total steps 5,934 → 6,328).
* `ZwSetEvent` returned success (EAX=0) with wsock32's image base preserved in EBX/EDI.
* The post-return path runs an internal store-result epilogue → `RtlAcquireSRWLockExclusive` → `RtlDestroyHeap` (+0x357..0x3e2 visible) → a helper pushing `-1, &base, &size, protect, &old` → `ZwProtectVirtualMemory` (eax=0x50) → thunk → second gate → immediate #UD at `0x0027c004`.
* The death therefore sits inside unload-tail heap destruction plus page-protection work, not inside ZwSetEvent; the fault page's reservation overlapping the destroyed heap is the strongest correlation (inferred).

## Conclusion and next steps

Observation now bridges the WOW64 gate via a return-address software breakpoint. Next evidence candidates: a 64-word stack dump at the resume hit to establish whether RtlDestroyHeap belongs to ws2_32 detach, correlating the heap handle with the fault allocation, searching fault-time memory for planted `{entry VA ×4}` patterns behind the stable register signature, and evaluating from the HLE perspective whether hooking the stub's FreeLibrary would avoid this modern-environment termination altogether.
