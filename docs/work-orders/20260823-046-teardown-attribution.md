# 종료 귀속과 식재 패턴 추적 작업 지시

관련 설계: [종료 귀속과 식재 패턴 추적](../design/20260823-046-teardown-attribution.md)

## 목표

`RtlDestroyHeap`의 소유(ws2_32 detach 여부)와, 실행마다 불변인 fault 서명 값(entry VA 반복, fault page base)의 공급원을 좁힌다.

## 작업

1. `syscall_resume_hit`에서 stack 상단 64 word를 기록하고, MEM_IMAGE 범위 word에 nearest-export 심볼을 붙여 `resume_stack_symbol` event로 남긴다(최대 24개 주석, 샘플 주석과 캐시 공유).
2. first-chance illegal-instruction에서 committed private/image 메모리를 경계 있는 순회로 훑어 entry VA dword 참조를 찾고, 앞뒤 dword와 함께 `fault_entry_reference`로 기록한다(상한 48개, capped 표시).
3. Windows x86 build·CTest·canonical 실행으로 검증하고 결과를 구조 문서·분석 문서·TODO에 반영한다.

---

# Teardown Attribution and Planted-Pattern Tracing Work Order

Related design: [Teardown Attribution and Planted-Pattern Tracing](../design/20260823-046-teardown-attribution.md)

## Goal

Narrow who owns `RtlDestroyHeap` (ws2_32 detach?) and where the run-invariant fault-signature values come from.

## Tasks

1. At `syscall_resume_hit`, record the top 64 stack words and attach nearest-export symbols to words inside MEM_IMAGE ranges as `resume_stack_symbol` events (max 24 annotations, sharing the sample-annotation cache).
2. On a first-chance illegal instruction, bounded-walk committed private/image memory for entry VA dword references and record them with neighboring dwords as `fault_entry_reference` (limit 48, capped flag).
3. Verify with the Windows x86 build, CTest, and a canonical run; reflect results in the structures and analysis documents plus TODO.
