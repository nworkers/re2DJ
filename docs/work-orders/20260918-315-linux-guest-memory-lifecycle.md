# 작업 315 — Linux guest memory lifecycle / Task 315 — Linux guest memory lifecycle

설계: [Linux guest memory lifecycle contract](../design/20260918-315-linux-guest-memory-lifecycle.md).

*Design: [Linux guest memory lifecycle contract](../design/20260918-315-linux-guest-memory-lifecycle.md).*

## 작업 / Work

1. `ExecutionBackend`에 guest memory access와 allocate/protect/free 연산을 추가하고, test fake와 Linux backend 선언을 맞춥니다.
2. protocol v4에 고정 폭 lifecycle packet을 추가합니다.
3. i386 helper에 anonymous mapping registry, access 검증, `mmap`/`mprotect`/`munmap` 처리와 종료 정리를 추가합니다.
4. Linux host backend가 pending import 상태에서 lifecycle 요청과 결과 검증을 수행하게 합니다.
5. native host probe에 allocate, protection, free lifecycle fixture를 추가합니다.
6. 아키텍처 문서와 작업 로그에 구현 상태 및 검증 결과를 기록합니다.

*1. Add guest-memory access plus allocate/protect/free operations to `ExecutionBackend`, and update test fakes and the Linux backend declaration.
2. Add fixed-width lifecycle packets to protocol v4.
3. Add an anonymous-mapping registry, access validation, `mmap`/`mprotect`/`munmap`, and exit cleanup to the i386 helper.
4. Make the Linux host backend issue lifecycle requests and validate their results only while an import is pending.
5. Add an allocation/protection/free lifecycle fixture to the native host probe.
6. Record implementation status and verification in architecture documentation and the work log.*

## 완료 기준 / Completion criteria

- 동적 mapping은 32비트 주소와 page-rounded size로만 host에 반환됩니다.
- read-only 보호 뒤 write는 실패하고 helper fault나 protocol desynchronization을 만들지 않습니다.
- free 뒤 read/write가 거부되며, image와 pending stack의 기존 동작은 유지됩니다.
- Linux x64/x86 host가 동일 i386 helper against probe를 통과합니다.

*Dynamic mappings return only a 32-bit address and page-rounded size to the host. Writes after read-only protection fail without a helper fault or protocol desynchronization. Reads/writes after free are rejected while existing image and pending-stack behavior remains intact. Linux x64/x86 hosts pass the probe against the same i386 helper.*

## 검증 / Verification

- `bash scripts/test_linux_native_helper_probe.sh` under WSL Ubuntu 24.04
- `git diff --check`

*Run `bash scripts/test_linux_native_helper_probe.sh` under WSL Ubuntu 24.04 and `git diff --check`.*
