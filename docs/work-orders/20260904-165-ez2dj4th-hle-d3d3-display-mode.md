# Task 165: EZ2DJ 4th DirectDraw/Direct3D3 HLE 연결 및 IDirectDraw4::SetDisplayMode 동작 검증 작업 지시서

## 1. 개요 (Overview)

EZ2DJ 4th에 대해 DirectDraw/Direct3D3 HLE를 활성화하고, `IDirectDraw4::SetDisplayMode` 가상 호출이 re2DJ의 `RootSetDisplayMode`로 전달되어 `DD_OK`를 반환하는지, 그리고 guard 2를 통과하여 필드 초기화기(`0x00018234`)에 도달하는지 확인합니다.

This work order wires DirectDraw/Direct3D3 HLE for EZ2DJ 4th and verifies that `IDirectDraw4::SetDisplayMode` reaches re2DJ's `RootSetDisplayMode`, returns `DD_OK`, and allows guard 2 to proceed to the field initializer.

---

## 2. 작업 계획 (Action Plan)

1. **DirectDraw 생성 후킹 경로 확인 및 연결**:
   - `EZ2DJ.EXE`의 `DirectDrawCreate` IAT 슬롯 존재 여부 검사.
   - `injected_runtime`의 `_Re2djHleGetProcAddress` 및 정적 슬롯 패치 경로 점검.
   - `ez2dj4th`의 `run_defaults.hle_d3d3` 지원 활성화 (또는 launcher probe의 타겟 허용 범위 확장).
2. **빌드 및 단위 테스트**:
   - CTest 및 `re2dj_unit_tests.exe` 무결성 검증.
3. **런타임 진단 실행 및 결과 관찰**:
   - `--hle-d3d3` 플래그를 포함하여 4th CHD 진단 실행.
   - `RVA 0x00010a6f` 가상 호출 시 대상 함수 포인터가 `re2dj_windows_injected_runtime.dll`의 `RootSetDisplayMode`인지 확인.
   - 반환값 `EAX`가 `0x00000000` (`DD_OK`)인지 관찰.
   - guard 2 조기 이탈 여부 및 필드 초기화기(`0x00018234`) 진입 여부 관찰.
4. **결과 문서화 및 커밋**:
   - 작업 로그 작성 및 런타임 분석 갱신.
