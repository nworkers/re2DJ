# 작업 309 — Linux x86 제품 빌드 결과 / Task 309 — Linux x86 Product Build Results

## 결과 / Result

Linux CMake 구성을 x86·x64 product host 공통으로 확장했다. Linux native backend와 CLI 링크 조건의 x64 제한을 제거하고 helper-only preset만 SDL product target에서 제외했다. `linux-x86-debug`·`linux-x86-release`와 test preset을 추가했으며, probe script가 x64와 x86 host에서 같은 i386 helper를 실행하도록 확장했다. x86 preset은 현재 WSL에 없는 i386 XScreenSaver·XTest 개발 패키지 때문에 두 선택 SDL 통합을 끈다.

*Extended the Linux CMake graph so x86 and x64 product hosts share the Linux native backend and CLI linkage, while only the helper-only preset excludes SDL product targets. Added linux-x86-debug/release and test presets, and extended the probe script to run the same i386 helper from x64 and x86 hosts. The x86 preset disables two optional SDL integrations absent from the current WSL installation: i386 XScreenSaver and XTest development packages.*

## 검증 / Verification

- x86 product Debug build: GCC 13.3.0, `-m32`, warnings-as-errors, all 478 Ninja steps passed.
- x86 CTest: 1/1 passed.
- `file`: x86 product and host probe ELF 32-bit Intel 80386; helper ELF32; x64 product/host probe ELF64 x86-64.
- x86 host probe against the existing i386 helper: result 51, child 0, SIGILL 4 fault event.
- x64 host probe against the same helper: result 51, child 0, SIGILL 4 fault event.
- x86 OpenGL blend probe: 10 pixel checks, 0 failures.
- x64 reconfigure/build: no work after configuration; CTest 1/1 passed.
- Windows x86 `re2dj` and `re2dj_unit_tests` rebuild after CMake regeneration; selected CTest 1/1 passed.

*The x86 product Debug build passed all 478 Ninja steps with GCC 13.3.0, -m32, and warnings-as-errors. x86 CTest passed 1/1. `file` identified the x86 product/host probe and helper as ELF32 i386, and the x64 product/host probe as ELF64 x86-64. Both host probes against the same helper reported result 51, child 0, and SIGILL 4. The x86 OpenGL blend probe passed 10/10. The x64 reconfigure/build had no work and CTest passed 1/1. Windows rebuilt after CMake regeneration and selected CTest passed 1/1.*

## 범위 / Scope

이 작업은 제품 host compile/link과 synthetic backend 경계까지다. Linux Win32 HLE, official HDD execution, audio activation, Clang matrix, native 32-bit kernel, and GPU acceleration remain unverified. 원본 자산은 사용하지 않았다.

*This task covers product-host compilation/linkage and the synthetic backend boundary. Linux Win32 HLE, official HDD execution, audio activation, the Clang matrix, a native 32-bit kernel, and GPU acceleration remain unverified. No original assets were used.*

다음 단계는 L2 memory·ABI·import dispatcher이며, 32비트·64비트 host 모두에서 고정폭 protocol과 guest memory 검증을 유지해야 한다.

*The next phase is L2 memory, ABI, and import dispatch; preserve fixed-width protocol and guest-memory validation on both host widths.*
