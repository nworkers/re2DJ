# 작업 095 — 1st SE execute-at-zero 크래시 작업 지시

관련 설계: [1st SE execute-at-zero 크래시 귀속](../design/20260829-095-execute-zero-crash.md)

상태: **완료**

## 작업

1. WER dump의 exception stream, x86 context, memory와 module 정보를 읽는다.
2. execute address 0의 stack return과 원본 call site를 확인한다.
3. object/vtable/slot 또는 함수 pointer 계약을 원본 instruction과 기존 facade에 대조한다.
4. 확인된 `IDirect3DDevice3::DrawIndexedPrimitiveVB` 슬롯, triangle-list topology, 범위 검사된 16-bit index 전개를 구현하고 회귀 probe를 추가한다.
5. Debug/Release build·CTest와 실제 제품 재실행으로 검증한다.
6. analysis, ARCHITECTURE, TODO/IMPLEMENTED와 작업 로그를 갱신한다.

## 안전 조건

- dump, runtime log와 원본 자산은 읽기 전용이며 커밋하지 않는다.
- 원본 binary를 patch하지 않는다.
- 확인되지 않은 HRESULT 성공이나 gameplay 대체 logic을 추가하지 않는다.

---

# Task 095 — 1st SE execute-at-zero crash work order

Related design: [1st SE execute-at-zero crash attribution](../design/20260829-095-execute-zero-crash.md)

Status: **Complete**

## Work

1. Read the WER dump's exception stream, x86 context, memory, and module information.
2. Identify the execute-at-zero stack return and original call site.
3. Match the object, vtable slot, or function-pointer contract against original instructions and existing facades.
4. Implement the confirmed `IDirect3DDevice3::DrawIndexedPrimitiveVB` slot, triangle-list topology, and bounds-checked 16-bit index expansion, then add a regression probe.
5. Verify with Debug/Release builds and CTest plus a live product rerun.
6. Update analysis, ARCHITECTURE, TODO/IMPLEMENTED, and the work log.

## Safety conditions

- Treat the dump, runtime logs, and original assets as read-only and never commit them.
- Do not patch the original binary.
- Do not add unconfirmed success HRESULTs or replacement gameplay logic.
