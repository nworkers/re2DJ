# 작업 328: Linux native PE session 분리 / Task 328: Linux native PE session extraction

## 목표 / Goal

helper의 PE 실행 상태를 `NativePeSession`으로 추출해 protocol adapter와 실행 수명주기를 분리한다.

*Extract helper PE execution state into `NativePeSession`, separating protocol adapter from execution lifecycle.*

## 완료 기준 / Completion criteria

- session이 prepare, TLS, entry, release를 소유한다.
- helper가 session을 통해 기존 synthetic PE를 실행한다.
- x64/x86 full helper probe가 통과한다.

*The session owns prepare, TLS, entry, and release. The helper runs the existing synthetic PE through session. Full x64/x86 helper probe passes.*
