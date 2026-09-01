# 작업 122 — ez2dj4th resolver caller instruction window

상태: 구현 및 실제 CHD trace 검증 완료. 반환값 저장은 확인되었지만 후속 consumer는 미확정입니다.

## 한국어

관련 설계: [ez2dj4th resolver caller instruction window 설계](../design/20260901-122-ez2dj4th-resolver-caller-window.md)

### 목표

작업 121에서 확인한 resolver caller return address 주변의 현재 protected
memory bytes를 bounded event로 기록하여, 반환 pointer 이후의 실제 code
소비 경계를 좁힙니다.

### 구현 항목

1. resolver caller 기준 앞 8바이트·뒤 16바이트 memory window를 읽습니다.
2. readable 여부, base, caller, hex bytes를 VFS log에 기록합니다.
3. caller-window budget을 32개로 제한합니다.
4. 실제 CHD trace 결과를 analysis, TODO, architecture, work-log에 기록합니다.

### 제외 항목

* code bytes 수정, breakpoint 추가, instruction stepping
* 반환 pointer 또는 calling convention 변경
* 정적 disassembly를 실행 사실로 확정
* Hardlock 응답, directory enumeration, 보호 성공 판정
* 원본 asset 또는 CHD의 저장소 추가

### 완료 조건

* 설계·작업 지시서가 구현 전에 존재합니다.
* Windows x86 Debug runtime build, unit tests, product-loader probe가
  성공합니다.
* 실제 4th VFS log에 caller instruction window event가 남습니다.
* window 해석의 확인 상태를 보수적으로 기록합니다.
* 작업 로그와 단일 Git commit을 남깁니다.

## English

Status: implementation and real-CHD trace verification complete. Return-value
storage is confirmed, but the later consumer remains unresolved.

Related design: [ez2dj4th resolver caller instruction window design](../design/20260901-122-ez2dj4th-resolver-caller-window.md)

### Goal

Narrow the actual code-consumption boundary after the returned pointer by
recording the current protected memory bytes around the resolver caller return
address as a bounded event.

### Implementation items

1. Read an 8-byte-before and 16-byte-after memory window around the resolver
   caller.
2. Record readability, base, caller, and hex bytes in the VFS log.
3. Cap caller-window events at 32.
4. Record the real CHD result in analysis, TODO, architecture, and the work log.

### Out of scope

Do not modify code bytes, add breakpoints, single-step instructions, change the
returned pointer or calling convention, promote static disassembly to runtime
fact, implement Hardlock responses or directory enumeration, decide protection
success, or add original assets/CHD storage.

### Completion criteria

* The design and work order exist before implementation.
* Windows x86 Debug runtime build, unit tests, and product-loader probe pass.
* The real 4th VFS log contains a caller-instruction-window event.
* Window interpretation is recorded with conservative confirmation status.
* A work log and one Git commit remain.
