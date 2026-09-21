# 작업 318: Linux native-helper capability rejection / Task 318: Linux native-helper capability rejection

설계: [Linux native-helper capability rejection](../design/20260918-318-linux-helper-capability-rejection.md)

*Design: [Linux native-helper capability rejection](../design/20260918-318-linux-helper-capability-rejection.md)*

## 작업 / Work

1. Linux backend의 시작 협상 실패 정리를 pipe close, 제한된 child reap, 기존 강제 종료 순서로 만듭니다.
2. 필수 feature를 의도적으로 누락하는 Linux 전용 protocol fixture helper를 추가합니다.
3. Linux host probe와 WSL 검증 script가 이미지 전송 전 EOF 상태를 x64·x86에서 확인하게 합니다.
4. architecture와 Linux L2 계획을 갱신하고, 검증 결과를 작업 로그에 기록합니다.

*1. Make Linux backend startup-handshake cleanup close pipes, reap the child for a bounded time, then use the existing forced-stop path.
2. Add a Linux-only protocol fixture helper that deliberately omits required features.
3. Make the Linux host probe and WSL verification script check the pre-image-transfer EOF status on x64 and x86.
4. Update architecture and the Linux L2 plan, then record verification results in the work log.*

## 완료 기준 / Completion criteria

feature가 누락된 `HelloResult`에서 `PrepareImage()`가 실패하고 오류가 필수 기능 누락을 설명해야 합니다. fixture 상태는 `eof-before-load-image`여야 하며, Linux x64·x86의 기존 production-helper probe도 계속 통과해야 합니다.

*With a feature-missing `HelloResult`, `PrepareImage()` must fail and explain that required features are missing. The fixture status must be `eof-before-load-image`, and the existing production-helper probes on Linux x64 and x86 must continue to pass.*
