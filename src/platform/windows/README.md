# src/platform/windows

Windows 전용 backend와 probe를 둡니다. x86 helper process와 IPC로 게스트를 실행하던 native helper 계열(`native_helper_backend`, `native_ipc_helper`, `native_pe_image`, `native_import_thunks`, helper probe)은 작업 379에서 제거했습니다.

*Windows-specific backends and probes. The native-helper family that ran the guest in an x86 helper process over IPC (`native_helper_backend`, `native_ipc_helper`, `native_pe_image`, `native_import_thunks`, and the helper probe) was removed in Task 379.*

`original_process_backend.cpp`는 Windows loader가 원본 PE를 주 image로 적재하는 검증된 실행 engine을 제품 CLI와 진단 launcher가 함께 사용하게 합니다. 제품 facade는 선택된 built-in profile의 실행 기본값을 사용하며, 현재 `ez2dj1stse`, `ez2dj2nd`, `ez2dj3rd` 정책을 허용합니다. `ez2dj2nd`는 확인된 raw-I/O helper 주소를 사용하고 2nd 전용 보호 계약은 미확정입니다. runtime 주입과 import-thunk HLE를 활성화한 detached 실행을 프로파일별로 조율합니다.

*`original_process_backend.cpp` shares the verified engine, in which the Windows loader maps the original PE as the main image, between the product CLI and diagnostic launcher. The product facade consumes the selected built-in profile's execution defaults and currently permits the `ez2dj1stse`, `ez2dj2nd`, and `ez2dj3rd` policies. `ez2dj2nd` uses the confirmed raw-I/O helper addresses; its version-specific protection contract remains unresolved. It orchestrates detached execution with profile-specific runtime injection and import-thunk HLE.*

`ez2dj2nd`의 실행 정책은 2nd 정적 IAT에 없는 `GetPrivateProfileIntA`를 위해 demo-volume을 기본 주입하지 않습니다. raw I/O는 실행으로 확인된 input/output helper RVA `0x000782d7`/`0x0007832b`를 사용합니다. launcher의 `DirectDrawCreateEx` IAT patch 예외는 packer import table을 보존해야 하는 `ez2dj4th`에만 적용됩니다.

*The `ez2dj2nd` execution policy does not inject demo-volume by default because its static IAT has no `GetPrivateProfileIntA`. Raw I/O uses the runtime-confirmed input/output helper RVAs `0x000782d7` and `0x0007832b`. The launcher's `DirectDrawCreateEx` IAT patch exception is limited to `ez2dj4th`, whose packer import table must be preserved.*

이 디렉터리의 루트는 Windows 전용이면서 host 비트 폭 중립인 코드 또는 x86/x64 공용 코드용입니다. 32비트 process·i386 ABI에만 성립하는 injected runtime, COM facade 구현은 향후 `x86/`로, 64비트 process에만 성립하는 구현은 `x64/`로 분리합니다. 현재 파일의 대량 이동은 CMake, include 경로와 runtime DLL 계약을 함께 바꾸는 별도 구조 작업입니다. Windows host API header는 이 플랫폼 트리 안에서만 포함할 수 있습니다.

*This directory root is for Windows-specific code that is host-width-neutral or shared by x86 and x64. The injected runtime and COM facades valid only in a 32-bit process or i386 ABI will move under `x86/`; 64-bit-process-only implementations will move under `x64/`. Bulk movement of current files is a separate structural task that updates CMake, include paths, and the runtime-DLL contract together. Host Windows API headers may be included only inside this platform tree.*
