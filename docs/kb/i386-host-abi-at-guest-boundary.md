# 게스트 경계에서의 i386 호스트 ABI / The i386 host ABI at the guest boundary

Linux i386 프로세스 안에서 Win32 게스트 코드와 호스트(Linux) 코드가 같은 스레드·스택을 번갈아 쓸 때 지켜야 하는 호스트 쪽 규약을 정리한다. 근거 작업은 [작업 459](../work-logs/20261005-459-linux-x86-gcc15-host-abi.md)다.

*What the host side must keep when Win32 guest code and Linux host code take turns on the same thread and stack inside an i386 process. The evidence is [task 459](../work-logs/20261005-459-linux-x86-gcc15-host-abi.md).*

## 스택 정렬 / Stack alignment

- **i386 System V ABI**(Linux)는 `call` 직전 `%esp`가 16바이트 정렬이기를 요구한다. 따라서 함수 진입 시 `(%esp + 4) % 16 == 0`이다. GCC의 기본값(`-mpreferred-stack-boundary=4`)이 이 가정 위에서 코드를 만든다. [i386 psABI](https://gitlab.com/x86-psABIs/i386-ABI), [GCC x86 options](https://gcc.gnu.org/onlinedocs/gcc/x86-Options.html)
- **Win32 x86**은 스택을 4바이트 정렬로만 유지한다. GCC 문서는 `-mstackrealign`을 "4바이트 정렬을 유지하는 레거시 코드와 SSE 호환을 위해 16바이트를 유지하는 최신 코드를 섞기 위한" 옵션으로 설명한다(같은 문서).
- 그래서 게스트가 호스트 함수를 부르는 경계(import thunk → 호스트 처리기)는 호스트 코드를 부르기 전에 스택을 직접 정렬해야 한다(`andl $-16, %esp`). 반대 방향(호스트 → 게스트)은 게스트가 정렬을 요구하지 않지만 re2DJ는 같은 방식으로 정렬한다.
- 정렬이 어긋난 채 호스트 라이브러리가 SSE 스택 저장(`movdqa`, `movaps`)을 하면 #GP가 나고, 프로세스에는 `SIGSEGV`가 `si_code = SI_KERNEL`(128), fault 주소 0으로 온다(**관찰**: Ubuntu 26.04 i386 `libLLVM.so.21.1`의 `movdqa %xmm0,0x20(%esp)`). 어떤 라이브러리가 그런 명령을 쓰는지는 배포판 빌드에 달려 있다. 그래서 같은 코드가 한 환경(WSL Ubuntu 24.04)에서는 돌고 다른 환경에서는 죽을 수 있다.
- 시그널 처리기는 해당하지 않는다. 커널이 i386 시그널 프레임을 처리기 진입 시점의 ABI 정렬에 맞춰 놓는다(`arch/x86/kernel/signal.c`의 `align_sigframe`). [Linux source](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/signal.c)

*The i386 System V ABI (Linux) requires `%esp` to be 16-byte aligned right before a `call`, so `(%esp + 4) % 16 == 0` at function entry, and GCC's default `-mpreferred-stack-boundary=4` generates code on that assumption ([i386 psABI](https://gitlab.com/x86-psABIs/i386-ABI), [GCC x86 options](https://gcc.gnu.org/onlinedocs/gcc/x86-Options.html)). Win32 x86 keeps the stack only 4-byte aligned; GCC's documentation describes `-mstackrealign` as supporting the mix of legacy code keeping 4-byte alignment with modern code keeping 16 bytes for SSE. A boundary where the guest calls the host (import thunk → host handler) must therefore align the stack itself (`andl $-16, %esp`) before host code runs; the other direction needs no alignment for the guest, but re2DJ aligns the same way. A host library storing SSE registers on a misaligned stack (`movdqa`, `movaps`) raises #GP, which reaches the process as `SIGSEGV` with `si_code = SI_KERNEL` (128) and fault address 0 (observed: `movdqa %xmm0,0x20(%esp)` in Ubuntu 26.04's i386 `libLLVM.so.21.1`). Whether a library uses such instructions depends on the distribution's build, so the same code can run in one environment (WSL Ubuntu 24.04) and die in another. Signal handlers are not affected: the kernel lays out the i386 signal frame with the ABI alignment at handler entry (`align_sigframe` in `arch/x86/kernel/signal.c`, [Linux source](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/signal.c)).*

## 어셈블리가 참조하는 C 이름 / C names referenced from assembly

- C++에서 이름 없는 namespace 안의 이름은 `extern "C"`로 감싸도 내부 링크(internal linkage)를 갖는다. [C++ [basic.link]](https://eel.is/c++draft/basic.link)
- **관찰**: GCC 15.2는 이름 없는 namespace 안에서 `extern "C"`로 정의한 변수를 `_ZN…12_GLOBAL__N_1…E`처럼 맹글링한다. 다른 번역 단위의 `extern "C"` 선언과 인라인 어셈블리의 `g_native_host_gs_selector` 참조는 C 이름을 찾으므로 링크가 실패한다. 같은 코드가 CI의 GCC 12와 WSL의 GCC 13에서는 링크됐으므로, 그 버전들은 C 이름을 그대로 냈던 것으로 **추정**한다.
- 어셈블리나 다른 번역 단위가 이름으로 찾는 정의는 이름 없는 namespace 밖에 둔다.

*In C++ a name inside an unnamed namespace has internal linkage even inside `extern "C"` ([C++ [basic.link]](https://eel.is/c++draft/basic.link)). Observed: GCC 15.2 mangles a variable defined `extern "C"` inside an unnamed namespace as `_ZN…12_GLOBAL__N_1…E`, so another translation unit's `extern "C"` declaration and inline assembly referring to `g_native_host_gs_selector` look for the C name and the link fails; the same code linked with CI's GCC 12 and WSL's GCC 13, so those versions are inferred to have emitted the plain C name. Keep definitions that assembly or other translation units find by name outside unnamed namespaces.*
