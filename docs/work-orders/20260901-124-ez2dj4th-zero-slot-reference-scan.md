# 작업 124 — ez2dj4th zero pointer-slot reference scan

## 목적

`EIP=0` fault 직전 읽힌 보호 코드 pointer slot `0x00AF0CF4`가 정식 PE
IAT가 아님을 문서화하고, fault 시점의 live main image에서 해당 slot 주소를
참조하는 위치를 bounded scan하여 초기화·소비 경로 후보를 찾습니다.

관련 설계: [ez2dj4th zero pointer-slot reference scan 설계](../design/20260901-124-ez2dj4th-zero-slot-reference-scan.md)

## 작업 범위

1. HLE 없는 native baseline에서 동일 zero slot과 fault가 발생하는지 비교합니다.
2. fault-call attribution이 absolute pointer slot 주소를 반환하도록 정리합니다.
3. main image의 committed readable region에서 slot 주소 immediate를 검색합니다.
4. match 위치별 section과 24-byte runtime window를 bounded JSONL event로 기록합니다.
5. 실제 CHD trace에서 reference 목록을 수집하고 writer/read/call 후보를 분류합니다.
6. 관련 analysis, TODO, architecture, 작업 로그를 갱신합니다.

## 완료 조건

* Windows x86 Debug launcher build와 기존 회귀 검증이 통과합니다.
* native baseline과 HLE 실행의 zero slot 비교가 문서화됩니다.
* 실제 CHD JSONL에 slot reference와 summary event가 기록됩니다.
* 확인되지 않은 reference를 writer나 보호 응답으로 단정하지 않습니다.
* 원본 자산은 저장소에 추가되지 않습니다.

## English

## Purpose

Document that protected-code pointer slot `0x00AF0CF4`, read immediately before
the `EIP=0` fault, is not a formal PE IAT slot. Search the live main image at
the fault for references to that slot and identify candidate initialization and
consumption paths.

Related design: [ez2dj4th zero pointer-slot reference scan design](../design/20260901-124-ez2dj4th-zero-slot-reference-scan.md)

## Scope

1. Compare the same zero slot and fault in a native baseline without HLE.
2. Return the absolute pointer-slot address from fault-call attribution.
3. Search committed readable main-image regions for the slot-address immediate.
4. Record each match's section and 24-byte runtime window as bounded JSONL.
5. Collect reference locations in a real CHD trace and classify writer/read/call
   candidates conservatively.
6. Update the related analysis, TODO, architecture, and work log.

## Completion criteria

* The Windows x86 Debug launcher build and existing regressions pass.
* The zero-slot comparison between native baseline and HLE runs is documented.
* The real CHD JSONL contains slot-reference and summary events.
* Unconfirmed references are not asserted to be writers or protection responses.
* Original assets are not added to the repository.
