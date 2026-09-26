# 작업 382 작업 지시서 — DirectX core 4단계: 장치 / Task 382 work order — DirectX core phase 4: the device

설계: [20260926-382-directx-device.md](../design/20260926-382-directx-device.md)

## 절차 / Steps

1. core에 `direct3d_device.h/.cpp`를 두고 ABI 구조체와 상수를 더한다.
   *Add `direct3d_device.h/.cpp` to the core, with the ABI structures and constants.*
2. Windows `DeviceFacade`의 상태를 `DeviceState`로 옮긴다. `CreateDevice`와 상태 메서드가 core를 쓰게 하고, SDK 검사를 더한다.
   *Move Windows `DeviceFacade`'s state onto `DeviceState`, put `CreateDevice` and the state methods on the core, and add the SDK checks.*
3. Linux에 `ddraw_device7.cpp`와 `IDirect3D7::CreateDevice`를 추가한다. COM 도우미에 `ReadStruct`를 더한다.
   *On Linux, add `ddraw_device7.cpp` and `IDirect3D7::CreateDevice`, with `ReadStruct` in the COM helpers.*
4. 검증과 문서.
   - 변경 전 build(`4532fa1`)와 Windows 실제 4th를 비교한다.
   - 단위 테스트를 추가한다(`directx_device_test.cpp`, ddraw).
   - 문서를 쓴다.

   *Compare the real 4th on Windows against the pre-change build (`4532fa1`), add unit tests (`directx_device_test.cpp`, ddraw), and write the documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 CTest를 통과한다. Windows 실제 4th의 기록이 변경 전과 같다.
  *Windows x86 and both Linux widths build and pass CTest, and the real 4th's record on Windows matches the pre-change build.*
- Linux 실제 4th가 두 폭에서 `CreateDevice`와 장치 설정을 지난다.
  *On both Linux widths the real 4th passes `CreateDevice` and the device setup.*
