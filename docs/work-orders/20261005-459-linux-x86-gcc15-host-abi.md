# 작업 459 작업 지시서 — GCC 15·새 배포판에서 Linux x86 product 링크·실행 / Task 459 work order — linking and running the Linux x86 product with GCC 15 on a newer distribution

설계: [20261005-459-linux-x86-gcc15-host-abi.md](../design/20261005-459-linux-x86-gcc15-host-abi.md)

## 절차 / Steps

1. `src/platform/linux/x86/native_process_bootstrap.cpp`: `g_native_host_gs_selector` 정의를 이름 없는 namespace 밖으로 옮긴다.
   *Move the `g_native_host_gs_selector` definition out of the unnamed namespace.*
2. `src/platform/linux/x86/native_import_bridge.cpp`: `NativeImportGateBridge`가 처리기를 부르기 전에 스택을 16바이트로 정렬하고, 복귀 뒤 `%ebp` 기준으로 복원한다.
   *Align the stack to 16 bytes before `NativeImportGateBridge` calls the handler and restore from `%ebp` afterwards.*
3. 가이드 `linux-sdl3-build.md`에 NVIDIA 32비트 Wayland 우회 방법을 적고, kb `i386-host-abi-at-guest-boundary.md`와 색인, ARCHITECTURE를 갱신한다.
   *Add the NVIDIA 32-bit Wayland workaround to the guide, and add the kb topic, its index entry and an ARCHITECTURE note.*
4. 설계의 검증 항목을 실행한다.
   *Run the design's verification.*

## 완료 조건 / Done when

x86 Debug(경고를 오류로)·Release가 빌드되고 CTest가 통과하며, x86으로 4th와 6th가 창을 열고 정상 종료한다.

*x86 Debug (warnings as errors) and Release build, CTest passes, and 4th and 6th open their windows and end cleanly on x86.*
