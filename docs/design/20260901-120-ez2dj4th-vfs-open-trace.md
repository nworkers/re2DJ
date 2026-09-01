# ez2dj4th bounded VFS open trace 설계

## 상태

구현 완료. 작업 119에서 동적 <code>CreateFileA</code> resolver route는
확인되었지만, wrapper 호출과 실제 파일 open 결과는 확인되지 않았습니다.
bounded trace는 wrapper request event가 없음을 확인했습니다.

## 한국어

### 목적과 근거

현재 4th VFS log는 <code>CreateFileA:route=hle</code>를 기록합니다. 그러나
같은 실행에서 asset-open event가 없고 child가
<code>0xc0000005</code>로 종료했습니다. resolver가 HLE 주소를 반환한 것과
원본 protected stub이 그 주소를 호출한 것은 별도 관찰 항목입니다.

이 작업은 injected runtime의 <code>Re2djVfsCreateFileA</code> 진입 및 주요
결과를 bounded trace로 기록하여 두 경계를 구분합니다.

### 설계 결정

1. VFS trace 전용 bounded counter를 추가하고 최대 128개 open trace message만
   기록합니다. 기존 image/script/device trace budget과 분리합니다.
2. <code>Re2djVfsCreateFileA</code> 진입 시 요청 경로, access,
   disposition, flags를 기록합니다.
3. device mock, 경로 매핑 실패, CHD pseudo-handle, overlay/native open의
   주요 결과를 각각 stage와 success/error로 기록합니다.
4. runtime이 trace file을 쓰기 위해 호출하는 native <code>CreateFileA</code>는
   patch 대상이 아니므로 새 event는 injected VFS wrapper 호출만 의미합니다.
5. 경로 매핑, CHD read, overlay write, <code>FILE_FLAG_NO_BUFFERING</code>
   정책은 변경하지 않습니다. 새 trace는 관찰만 추가합니다.
6. 원본 asset과 보호 응답, directory enumeration은 범위에 포함하지 않습니다.

~~~mermaid
sequenceDiagram
    participant G as 4th protected stub
    participant R as Re2djHleGetProcAddress
    participant V as Re2djVfsCreateFileA
    participant T as bounded VFS trace
    G->>R: resolve CreateFileA
    R-->>G: VFS wrapper address
    G->>V: call CreateFileA
    V->>T: request event
    V->>T: stage/result event
    V-->>G: handle or Win32 error
~~~

### 파일별 범위

* <code>src/platform/windows/injected_runtime.cpp</code>: bounded open trace
  counter와 request/result event를 추가합니다.
* <code>docs/analysis/</code>, <code>docs/TODO.md</code>,
  <code>ARCHITECTURE.md</code>: 실제 trace 결과와 확인 상태를 기록합니다.
* work-order와 work-log: 반복 가능한 검증 명령 및 결과를 남깁니다.

### 검증 결과

* Windows x86 injected runtime을 Debug로 빌드합니다.
* unit tests와 product-loader probe를 실행합니다.
* 실제 <code>4thTrax.chd</code>에서 4th dynamic VFS bounded trace를 실행합니다.
* VFS log에 <code>create-file:stage=request</code>가 없었습니다.
* 따라서 현재 증거는 resolver 선택까지이며 실제 wrapper 호출과 매핑·handle
  결과는 미확정입니다. child fault는 보호 성공으로 해석하지 않습니다.

## English

### Status

Implementation complete. Task 119 confirmed the dynamic
<code>CreateFileA</code> resolver route, but neither the wrapper call nor the
actual file-open result has been confirmed. The bounded trace confirmed that
there was no wrapper request event.

### Purpose and evidence

The current 4th VFS log records <code>CreateFileA:route=hle</code>, but the same
run has no asset-open event and the child exits with
<code>0xc0000005</code>. Returning an HLE address from the resolver and having
the protected stub call that address are separate observations.

This task adds a bounded trace for entry into and major results from the
injected runtime's <code>Re2djVfsCreateFileA</code> so those boundaries can be
distinguished.

### Design decisions

1. Add a VFS-open-specific bounded counter with at most 128 open-trace messages,
   separate from existing image/script/device budgets.
2. Record the requested path, access, disposition, and flags when
   <code>Re2djVfsCreateFileA</code> is entered.
3. Record stage plus success/error for the major device-mock, path-mapping
   failure, CHD pseudo-handle, and overlay/native-open results.
4. The native <code>CreateFileA</code> used by the runtime to write its trace
   file is not patched, so the new events represent injected VFS-wrapper calls.
5. Do not change path mapping, CHD reads, overlay writes, or the
   <code>FILE_FLAG_NO_BUFFERING</code> policy. The trace is observational only.
6. Original assets, protection responses, and directory enumeration are out of
   scope.

### File boundaries

* <code>src/platform/windows/injected_runtime.cpp</code>: add the bounded
  open-trace counter and request/result events.
* Update <code>docs/analysis/</code>, <code>docs/TODO.md</code>, and
  <code>ARCHITECTURE.md</code> with the real trace result and confirmation state.
* Leave the repeatable verification commands and results in the work-order and
  work-log.

### Verification result

* Build the Windows x86 injected runtime in Debug.
* Run the unit tests and product-loader probe.
* Run the real <code>4thTrax.chd</code> 4th dynamic-VFS bounded trace.
* The VFS log contained no <code>create-file:stage=request</code> event.
* The current evidence therefore confirms only resolver selection; the wrapper
  call and mapping/handle result remain unresolved. The remaining child fault
  must not be called protection success.
