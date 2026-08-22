# 보호 종료 경로 정밀 관찰 작업 지시

관련 설계: [보호 종료 경로 정밀 관찰](../design/20260823-044-protected-fault-path-precision.md)

## 목표

protected `ez2dj.exe`의 illegal-instruction 직전 상태를 32비트 디버거 범위에서 최대한 좁힌다. fault 시점 세그먼트, 언로드 종반 샘플별 레지스터 트레일, 샘플 심볼 주석을 JSONL에 추가한다.

## 작업

1. `RecordIllegalInstructionContext`의 register 기록에 SegCs·SegDs·SegEs·SegFs·SegGs·SegSs를 추가한다.
2. 언로드 종반 수집기가 매 샘플마다 EAX~EDI·EBP·ESP를 기록하게 확장한다. `InstructionSample`에 레지스터와 심볼 필드를 추가한다.
3. `remote_module_exports`에 `FindRemotePe32NearestExport`를 추가한다. export directory를 해석해 주소 이하 가장 가까운 non-forwarded export 이름과 오프셋, 모듈 이름을 돌려준다.
4. 수집기는 VirtualQueryEx 결과가 MEM_IMAGE면 AllocationBase에서 nearest-export를 해석해 샘플에 `"symbol"`로 붙인다.
5. Windows x86 build·CTest·canonical 실행으로 검증하고 결과를 분석 문서와 작업 로그에 반영한다.

---

# Protected Termination Path Precision Observation Work Order

Related design: [Protected Termination Path Precision Observation](../design/20260823-044-protected-fault-path-precision.md)

## Goal

Narrow the state immediately before the protected `ez2dj.exe` illegal instruction within 32-bit debugger reach: segments at the fault, a per-sample register trail in the unload-tail collection, and symbol annotations on samples.

## Tasks

1. Add SegCs/Ds/Es/Fs/Gs/Ss to the register record in `RecordIllegalInstructionContext`.
2. Extend the unload-tail collector to record EAX–EDI plus EBP and ESP for every sample; add register and symbol fields to `InstructionSample`.
3. Add `FindRemotePe32NearestExport` to `remote_module_exports`, resolving the nearest non-forwarded export at or below an address together with its offset and the export-directory module name.
4. When VirtualQueryEx reports MEM_IMAGE, resolve nearest exports from the region's AllocationBase and attach them as `"symbol"` on samples.
5. Verify with the Windows x86 build, CTest, and a canonical run; reflect the results in the analysis documents and work log.
