# 작업 459 설계 — GCC 15·새 배포판에서 Linux x86 product가 링크·실행되지 않는 문제 / Task 459 design — the Linux x86 product failing to link and run with GCC 15 on a newer distribution

선행: [작업 458 로그(Linux 실기 검증)](../work-logs/20261005-458-linux-desktop-validation.md), [작업 330 설계(x86 in-process runner)](20260919-330-linux-x86-in-process-runner.md)

## 문제 / Problem

Ubuntu 26.04(GCC 15.2, glibc 2.43, Mesa 26, NVIDIA 595) 실기에서 `linux-x86-debug`가 두 단계에서 실패했다. CI(Debian 12 컨테이너, GCC 12)와 WSL(Ubuntu 24.04, GCC 13)에서는 둘 다 드러나지 않았다.

1. **링크**: `native_import_bridge.cpp`와 `native_process_bootstrap.cpp`의 인라인 어셈블리, 그리고 `extern "C"` 선언이 `g_native_host_gs_selector`를 찾지 못한다. 정의가 `native_process_bootstrap.cpp`의 이름 없는 namespace 안에 `extern "C"`로 있어서, GCC 15.2가 `_ZN5re2dj8platform6native12_GLOBAL__N_125g_native_host_gs_selectorE`로 맹글링했다(`nm`으로 확인).
2. **실행**: 링크를 고치자 4th·6th 모두 `DirectDrawCreateEx`가 SDL 창과 GL을 여는 중에 `SIGSEGV`(`si_code` 128, fault 주소 0)로 멈췄다. EIP는 i386 `libLLVM.so.21.1`(Mesa가 로드)의 `movdqa %xmm0,0x20(%esp)`였다. 16바이트로 정렬되지 않은 스택에 SSE 값을 저장하다 #GP가 난 것이다. `NativeImportGateBridge`는 게스트 스택(Win32 규약상 4바이트 정렬)에서 그대로 `NativeImportGateBridgeImpl`을 부른다. 호스트 쪽 GCC 코드는 i386 System V ABI대로 호출 시점의 16바이트 정렬을 가정한다. 반대 방향의 `CallGuestStdcallWords`는 이미 `andl $-16, %esp`로 정렬하고 있다.

*On an Ubuntu 26.04 machine (GCC 15.2, glibc 2.43, Mesa 26, NVIDIA 595) `linux-x86-debug` failed at two stages, neither seen in CI (Debian 12 container, GCC 12) or WSL (Ubuntu 24.04, GCC 13). Link: inline assembly in `native_import_bridge.cpp` and `native_process_bootstrap.cpp` and an `extern "C"` declaration cannot find `g_native_host_gs_selector`, whose definition sits `extern "C"` inside an unnamed namespace in `native_process_bootstrap.cpp`, so GCC 15.2 mangles it as `_ZN5re2dj8platform6native12_GLOBAL__N_125g_native_host_gs_selectorE` (seen with `nm`). Run: with the link fixed, 4th and 6th both stopped with `SIGSEGV` (`si_code` 128, fault address 0) while `DirectDrawCreateEx` opened the SDL window and GL, at `movdqa %xmm0,0x20(%esp)` in the i386 `libLLVM.so.21.1` Mesa had loaded — #GP from storing an SSE value on a stack not aligned to 16. `NativeImportGateBridge` calls `NativeImportGateBridgeImpl` straight on the guest stack, 4-byte aligned under the Win32 convention, while the host's GCC code assumes the i386 System V ABI's 16 bytes at a call; `CallGuestStdcallWords`, going the other way, already aligns with `andl $-16, %esp`.*

## 결정 / Decisions

1. `g_native_host_gs_selector`의 정의를 이름 없는 namespace 밖(`re2dj::platform::native` 안, `extern "C"`)으로 옮긴다. 이름 있는 namespace 안의 `extern "C"` 변수는 C 이름으로 링크된다. 초기화에 쓰는 `QueryCurrentGs`는 이름 없는 namespace에 그대로 둔다.
2. `NativeImportGateBridge`에서 호스트 GS를 적재한 뒤, 인자 두 개를 넣기 전에 `andl $-16, %esp` 하고 `subl $8, %esp` 한다. 그러면 `call` 시점의 `%esp`가 16바이트 정렬이 된다. 복귀 뒤에는 `addl $8, %esp` 대신 `leal -8(%ebp), %esp`로 저장해 둔 게스트 GS와 `%ebx` 위치로 돌아간다. 반환값 `%edx:%eax`는 건드리지 않는다.
3. 다른 진입 경로는 바꾸지 않는다. 시그널 처리기는 커널이 i386 ABI 정렬로 프레임을 만들고 `GuestSignalTrampoline`은 push/pop 균형 뒤 `jmp`하므로 정렬이 유지된다. `CallGuestEntry`·`CallGuestThread`·`CallGuestTls`는 호스트 스택에서 게스트를 부를 뿐이다. 호스트가 게스트 스택 위에서 C++를 실행하는 곳은 import 브리지 하나다.
4. Windows x86 backend는 해당하지 않는다. Win32 x86 규약은 4바이트 정렬만 보장하고 MSVC 코드도 그것을 전제한다.
5. NVIDIA 595의 32비트 `egl-wayland2`에서 Wayland EGL 창 표면이 만들어지지 않는 문제는 드라이버 쪽이다(게스트 없는 GL probe도 실패, `egl-wayland` v1로는 통과). 코드로 피하지 않고 가이드에 우회 방법(`__EGL_EXTERNAL_PLATFORM_CONFIG_FILENAMES` 또는 `SDL_VIDEO_DRIVER=x11`)을 적는다.

*1. Move the definition of `g_native_host_gs_selector` out of the unnamed namespace (into `re2dj::platform::native`, still `extern "C"`), where an `extern "C"` variable links under its C name; `QueryCurrentGs`, used to initialise it, stays in the unnamed namespace. 2. In `NativeImportGateBridge`, after loading the host GS and before pushing the two arguments, `andl $-16, %esp` and `subl $8, %esp`, so `%esp` is 16-byte aligned at the `call`; afterwards `leal -8(%ebp), %esp` replaces `addl $8, %esp` to return to the saved guest GS and `%ebx`, leaving the `%edx:%eax` result untouched. 3. Other entries stay: the kernel builds signal frames with the i386 ABI alignment and `GuestSignalTrampoline` keeps it through balanced push/pop and a `jmp`, while `CallGuestEntry`, `CallGuestThread` and `CallGuestTls` only call the guest from the host stack; the import bridge is the one place host C++ runs on the guest stack. 4. The Windows x86 backend is unaffected: the Win32 x86 convention guarantees only 4-byte alignment and MSVC code assumes no more. 5. Wayland EGL window surfaces failing under NVIDIA 595's 32-bit `egl-wayland2` is a driver matter (the guest-free GL probe fails too, and passes with `egl-wayland` v1), so it is not worked around in code; the guide gives `__EGL_EXTERNAL_PLATFORM_CONFIG_FILENAMES` or `SDL_VIDEO_DRIVER=x11`.*

## 검증 / Verification

- Ubuntu 26.04 GCC 15.2 `linux-x86-debug`(경고를 오류로) 빌드와 CTest, `linux-x86-release` 빌드와 NEEDED, x86 GL probe, x86 4th·6th(런처→자식) 실행, x86 게임패드 핫플러그.
- x64 빌드는 이 두 파일을 컴파일하지 않으므로 영향이 없다. CI(GCC 12, x86 컨테이너)는 push 뒤에 확인한다.

*Build `linux-x86-debug` (warnings as errors) with Ubuntu 26.04's GCC 15.2 and run CTest; build `linux-x86-release` and check NEEDED; run the x86 GL probe, 4th and 6th (launcher → child) on x86, and gamepad hot-plug on x86. The x64 build does not compile these two files; CI (GCC 12, the x86 container) is checked after the push.*
