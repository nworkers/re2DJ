# Linux helper process-entry 분리 / Linux helper process-entry separation

## 목적 / Purpose

단일 프로세스 backend 전환 전에 Linux i386 helper의 process entry와 runtime body를 분리한다. 현재 `main()`은 protocol handshake, PE session 생성, guest 실행까지 직접 소유한다. entry wrapper는 process 전용으로 두고 body를 호출 가능한 함수로 만들면, 다음 작업에서 protocol adapter와 native execution session을 독립적으로 추출할 수 있다.

*Before transitioning to an in-process backend, separate Linux i386 helper process entry from its runtime body. The current `main()` directly owns protocol handshake, PE session creation, and guest execution. Keeping entry wrapper process-specific and making the body callable lets the next task extract protocol adapter and native execution session independently.*

## 범위 / Scope

`RunNativeIpcHelper()`가 기존 helper body를 소유하고, 새 작은 `main` translation unit이 i386 assertion 후 이를 호출한다. protocol wire format, PE mapping, import-thunk ABI, bootstrap, memory protection, CLI와 helper executable name은 바꾸지 않는다.

*`RunNativeIpcHelper()` owns the existing helper body, while a new small `main` translation unit performs the i386 assertion and calls it. Do not change protocol wire format, PE mapping, import-thunk ABI, bootstrap, memory protection, CLI, or helper executable name.*

## 검증 / Validation

기존 `bash scripts/test_linux_native_helper_probe.sh`를 실행하여 x64/x86 host가 같은 ELF32 helper와 normal import, fault, stop, capability rejection을 모두 통과하는지 확인한다.

*Run existing `bash scripts/test_linux_native_helper_probe.sh` to verify x64/x86 hosts against the same ELF32 helper through normal import, fault, stop, and capability rejection.*
