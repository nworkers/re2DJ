# 20260904-166 IDirect3D7 / IDirectDraw7 COM Facade 분리 구현 계획서
# 20260904-166 IDirect3D7 / IDirectDraw7 COM Facade Separation Implementation Work Order

## 1. 목적 (Goal)

DirectDraw와 Direct3D의 DirectX 버전별 분리 원칙에 따라, `direct3d3_com_facade`로부터 DirectX 7 관련 구현을 독립시켜 신규 파일 `directdraw7_com_facade` 및 `direct3d7_com_facade`로 분리 구현한다. 이를 통해 EZ2DJ 4th Trax의 `DirectDrawCreateEx` 및 `IDirect3D7` 초기화 시 발생하는 `0x00000000` 크래시를 해결하고, 최종 목표 지점인 `IDirectDraw7::SetDisplayMode` (`0x00010a6f`) 정상 호출 및 guard 2 통과를 달성한다.

In accordance with the DirectX version separation principle between DirectDraw and Direct3D, isolate DirectX 7 implementations from `direct3d3_com_facade` into dedicated new files `directdraw7_com_facade` and `direct3d7_com_facade`. This resolves the `0x00000000` crash occurring during EZ2DJ 4th Trax's `DirectDrawCreateEx` and `IDirect3D7` initialization, achieving successful execution of `IDirectDraw7::SetDisplayMode` (`0x00010a6f`) and passing guard 2.

---

## 2. 작업 단계 (Tasks)

1. **공통 컨텍스트 헤더 추출 (`directdraw_com_context.h`)**:
   - Facade 공통 상태(창 핸들, 해상도, 백엔드 렌더러 참조 등)를 헤더로 분리.
2. **`direct3d7_com_facade.h/.cpp` 신규 작성**:
   - `IDirect3D7` 인터페이스 정의 및 vtable 구현 (`EnumDevices`, `CreateDevice`, `EnumZBufferFormats` 등).
   - `IDirect3DDevice7` 최소 호환 vtable 구현.
3. **`directdraw7_com_facade.h/.cpp` 신규 작성**:
   - `IDirectDraw7` 인터페이스 정의 및 vtable 구현 (`SetCooperativeLevel`, `SetDisplayMode`, `CreateSurface`, `GetCaps`).
   - `Re2djHleDirectDrawCreateEx` 진입점을 이 파일로 이전.
   - `QueryInterface`에서 `IID_IDirect3D7` 요청 시 `IDirect3D7` 인터페이스 반환.
4. **`CMakeLists.txt` 등록**:
   - `re2dj_windows_injected_runtime` 타겟에 신규 소스 파일 등록.
5. **빌드 및 단위 테스트**:
   - `scripts/build_win32.bat` 빌드 통과 및 `re2dj_unit_tests.exe` 무결성 검증.
6. **EZ2DJ 4th Trax 런타임 진단 검증**:
   - `re2dj_windows_x86_launcher_probe.exe`를 통해 `IDirect3D7` 초기화 통과 및 `SetDisplayMode` 진입, guard 2 통과 여부 확인.
