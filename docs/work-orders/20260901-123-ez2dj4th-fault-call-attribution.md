# 작업 123 — ez2dj4th EIP=0 fault 호출 대상 귀속

## 목적

작업 122에서 확인한 `CreateFileA` resolver 반환값 저장 이후의 fault를 호출 명령과 포인터 슬롯 수준으로 관찰합니다. `EIP=0` execute fault가 발생한 시점의 stack return address 직전 x86 call encoding과 간접 호출 대상 슬롯을 bounded diagnostic으로 기록합니다.

## 작업 범위

1. `RecordAccessViolationContext`에 stack return address 기반의 fault-call attribution을 추가합니다.
2. relative direct call, absolute memory-indirect call, register-indirect call을 제한적으로 식별합니다.
3. absolute memory-indirect call의 pointer slot 값을 읽고 target memory allocation 및 image section/export 정보를 기록합니다.
4. 진단은 read-only로 유지하며 원본 프로세스의 context, code, pointer slot을 변경하지 않습니다.
5. 실제 `ez2dj4th` CHD trace에서 zero-target 여부와 다음 실행 경계를 확인합니다.
6. 관련 설계, 분석, TODO, architecture, 작업 로그를 한국어/영어로 갱신합니다.

## 완료 조건

* Windows x86 Debug launcher가 빌드됩니다.
* 단위 테스트와 product-loader probe가 통과합니다.
* 실제 CHD bounded trace의 `.jsonl`에 `av_indirect_call` 이벤트가 남습니다.
* 이벤트가 fault stack return address, call encoding, pointer slot, target 값 및 가능한 section 정보와 연결됩니다.
* target 값이 0이어도 보호 응답 성공이나 정상 게임 실행으로 판정하지 않습니다.
* 원본 실행 파일, CHD, HDD 자산은 저장소에 추가되지 않습니다.

## English

## Purpose

Observe the post-task-122 fault at the call-instruction and pointer-slot level. At the `EIP=0` execute fault, record the x86 call encoding immediately before saved stack return addresses and the target of any absolute memory-indirect call through a bounded diagnostic.

## Scope

1. Add fault-call attribution based on stack return addresses to `RecordAccessViolationContext`.
2. Identify relative direct calls, absolute memory-indirect calls, and register-indirect calls conservatively.
3. Read the pointer-slot value for absolute memory-indirect calls and record target allocation and image section/export information when available.
4. Keep the diagnostic read-only; do not change original process context, code, or pointer slots.
5. Use a real `ez2dj4th` CHD trace to determine whether the target is zero and expose the next execution boundary.
6. Update the related design, analysis, TODO, architecture, and work-log documents in Korean and English.

## Completion criteria

* The Windows x86 Debug launcher builds.
* Unit tests and the product-loader probe pass.
* The real CHD bounded trace contains an `av_indirect_call` event in `.jsonl`.
* The event connects the fault-stack return address, call encoding, pointer slot, target value, and available section information.
* A zero target is not treated as protection success or normal game execution.
* The original executable, CHD, and HDD assets are not added to the repository.
