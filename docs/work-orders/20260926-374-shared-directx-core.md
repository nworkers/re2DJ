# 작업 374 작업 지시서 — 공용 DirectX core와 첫 단계 / Task 374 work order — shared DirectX core, first phase

설계: [20260926-374-shared-directx-core.md](../design/20260926-374-shared-directx-core.md)

## 절차 / Steps

1. `re2dj_directx`(`abi.h`, `directdraw_description`, `direct3d_description`)를 만든다.
   *Create `re2dj_directx` (`abi.h`, `directdraw_description`, `direct3d_description`).*
2. Windows adapter에 `directx_abi_windows.h`를 두고, DirectX 7·6 facade의 설명 코드를 core 호출로 바꾼다.
   *Add `directx_abi_windows.h` to the Windows adapter and replace the DirectX 7 and 6 facades' description code with core calls.*
3. 변경 전 commit을 별도 worktree에서 build한다. 두 build로 `re2dj ez2dj4th`를 30초씩 실행하고 `.ddraw.log`를 비교한다.
   *Build the pre-change commit in a separate worktree, run `re2dj ez2dj4th` for 30 seconds on each build, and compare the `.ddraw.log`s.*
4. Linux ddraw module에 `IDirect3D7`과 `IDirectDraw7` 설명 메서드를 넣는다. `GuestComObject::parent`, `lstrcpynA`, `ShowCursor`를 추가하고, 진단의 문자열 읽기 범위를 넓힌다.
   *Add `IDirect3D7` and the `IDirectDraw7` description methods to the Linux ddraw module, plus `GuestComObject::parent`, `lstrcpynA`, `ShowCursor`, and the wider string ranges in the diagnostic.*
5. 단위 테스트, 문서.
   *Unit tests and documentation.*

## 완료 조건 / Done when

- Windows x86 build(`static_assert` 포함)·CTest가 통과하고, 실제 4th의 DirectX 기록이 변경 전과 같다.
  *Windows x86 builds (with its `static_assert`s) and passes CTest, and the real 4th's DirectX record matches the pre-change build.*
- Linux 두 폭 build·CTest가 통과하고, 실제 4th가 DirectDraw 열거를 마친 뒤 `SetCooperativeLevel`까지 간다.
  *Both Linux widths build and pass CTest, and the real 4th finishes its DirectDraw enumeration and reaches `SetCooperativeLevel`.*
