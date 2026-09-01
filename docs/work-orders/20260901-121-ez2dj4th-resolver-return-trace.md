# 작업 121 — ez2dj4th 동적 resolver 반환 ABI trace

상태: 구현 및 실제 CHD trace 검증 완료. 반환 주소는 확인되었지만 wrapper 호출은 미확정입니다.

## 한국어

관련 설계: [ez2dj4th 동적 resolver 반환 ABI trace 설계](../design/20260901-121-ez2dj4th-resolver-return-trace.md)

### 목표

4th 동적 resolver가 반환하는 HLE/native 함수 포인터 주소와 원본 caller
return address를 VFS log에 기록하여, resolver 선택과 protected stub의 실제
후속 호출을 구분할 관찰 증거를 추가합니다.

### 구현 항목

1. dynamic resolver log event에 반환 주소와 caller 주소를 추가합니다.
2. HLE, device mock, WTS observe, native fallback 경로를 동일한 형식으로
   기록합니다.
3. 기존 함수 포인터 반환값과 route semantics는 변경하지 않습니다.
4. 실제 CHD 결과를 analysis, TODO, architecture, work-log에 기록합니다.

### 제외 항목

* calling convention 변환 또는 반환 포인터 수정
* Hardlock 응답, seed solver, directory enumeration
* VFS path/CHD/overlay semantics 변경
* 원본 asset 또는 CHD의 저장소 추가
* 보호 성공 판정

### 완료 조건

* 설계·작업 지시서가 구현 전에 존재합니다.
* Windows x86 Debug runtime build, unit tests, product-loader probe가
  성공합니다.
* 실제 4th VFS log에 <code>GetVersion</code>과
  <code>CreateFileA</code>의 반환 주소·caller 주소가 남습니다.
* 호출 성공과 보호 응답의 확인 상태를 과장하지 않습니다.
* 작업 로그와 단일 Git commit을 남깁니다.

## English

Status: implementation and real-CHD trace verification complete. Return
addresses are confirmed, but wrapper invocation remains unresolved.

Related design: [ez2dj4th resolver return ABI trace design](../design/20260901-121-ez2dj4th-resolver-return-trace.md)

### Goal

Add evidence that distinguishes resolver selection from the protected stub's
actual continuation by recording the returned HLE/native function-pointer
address and original caller return address in the VFS log.

### Implementation items

1. Add the returned address and caller address to the dynamic-resolver event.
2. Use the same format for HLE, device-mock, WTS-observe, and native-fallback
   routes.
3. Preserve existing returned pointers and route semantics.
4. Record the real CHD result in analysis, TODO, architecture, and the work log.

### Out of scope

Do not convert calling conventions, modify returned pointers, implement
Hardlock responses, a seed solver, directory enumeration, change VFS/CHD/
overlay semantics, add original assets or CHD storage, or decide protection
success.

### Completion criteria

* The design and work order exist before implementation.
* Windows x86 Debug runtime build, unit tests, and product-loader probe pass.
* The real 4th VFS log contains return and caller addresses for
  <code>GetVersion</code> and <code>CreateFileA</code>.
* Call success and protection-response status are not overstated.
* A work log and one Git commit remain.
