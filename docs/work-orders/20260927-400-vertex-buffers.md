# 작업 400 작업 지시서 — DX7 vertex buffer / Task 400 work order — DX7 vertex buffers

설계: [20260927-400-vertex-buffers.md](../design/20260927-400-vertex-buffers.md)

## 절차 / Steps

1. Windows facade의 vertex buffer 규칙을 core로 옮기고 facade가 쓰게 한다.
   *Move the Windows facade's vertex buffer rules into the core and have the facade use them.*
2. Linux `IDirect3DVertexBuffer7`, `CreateVertexBuffer`, `DrawPrimitiveVB`, `DrawIndexedPrimitiveVB`를 구현한다.
   *Implement Linux `IDirect3DVertexBuffer7`, `CreateVertexBuffer`, `DrawPrimitiveVB`, and `DrawIndexedPrimitiveVB`.*
3. 단위 테스트, 실제 실행, Windows 전후 비교, 문서.
   *Unit tests, a real run, a Windows before/after comparison, and documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 CTest를 통과하고, Windows 실제 4th의 그래픽·vertex buffer 기록이 작업 전과 같다.
  *Windows x86 and both Linux widths build and pass CTest, and the real 4th's graphics and vertex buffer logs on Windows match the ones before the task.*
- Linux 실제 4th가 vertex buffer 생성·잠금을 지난다.
  *On Linux the real 4th gets past vertex buffer creation and locking.*
