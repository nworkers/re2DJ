# 작업 311 — Linux guest memory transport 확장

## 목적

Linux x64·x86 product host와 i386 helper 사이의 guest memory transport를 4KiB stack window에서 64KiB image/stack 범위로 확장합니다. 설계는 [Linux guest memory transport 확장](../design/20260918-311-linux-guest-memory-window.md)을 따릅니다.

*Purpose*

Expand guest-memory transport between the Linux x64/x86 product hosts and the i386 helper from a 4KiB stack window to a 64KiB image/stack range. Follow the [Linux guest memory transport design](../design/20260918-311-linux-guest-memory-window.md).

## 작업

1. protocol header에 memory transfer 상한을 추가합니다.
2. Linux host backend가 새 상한을 사용하도록 변경합니다.
3. i386 helper가 현재 image와 pending stack을 허용 영역으로 검증하도록 변경합니다.
4. 기존 synthetic native probe에 image read/write와 경계 검사를 추가합니다.
5. TODO, 아키텍처와 작업 로그를 갱신하고 Linux x64/x86을 재검증합니다.

*Tasks*

Add a shared memory-transfer limit, use it in the Linux host backend, validate the current image and pending stack as the helper's allowed regions, extend the synthetic native probe with image read/write and boundary checks, update TODO/architecture/work-log, and revalidate Linux x64/x86.

## 검증

* Linux x64 configure/build/CTest
* Linux x86 configure/build/CTest
* Linux i386 helper build
* x64 및 x86 host probe against the same helper
* `git diff --check`

*Verification*

Run Linux x64 and x86 configure/build/CTest, build the i386 helper, run both host probes against that helper, and run `git diff --check`.
