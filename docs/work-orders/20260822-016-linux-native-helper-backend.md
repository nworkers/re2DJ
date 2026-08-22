# Linux Native Helper PE32 Backend

## 한국어

### 작업 목표

Linux native helper feasibility probe를 범용 PE32 mapping 및 `ExecutionBackend` adapter 경로로 확장합니다.

### 작업 항목

1. Linux public backend header와 POSIX IPC adapter를 추가합니다.
2. i386 helper에 PE32 map, relocation, import thunk, TLS callback을 추가합니다.
3. 기존 x64 host probe를 adapter 기반 합성 PE 결합 검증으로 교체합니다.
4. CMake preset 및 Linux 검증 script가 새 helper target을 빌드하도록 갱신합니다.
5. Linux platform 문서와 작업 로그를 갱신하고 결합 검증을 수행합니다.

### 완료 기준

요청 base의 합성 PE32가 Linux i386 helper에서 실행되고, 두 import event와 read/write/completion 왕복 후 예상 process-exit 값 51을 host probe가 확인합니다.

## English

### Objective

Extend the Linux native-helper feasibility probe into a generic PE32 mapping and `ExecutionBackend` adapter path.

### Work items

1. Add a Linux public backend header and POSIX IPC adapter.
2. Add PE32 mapping, relocation, import thunks, and TLS callbacks to the i386 helper.
3. Replace the existing x64 host probe with an adapter-based synthetic-PE integration check.
4. Update the CMake preset and Linux verification script to build the new helper target.
5. Update Linux platform documentation and the work log, then run integration verification.

### Completion criteria

The Linux i386 helper executes a synthetic PE32 at the requested base, and the host probe confirms two import events with read/write/completion round trips followed by expected process exit 51.
