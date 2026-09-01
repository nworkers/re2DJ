# 작업 120 — ez2dj4th bounded VFS open trace

상태: 구현 및 실제 CHD trace 검증 완료. wrapper request event는 관찰되지 않았습니다.

## 한국어

관련 설계: [ez2dj4th bounded VFS open trace 설계](../design/20260901-120-ez2dj4th-vfs-open-trace.md)

### 목표

작업 119에서 확인된 <code>CreateFileA:route=hle</code>가 실제
<code>Re2djVfsCreateFileA</code> 호출로 이어지는지, 그리고 요청 경로가
CHD·overlay·native 중 어디로 처리되는지 제한된 trace로 확인합니다.

### 구현 항목

1. injected runtime에 VFS open trace 전용 bounded budget을 추가합니다.
2. <code>Re2djVfsCreateFileA</code> 진입 요청과 주요 결과 stage를 기록합니다.
3. 기존 VFS 파일 의미, CHD read-only 정책, overlay write 정책을 유지합니다.
4. 실제 CHD 실행 결과를 analysis, TODO, architecture, work-log에 기록합니다.

### 제외 항목

* Hardlock 응답, seed solver, 보호 성공 판정
* directory enumeration 구현
* 원본 asset 또는 CHD의 저장소 추가
* native Win32 <code>CreateFileA</code> 자체의 trace/patch
* 기존 1st/3rd VFS 및 device mock semantics 변경

### 완료 조건

* 설계·작업 지시서가 구현 전에 존재합니다.
* Windows x86 Debug runtime build, unit tests, product-loader probe가
  성공합니다.
* 실제 4th CHD VFS log에서 wrapper request event 유무와 stage/result를
  확인합니다.
* 확인됨·추정·미확정 상태를 분석 문서에 구분합니다.
* 작업 로그와 단일 Git commit을 남깁니다.

## English

Status: implementation and real-CHD trace verification complete. No wrapper
request event was observed.

Related design: [ez2dj4th bounded VFS open trace design](../design/20260901-120-ez2dj4th-vfs-open-trace.md)

### Goal

Use a bounded trace to determine whether the confirmed
<code>CreateFileA:route=hle</code> reaches an actual
<code>Re2djVfsCreateFileA</code> call, and whether its requested path is handled
by CHD, overlay, or native storage.

### Implementation items

1. Add a VFS-open-specific bounded budget to the injected runtime.
2. Record the request entering <code>Re2djVfsCreateFileA</code> and its major
   result stages.
3. Preserve existing VFS semantics, read-only CHD behavior, and overlay-write
   policy.
4. Record the real CHD result in analysis, TODO, architecture, and the work log.

### Out of scope

Do not implement Hardlock responses, a seed solver, a protection-success
decision, directory enumeration, original assets or CHD storage, tracing or
patching of native Win32 <code>CreateFileA</code>, or changes to existing
1st/3rd VFS and device-mock semantics.

### Completion criteria

* The design and work order exist before implementation.
* Windows x86 Debug runtime build, unit tests, and product-loader probe pass.
* The real 4th CHD VFS log is checked for the wrapper request and stage/result.
* Confirmed, inferred, and unresolved status is separated in analysis documents.
* A work log and one Git commit remain.
