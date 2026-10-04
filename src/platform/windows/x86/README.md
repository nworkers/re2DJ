# src/platform/windows/x86

Windows x86(64비트 Windows의 WOW64에서 도는 32비트 제품) 전용 코드입니다.

*Code for Windows x86 only, the 32-bit product running under WOW64 on 64-bit Windows.*

- **in-process backend(작업 448)**: 공용 러너 [`../../native/`](../../native/README.md)의 backend 계약을 Windows에서 구현합니다. 게스트는 자기를 실행하는 host 스레드의 스택과 실제 TEB(FS)에서 돌고, HLE는 스레드마다 둔 그림자 TEB를 봅니다. import bridge가 fs:0(SEH 체인)을 그림자 TEB와 주고받습니다. fault는 VEH로 받고, 게스트 SEH 배달은 VEH가 게스트를 `GuestFaultTrampoline`으로 돌려 스레드별 배달 스택에서 합니다. asm은 MSVC naked 함수입니다. 근거: [작업 448 설계](../../../../docs/design/20261004-448-windows-x86-backend.md).
  - `native_guest_transition.*`: 진입·탈출(`EnterGuestRun`·`EscapeGuestRun`), entry·ThreadProc·TLS callback 호출, host→게스트 stdcall.
  - `native_import_bridge.cpp`, `native_process_bootstrap.cpp`, `native_low_memory.cpp`, `native_in_process_probe.cpp`.

*The in-process backend (task 448) implements the shared runner's backend contracts on Windows: the guest runs on the stack and real TEB (FS) of the host thread executing it while the HLE sees a shadow TEB per thread, the import bridge moves fs:0 (the SEH chain) to and from the shadow, faults arrive through a VEH, and guest SEH delivery happens on a per-thread delivery stack after the VEH sends the guest to `GuestFaultTrampoline`; the asm is MSVC naked functions. See the [task 448 design](../../../../docs/design/20261004-448-windows-x86-backend.md).*
