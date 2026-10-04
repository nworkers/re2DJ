# 작업 448 작업 지시서 — Windows x86 in-process backend / Task 448 work order — the Windows x86 in-process backend

설계: [20261004-448-windows-x86-backend.md](../design/20261004-448-windows-x86-backend.md) · 상위: [작업 446 설계](../design/20261004-446-windows-in-process-loader.md)

## 절차 / Steps

1. `src/platform/windows/native_host_services.cpp`(VirtualAlloc·VirtualProtect·VirtualFree·FlushInstructionCache·GetTickCount64·Sleep·벽시계)와 `native_host_protection.h`.
   *The Windows host services and the PAGE_* converter.*
2. `src/platform/windows/x86/`: `native_low_memory.cpp`, `native_guest_transition.{h,cpp}`(naked asm), `native_import_bridge.cpp`(fs:0 ↔ 그림자 TEB), `native_process_bootstrap.cpp`(VEH, 그림자 TEB, 배달 스택, 게스트 스레드).
   *The x86 backend files.*
3. `NativeGuestThread::fs_teb`(게스트가 FS로 보는 TEB)와 thread probe의 비교 대상.
   *`NativeGuestThread::fs_teb` and the thread probe's comparison.*
4. `native_in_process_probe.cpp`(Windows): i386 probe 항목 + 게스트 SEH 배달·거절 + 그림자 TEB 확인. CMake `RE2DJ_NATIVE_RUNNER_SOURCES`, `re2dj_windows_native_backend`, CTest.
   *The Windows probe, CMake and CTest.*
5. 문서와 작업 로그, 검증(Windows CTest, Linux 두 폭 build·CTest).
   *Documents, the work log and verification.*
