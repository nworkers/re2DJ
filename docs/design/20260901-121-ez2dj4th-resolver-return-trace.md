# ez2dj4th 동적 resolver 반환 ABI trace 설계

## 상태

구현 완료. 작업 120에서 resolver 내부의 HLE route는 확인되었지만
<code>Re2djVfsCreateFileA</code> wrapper request는 관찰되지 않았습니다.
반환 주소와 원본 caller를 실제 CHD VFS log에서 확인했습니다.

## 한국어

### 목적과 근거

4th VFS log의 <code>CreateFileA:route=hle</code>는 runtime이 HLE 함수
주소를 선택했다는 증거입니다. 그러나 반환된 함수 포인터의 실제 주소,
resolver에 진입한 원본 caller, 그리고 native fallback의 반환 실패 여부가
기록되지 않아 protected stub의 다음 동작을 구분할 수 없습니다.

이번 작업은 resolver 반환 ABI의 관찰 정보만 추가합니다. 반환 주소를 바꾸거나
호출 규약을 변환하지 않습니다.

### 설계 결정

1. <code>ReportDynamicResolverName</code> event에 반환 함수 포인터 주소와
   resolver 진입 caller 주소를 추가합니다.
2. resolver 진입 직후 <code>_ReturnAddress()</code>를 한 번 캡처하여
   injected runtime 내부의 nested native <code>GetProcAddress</code> 주소가
   아니라 원본 호출자의 return address를 기록합니다.
3. HLE 파일 API, device mock, WTS observe, native Win32 fallback 모두 같은
   event 형식을 사용합니다. null fallback은 주소 0으로 기록합니다.
4. 기존 route 선택, wrapper 구현, CHD/overlay semantics와 trace budget은
   변경하지 않습니다.
5. 결과로 protected stub의 정상 실행이나 보호 응답을 확정하지 않습니다.

~~~mermaid
sequenceDiagram
    participant G as 4th protected stub
    participant R as resolver thunk
    participant L as resolver log
    G->>R: GetProcAddress(name)
    R->>L: name, route, return address, original caller
    R-->>G: unchanged FARPROC result
    G->>G: next protected instruction
~~~

### 검증 전략

* Windows x86 injected runtime을 Debug로 빌드합니다.
* unit tests와 product-loader probe를 실행합니다.
* 실제 <code>4thTrax.chd</code> bounded trace의 VFS log에서
  <code>GetVersion</code>과 <code>CreateFileA</code>의
  <code>address</code>/<code>caller</code> 필드를 확인합니다.
* 실제 trace에서 <code>CreateFileA</code> 반환 주소
  <code>0x62f5350d</code>가 runtime base <code>0x62f50000</code> 범위에 있고,
  native <code>GetVersion</code> 반환 주소 <code>0x77451c10</code>가
  kernel32 base <code>0x77430000</code> 범위에 있음을 확인했습니다.
* 주소 범위가 유효해도 wrapper 호출 성공이나 보호 응답은 판정하지 않습니다.

## English

### Status

Implementation complete. Task 120 confirmed the resolver's HLE route but
observed no <code>Re2djVfsCreateFileA</code> request.
The real CHD VFS log now records return pointers and original callers.

### Purpose and evidence

The <code>CreateFileA:route=hle</code> entry in the 4th VFS log proves that the
runtime selected an HLE function address. The actual returned pointer, the
original caller entering the resolver, and whether a native fallback returned
null are not recorded, so the protected stub's next action cannot yet be
distinguished.

This task adds observation data for the resolver return ABI only. It does not
change returned addresses or convert calling conventions.

### Design decisions

1. Add the returned function-pointer address and resolver-entry caller address
   to the <code>ReportDynamicResolverName</code> event.
2. Capture <code>_ReturnAddress()</code> once on resolver entry so the log
   records the original caller's return address rather than the nested native
   <code>GetProcAddress</code> call inside the injected runtime.
3. Use the same event format for HLE file APIs, device mock, WTS observe, and
   native Win32 fallback. Record a null fallback as address zero.
4. Do not change route selection, wrapper implementation, CHD/overlay
   semantics, or the trace budget.
5. Do not use the result to claim normal protected execution or a protection
   response.

### Verification strategy

* Build the Windows x86 injected runtime in Debug.
* Run the unit tests and product-loader probe.
* Check the real <code>4thTrax.chd</code> bounded trace VFS log for
  <code>address</code> and <code>caller</code> fields on
  <code>GetVersion</code> and <code>CreateFileA</code>.
* The real trace confirmed that <code>CreateFileA</code> returned
  <code>0x62f5350d</code> within runtime base <code>0x62f50000</code>, while
  native <code>GetVersion</code> returned <code>0x77451c10</code> within
  kernel32 base <code>0x77430000</code>.
* A valid address range alone is not proof of a successful wrapper call or
  protection response.
