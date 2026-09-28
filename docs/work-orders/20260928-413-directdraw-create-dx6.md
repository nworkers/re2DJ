# 작업 413 작업 지시서 — DirectDrawCreate와 DirectX 6 객체 / Task 413 work order — DirectDrawCreate and the DirectX 6 objects

설계: [20260928-413-directdraw-create-dx6.md](../design/20260928-413-directdraw-create-dx6.md)

## 절차 / Steps

1. Windows DX6 facade의 루트 객체와 `QueryInterface`, 1st가 요청하는 IID를 확인한다.
   *Check the Windows DX6 facade's root object and `QueryInterface`, and the IIDs 1st asks for.*
2. `IDirectDraw4`와 `IDirect3D3` 객체, `DirectDrawCreate`를 구현한다.
   *Implement the `IDirectDraw4` and `IDirect3D3` objects and `DirectDrawCreate`.*
3. 단위 테스트, 1st 실행으로 다음 경계 확인, 문서.
   *Unit tests, a 1st run for the next boundary, and documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 테스트를 통과한다.
  *Windows x86 and both Linux widths build and pass their tests.*
- Linux 1st가 `DirectDrawCreate`와 `QueryInterface`를 지난다.
  *On Linux, 1st gets past `DirectDrawCreate` and `QueryInterface`.*
