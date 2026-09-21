# 작업 317 — Linux native-helper capability handshake / Task 317 — Linux native-helper capability handshake

설계: [Linux native-helper capability handshake](../design/20260918-317-linux-helper-capability-handshake.md).

*Design: [Linux native-helper capability handshake](../design/20260918-317-linux-helper-capability-handshake.md).*

## 작업 / Work

1. protocol v5에 feature bit, `Hello`, `HelloResult` packet을 추가합니다.
2. Linux backend가 helper launch 직후 required/optional feature를 보내고 결과를 검증하게 합니다.
3. i386 helper가 `LoadImage` 전에 handshake를 처리하고 supported feature를 반환하게 합니다.
4. architecture, L2 계획, 작업 로그를 갱신하고 Linux x64/x86 통합 검증을 수행합니다.

*1. Add feature bits and `Hello`/`HelloResult` packets to protocol v5.
2. Make the Linux backend send required/optional features immediately after helper launch and validate the result.
3. Make the i386 helper process the handshake before `LoadImage` and return supported features.
4. Update architecture, the L2 plan, and the work log, then run Linux x64/x86 integration validation.*

## 완료 기준 / Completion criteria

host는 모든 required bit를 가진 `HelloResult`를 받기 전에는 image bytes를 전송하지 않아야 합니다. 동일 helper를 사용하는 Linux x64/x86 host는 기존 synthetic fixture를 모두 통과해야 합니다.

*The host must not send image bytes before receiving a `HelloResult` containing every required bit. Linux x64/x86 hosts using the same helper must pass all existing synthetic fixtures.*
