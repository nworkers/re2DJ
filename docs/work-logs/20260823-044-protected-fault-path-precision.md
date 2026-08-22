# 보호 종료 경로 정밀 관찰 작업 로그

관련 작업 지시: [보호 종료 경로 정밀 관찰 작업 지시](../work-orders/20260823-044-protected-fault-path-precision.md)  
관련 설계: [보호 종료 경로 정밀 관찰](../design/20260823-044-protected-fault-path-precision.md)

## 구현 결과

1. `remote_module_exports`에 `FindRemotePe32NearestExport`를 추가했다. export directory 해석을 `ReadExportDirectoryInfo`와 `ExportNameReader`로 공유화했고, non-forwarded export 중 주소 이하에서 가장 가까운 이름과 오프셋, directory의 모듈 이름을 돌려준다.
2. 언로드 종반 수집기가 매 샘플마다 GP 레지스터(EAX~EDI, EBP, ESP)를 기록하고 TF 재무장을 같은 thread 왕복으로 처리한다. `InstructionSample`에 `regs`, `has_regs`, `symbol` 필드가 생겼다.
3. 수집기는 VirtualQueryEx 결과가 MEM_IMAGE면 AllocationBase 기준 nearest-export를 해석해 `"symbol":"모듈!함수+0x오프셋"` 형식으로 붙이고, (module, address) 단위 캐시로 반복 조회를 피한다.
4. `fault_registers`에 SegCs·SegDs·SegEs·SegFs·SegGs·SegSs를 추가했다.

64비트 컨텍스트 직접 포획은 설계 단계에서 배제했다. SDK `winnt.h`의 `WOW64_CONTEXT`는 `WOW64_CONTEXT_i386` 플래그만 정의하므로 32비트 프로세스가 문서화된 API로 WOW64 스레드의 진짜 64비트 컨텍스트를 받을 수 없다.

## 검증 결과

Windows x86 Debug build 성공, CTest 2/2 통과. canonical `ez2dj.exe` `--api-trace` 실행(logs/…/20260823-033106-536.jsonl, 5,934 step)에서 다음을 확인했다.

### 확인됨: 전환 직전 시스템 콜은 ntdll!ZwSetEvent다

샘플 심볼 해석에서 스텁 주소 `0x77109b90`이 `ntdll.dll!ZwSetEvent+0x0`과 정확히 일치한다. 바이트 `b8 0e 00 07 00 / ba c0 55 14 77 / ff d2 / c2 08 00`는 표준 syscall 스텁이고 `ret 8`은 NtSetEvent의 두 인자와 맞는다. 호출부(`RtlGetAppContainerNamedObjectPath+0xbd` 부근의 내부 함수 — nearest-export 추정 위치)는 `push 0; push dword [0x771c71b8]`(ntdll 전역 이벤트 핸들) 후 스텁을 부른다. 즉 win32k 서비스가 아니라 **NtSetEvent**였고, 언로드 종반에 ntdll 소유 이벤트 하나를 시그널하는 동작이다.

### 확인됨: fault 시점 보고 컨텍스트는 32비트 세그먼트다

```text
cs=0x0023 ds=0x002b es=0x002b fs=0x0053 gs=0x002b ss=0x002b
```

WOW64 계층이 합성한 값이라 실제 CPU 모드와 다를 수 있지만, fault 보고 기준은 32비트 사용자 코드 세그먼트다.

### 확인됨: 게이트 통과 순간 single-step 보고가 끊긴다

마지막 두 샘플:

```text
0x77087000  ea 09 70 08 77 33 00 00 00   ebx=0x73190000 edi=0x73190000 esi=0x771c71e0 esp=0x001afef0
0x77087009  41 ff a7 f8 00 00 00         ebx=0x020f7f88 edi=0x020f7f88 esi=0x020f7b00 esp=0x001afe7c
→ 그 다음 event는 곧바로 0x00203004 illegal instruction
```

`jmp far`는 GPR을 바꾸지 못하므로, 도착 샘플에서 보고 레지스터가 뒤집힌 것은 WOW64 세이브 영역 기반 재합성 때문이다(추정). 그리고 그 다음 관찰이 fault라는 사실은 **TF가 64비트 전환을 살아남지 못해** 게이트 이후 한 명령도 포착되지 않았다는 뜻이다(확인됨).

### fault 서명은 불변

`eax=0x001affcc, ebx=0x00203000(fault page base), ecx=edx=esi=edi=0x01ed23cf(entry), ebp=kernel32 내부, esp=0x001aff80`. page 내용과 allocation 구조도 이전 실행과 동일하다.

## 결론

종료 경로는 `FreeLibrary → LdrUnloadDll 종반 → ZwSetEvent(전역 이벤트 시그널) → WOW64 전환 → (관찰 불가 구간) → 32비트 복귀 후 private RW page로 전송 → 0xC000001D`로 좁혔다. 최종 전송 명령은 TF가 게이트에서 죽기 때문에 software single-step으로 관찰할 수 없으며, 다음 단계는 `call edx` 패턴 관찰 시점에 DR0를 스텁 복귀 주소로 걸어 하드웨어 breakpoint로 32비트 복귀를 포착한 뒤 TF를 재무장하는 것이다. 이 종료가 동글·환경 검사 실패의 반응인지 현대 WOW64 부정합인지는 여전히 미확정이다.

---

# Protected Termination Path Precision Observation Work Log

Related work order: [Protected Termination Path Precision Observation Work Order](../work-orders/20260823-044-protected-fault-path-precision.md)  
Related design: [Protected Termination Path Precision Observation](../design/20260823-044-protected-fault-path-precision.md)

## Implementation result

Added `FindRemotePe32NearestExport` with shared export-directory parsing; the unload-tail collector now records a GP-register trail per sample (rearming TF in the same thread round trip), annotates MEM_IMAGE samples with nearest-export symbols via a per-(module,address) cache, and the fault register record includes all six segment registers. Direct 64-bit-context capture was rejected at design time because the SDK's `WOW64_CONTEXT` defines only i386 flags.

## Verification result

Build succeeded, CTest passed 2/2, and the canonical run (5,934 steps) confirmed:

* The pre-transition syscall is exactly `ntdll!ZwSetEvent` (symbol offset +0x0; stub pattern matches NtSetEvent's two arguments), called by an internal routine pushing NULL plus a global ntdll event handle.
* The fault-time synthesized context reports 32-bit user segments (`cs=0x0023`, `fs=0x0053`).
* Crossing the gate flips reported registers to save-area values and silently ends single-step reporting: no step fires between the gate arrival and the fault, so TF does not survive the transition.
* The fault signature is unchanged across runs.

## Conclusion

The termination path narrows to FreeLibrary → LdrUnloadDll tail → ZwSetEvent on a global event → WOW64 gate → unobservable stretch → transfer to a private RW page after returning toward 32-bit code → 0xC000001D. The final transferring instruction needs hardware-breakpoint assistance: plant DR0 on the syscall stub's return address when the call-edx pattern is observed, catch the 32-bit return, re-arm TF, and trace the final stretch. Whether this path is deliberate anti-tamper or modern-environment fallout remains unresolved.
