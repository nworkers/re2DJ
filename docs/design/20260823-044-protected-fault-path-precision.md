# 보호 종료 경로 정밀 관찰

관련 작업 지시: [보호 종료 경로 정밀 관찰 작업 지시](../work-orders/20260823-044-protected-fault-path-precision.md)

## 목적

protected `ez2dj.exe`의 illegal-instruction이 WOW64 win32k 시스템 콜 전환 직후에 발생한다는 것까지 확인됐다([작업 로그 20260823-042](../work-logs/20260823-042-protected-api-observation-trace.md)). 남은 질문은 두 가지다. 전송 직전의 연산 대상은 무엇이었는가, 그리고 전송을 유도한 시스템 코드는 어느 함수인가. 이 작업은 32비트 디버거 범위 안에서 가능한 가장 가까운 관찰을 추가한다.

## 설계 결정과 제약

**64비트 컨텍스트 직접 포획은 채택하지 않았다.** Windows SDK(`winnt.h`)의 `WOW64_CONTEXT`는 `WOW64_CONTEXT_i386` 플래그만 정의하며 AMD64 플래그가 없다. `Wow64GetThreadContext`는 WOW64 스레드의 **i386 형태** 컨텍스트를 주고받는 API이므로, 32비트 프로세스인 launcher probe로는 문서화된 API만으로 게스트 스레드의 진짜 64비트 컨텍스트(Rip, Rsp, CS)를 얻을 수 없다. 64비트 헬퍼 프로세스를 두는 방법은 이후 필요해졌을 때 검토한다.

대신 세 가지 관찰을 추가한다.

1. **fault 시점 세그먼트 덤프.** 기존 `fault_registers`에 SegCs·SegDs·SegEs·SegFs·SegGs·SegSs를 추가한다. WOW64 계층이 합성하는 32비트 컨텍스트가 fault를 어떤 모드로 보고하는지 확인한다(0x23 = 32비트 호환, 0x33 = 64비트).
2. **샘플별 레지스터 트레일.** 언로드 종반 single-step 수집에서 매 샘플마다 GP 레지스터(EAX~EDI, EBP, ESP)를 함께 기록한다. 마지막 전송 명령의 피연산자를 재구성할 수 있다. 예컨대 마지막 샘플 바이트가 `ff a7 f8000000`(`jmp dword [edi+0xf8]`, 32비트 해석)이라면 직전 EDI 값이 target 산출 근거가 된다.
3. **샘플 심볼 주석.** 새 헬퍼 `FindRemotePe32NearestExport`가 모듈 base와 주소를 받아 해당 주소 이하의 가장 가까운 export 이름과 오프셋, export directory의 모듈 이름을 돌려준다. 수집기는 VirtualQueryEx로 샘플이 MEM_IMAGE인지 보고 AllocationBase에서 해석해 `"symbol":"ntdll!Foo+0x12"` 형태로 기록한다. MEM_PRIVATE이면 심볼 없이 둔다.

## 해석 경계

세그먼트 값은 WOW64가 보고하는 값이지 CPU의 실제 CS가 아닐 수 있다. 레지스터 트레일은 single-step 예외 전달 시점의 값이다. 심볼은 nearest-export 추정으로 함수 시작의 일부 오프셋 위치일 뿐 실행 중인 함수를 증명하지 않는다. 결론은 확인된 관찰과 추정으로 나눠 기록한다.

## 검증

Windows x86 build·CTest 후 canonical `ez2dj.exe`를 `--api-trace`로 실행한다. 성공 기준: `fault_registers`에 세그먼트가 포함되고, 마지막 샘플들에 레지스터 트레일과 심볼 주석이 붙으며, syscall 스텁 호출자(`0x770fdf3d`류 주소)의 소속 함수가 식별된다.

---

# Protected Termination Path Precision Observation Work Order

Related design: [Protected Termination Path Precision Observation](../design/20260823-044-protected-fault-path-precision.md)

## Purpose

The protected `ez2dj.exe` illegal instruction follows the WOW64 win32k syscall transition (work log 20260823-042). Two questions remain: what operands fed the final transfer, and which system function induced it. Add the closest observations reachable from a 32-bit debugger.

## Design decision and constraint

Direct 64-bit-context capture is rejected: the SDK's `WOW64_CONTEXT` defines only `WOW64_CONTEXT_i386` flags, so `Wow64GetThreadContext` exchanges the i386-shaped context and a 32-bit probe cannot obtain the guest thread's true 64-bit context through documented APIs. Revisit a 64-bit helper process later if needed.

Instead add three observations:

1. Segment dump in `fault_registers` (SegCs/Ds/Es/Fs/Gs/Ss) to see which mode the synthesized 32-bit context reports (0x23 compatibility, 0x33 64-bit).
2. A per-sample GP-register trail (EAX–EDI, EBP, ESP) in the unload-tail collection so the final transfer's operands can be reconstructed (for example, prior EDI for a `jmp dword [edi+0xf8]` reading).
3. Symbol annotations: a new `FindRemotePe32NearestExport` helper resolves the nearest export at or below an address plus the export-directory module name; the collector annotates MEM_IMAGE samples as `ntdll!Foo+0x12` using the region's AllocationBase.

## Interpretation boundary

Segment values are what the WOW64 layer reports, not necessarily the live CS. Register trails hold at single-step delivery time. Symbols are nearest-export estimates and do not prove the running function. Record conclusions separately as confirmed observations and inferred candidates.

## Verification

Build, run CTest, then run canonical `ez2dj.exe` with `--api-trace`. Success criteria: segments present in `fault_registers`, register trails and symbol annotations on the final samples, and identification of the owning function behind the syscall-stub caller addresses.
