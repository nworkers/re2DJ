# 작업 로그 316 — Linux guest memory cross-import persistence / Work log 316 — Linux guest memory cross-import persistence

## 결과 / Result

Linux 전용 native IPC probe가 첫 import gate에서 만든 8 KiB anonymous mapping을 completion 뒤에도 유지하고, 두 번째 import gate에서 같은 32비트 guest base로 pattern을 다시 읽도록 변경했습니다. 두 번째 gate에서 exact-base free를 수행한 뒤 read가 오류 packet으로 실패하는 것도 확인합니다.

*The Linux-only native IPC probe now retains the 8 KiB anonymous mapping created at the first import gate after completion and reads its pattern again through the same 32-bit guest base at the second import gate. It also verifies exact-base free and rejected read through an error packet at the second gate.*

이 결과는 helper의 mapping registry가 pending stack처럼 completion 시 지워지지 않고 helper process 수명 동안 유지된다는 것을 synthetic fixture에서 확인합니다. guest code에 allocation을 전달하거나 Win32 allocator API를 구현한 것은 아닙니다.

*This confirms through the synthetic fixture that the helper mapping registry is not cleared on completion like the pending stack and remains for the helper-process lifetime. It does not pass an allocation to guest code or implement a Win32 allocator API.*

## 검증 / Verification

- WSL2 Ubuntu 24.04에서 `bash scripts/test_linux_native_helper_probe.sh` 실행
- Linux x64 Debug CTest 1/1 통과 및 x64 host/i386 helper probe 통과
- Linux i386 helper ELF32 build 통과
- Linux x86 Debug CTest 1/1 통과 및 x86 host/동일 helper probe 통과
- 기존 fault signal 4 및 terminal stop fixture 통과
- `git diff --check` 통과

*Ran `bash scripts/test_linux_native_helper_probe.sh` on WSL2 Ubuntu 24.04. Linux x64 Debug CTest passed 1/1 and the x64 host/i386-helper probe passed. The Linux i386 helper built as ELF32. Linux x86 Debug CTest passed 1/1 and its host passed against the same helper. Existing fault-signal-4 and terminal-stop fixtures passed, and `git diff --check` passed.*

## 다음 단계 / Next step

실제 원본 import 관찰에 근거해 shared Win32 memory policy를 설계할 때, 이 persistent mapping을 `LocalAlloc` 또는 `VirtualAlloc` 계열의 확인된 의미에 연결합니다. reserve/commit, requested base, partial protect는 그때 별도 검증과 함께 추가합니다.

*When shared Win32 memory policy is designed from actual original-import observations, connect this persistent mapping to confirmed `LocalAlloc` or `VirtualAlloc` semantics. Add reserve/commit, requested base, and partial protection then with separate verification.*
