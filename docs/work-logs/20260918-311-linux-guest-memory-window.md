# 작업 로그 311 — Linux guest memory transport 확장

## 결과

Linux x64·x86 product host가 공용 i386 helper의 현재 PE image와 pending import stack을 읽고 쓸 수 있도록 guest-memory transport를 확장했다. protocol v3 packet layout과 version은 바꾸지 않았고, memory transfer 상한을 protocol header의 공용 상수 64KiB로 고정했다.

The Linux x64/x86 product hosts can now read and write the current PE image and pending import stack through the shared i386 helper. Protocol v3 packet layout and version are unchanged; the memory-transfer limit is a shared 64 KiB constant in the protocol header.

helper는 요청 전체가 현재 image `[load_base, load_base + size_of_image)` 또는 pending stack window 안에 있을 때만 처리한다. 32비트 주소 overflow, 허용 영역 바깥 주소, 64KiB 초과 요청은 거부한다. import completion 뒤에는 stack window만 지우고 image 범위는 다음 import를 위해 유지하며, process exit·fault cleanup에서는 두 범위를 모두 지운다.

The helper accepts a request only when its complete range lies inside the current image `[load_base, load_base + size_of_image)` or the pending stack window. It rejects 32-bit address overflow, addresses outside those regions, and transfers larger than 64 KiB. After import completion it clears only the stack window so the image remains available for the next import; process-exit and fault cleanup clear both regions.

## 검증

- WSL2 Ubuntu 24.04.1에서 `bash scripts/test_linux_native_helper_probe.sh` 실행
- Linux x64 Debug build 및 CTest 1/1 통과
- Linux i386 helper build 통과
- Linux x86 Debug build 및 CTest 1/1 통과
- x64·x86 host probe가 같은 i386 helper를 실행해 image 64KiB read/write, oversized request 거부, image 바깥 주소 거부를 확인
- 두 host probe가 `result=51`, `child=0`을 보고하고 fault probe가 signal 4를 보고
- `git diff --check` 통과

The validation ran `bash scripts/test_linux_native_helper_probe.sh` under WSL2 Ubuntu 24.04.1. Linux x64 Debug and x86 Debug CTest each passed 1/1, the Linux i386 helper built successfully, and both host probes used the same helper to verify 64 KiB image read/write plus rejection of oversized and out-of-image requests. Both probes reported `result=51`, `child=0`, and the fault probe reported signal 4. `git diff --check` passed.

## 범위와 다음 단계

이번 작업은 guest-memory transport 경계만 고정했다. 공용 Win32 import dispatcher, ABI marshalling, allocator, page protection, handle registry, callback, thread, 실제 원본 실행은 아직 구현하거나 검증하지 않았다. 다음 Linux 단계는 이 transport 위에 공용 Win32 import dispatcher와 x86 ABI marshalling을 연결하는 것이다.

This task fixes only the guest-memory transport boundary. The shared Win32 import dispatcher, ABI marshalling, allocator, page protection, handle registry, callbacks, threads, and actual original-game execution remain unimplemented or unvalidated. The next Linux step is to connect the shared Win32 import dispatcher and x86 ABI marshalling on top of this transport.
