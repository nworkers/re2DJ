# 종료 귀속과 식재 패턴 추적

관련 작업 지시: [종료 귀속과 식재 패턴 추적 작업 지시](../work-orders/20260823-046-teardown-attribution.md)

## 목적

protected `ez2dj.exe`의 종료가 언로드 종반 힙 파괴 안에서 일어난다는 것까지 확인됐다([작업 로그 20260823-045](../work-logs/20260823-045-post-gate-resume-trace.md)). 남은 질문은 두 가지다. 첫째, `RtlDestroyHeap`이 누구 소유인가(ws2_32 detach인가). 둘째, 실행마다 불변인 fault 서명(`ECX=EDX=ESI=EDI`=entry VA, `EBX`=fault page base)의 값 공급원은 어디인가. 이 작업은 두 관찰을 추가해 답을 좁힌다.

## 설계

### 복귀 시점 깊은 stack 덤프와 심볼 주석

`syscall_resume_hit` 처리에서 stack 상단 word를 4개에서 64개로 넓혀 읽고, 전체를 한 기록으로 남긴 뒤, MEM_IMAGE 범위에 드는 word마다 nearest-export 심볼(`모듈!함수+오프셋`)을 붙여 `resume_stack_symbol` event로 남긴다. 주석은 최대 24개로 제한하고, 모듈·주소 캐시를 샘플 주석과 공유한다. 동적으로 적재된 `ws2_32` 영역의 복귀 주소가 보이면 detach 귀속이 확인되고, 없으면 다른 소유 후보를 따진다.

### fault 시점 entry 참조 탐색

first-chance illegal-instruction에서 child를 계속하기 전에, committed private/image 메모리를 경계 있는 블록 순회로 훑어 entry VA dword 참조를 찾는다. 구조는 기존 `ScanFaultReferences`와 같고 일치 조건만 다르다. 각 match는 앞뒤 dword 값을 함께 기록해 `{entry ×4}` 연속 실행 여부를 판별한다. 결과 상한 48개, 초과 시 capped 표시. 이 탐색은 `--api-trace` 없이도 fault 컨텍스트 기록의 일부로 동작한다.

### 해석 경계

stack word의 심볼은 nearest-export 근사라 실제 프레임을 증명하지 않는다. entry 참조 match는 값의 저장 위치일 뿐 그것이 register 공급원임을 증명하지 않는다. 두 관찰 모두 confirmed observation과 inferred candidate로 나눠 기록한다.

## 검증

Windows x86 build·CTest 후 canonical `ez2dj.exe`를 `--api-trace`로 실행한다. 성공 기준: `resume_stack_symbol` 기록이 생성돼 복귀 시점 stack의 모듈 분포를 판별할 수 있고, `fault_entry_reference` 기록이 entry 값의 저장 위치(연속 실행 포함)를 보여준다.

---

# Teardown Attribution and Planted-Pattern Tracing Work Order

Related design: [Teardown Attribution and Planted-Pattern Tracing](../design/20260823-046-teardown-attribution.md)

## Purpose

Two questions remain after pinning the termination inside unload-tail heap destruction: who owns `RtlDestroyHeap` (ws2_32 detach?), and where the run-invariant fault-signature values (entry VA repeated across registers, fault page base) come from. Add two observations to narrow both.

## Design

### Deep stack dump with symbols at the resume hit

Read 64 top stack words at `syscall_resume_hit` instead of four, log them in one record, and annotate every word landing in a MEM_IMAGE range with its nearest-export symbol (`module!function+offset`) as separate `resume_stack_symbol` events, capped at 24 annotations and sharing the sample-annotation module/address cache. Return addresses inside dynamically loaded `ws2_32` would confirm detach ownership; their absence forces other ownership candidates.

### Bounded entry-reference scan at the fault

On a first-chance illegal instruction, before continuing the child, walk committed private/image memory in bounded blocks — same structure as the existing `ScanFaultReferences`, matching only the entry VA dword. Each match records its neighboring dwords so consecutive `{entry ×4}` runs are visible, capped at 48 results with a capped flag. This runs as part of the fault-context record even without `--api-trace`.

## Interpretation boundary

Stack-word symbols are nearest-export approximations, not frame proofs; an entry reference marks a storage site, not proven register provenance. Keep confirmed observations and inferred candidates separate.

## Verification

Build, run CTest, run canonical `ez2dj.exe` with `--api-trace`. Success criteria: `resume_stack_symbol` records reveal the module distribution of the resume-time stack, and `fault_entry_reference` records show where the entry value is stored, including any consecutive runs.
