# 작업 로그 317 — Linux native-helper capability handshake / Work log 317 — Linux native-helper capability handshake

## 결과 / Result

native-helper protocol을 v5로 올리고 `Hello`/`HelloResult` startup packet을 추가했습니다. Linux backend는 helper fork 뒤 import metadata, bounded memory transfer, guest-memory lifecycle bit를 required set으로 보내며, response가 이 bit를 모두 제공하지 않으면 image payload를 보내지 않고 실패합니다.

*The native-helper protocol is now v5 with `Hello`/`HelloResult` startup packets. After forking the helper, the Linux backend sends import-metadata, bounded-memory-transfer, and guest-memory-lifecycle bits as its required set; if the response lacks any of them, it fails without sending an image payload.*

i386 helper는 `LoadImage` 전에 Hello packet의 layout과 required feature를 검사하고 supported set을 반환합니다. header version이 다른 peer는 기존 header validation에서 거부되며, 같은 version이라도 helper가 지원하지 않는 required bit는 오류 packet으로 거부됩니다.

*Before `LoadImage`, the i386 helper validates the Hello packet layout and required features, then returns its supported set. A peer with another header version remains rejected by existing header validation; at the same version, an unsupported required bit is rejected through an error packet.*

이 handshake는 future optional feature를 위한 compatibility boundary이며, callback, thread, dynamic import나 Win32 API 기능을 새로 구현하지 않습니다.

*This handshake is a compatibility boundary for future optional features. It does not add callbacks, threads, dynamic imports, or Win32 API functionality.*

## 검증 / Verification

- WSL2 Ubuntu 24.04에서 `bash scripts/test_linux_native_helper_probe.sh` 실행
- Linux x64 Debug CTest 1/1, i386 helper ELF32 build, x64 host probe 통과
- Linux x86 Debug CTest 1/1 및 같은 helper를 쓰는 x86 host probe 통과
- 기존 memory lifecycle, fault signal 4, terminal stop fixture 통과
- `git diff --check` 통과

*Ran `bash scripts/test_linux_native_helper_probe.sh` on WSL2 Ubuntu 24.04. Linux x64 Debug CTest passed 1/1, the i386 helper built as ELF32, and the x64 host probe passed. Linux x86 Debug CTest passed 1/1 and its host probe passed against the same helper. Existing memory-lifecycle, fault-signal-4, and terminal-stop fixtures passed, and `git diff --check` passed.*
