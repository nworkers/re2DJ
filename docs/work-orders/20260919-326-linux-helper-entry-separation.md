# 작업 326: Linux helper process-entry 분리 / Task 326: Linux helper process-entry separation

## 목표 / Goal

Linux i386 helper의 process entry를 runtime body에서 분리해 단일 프로세스 backend 추출을 위한 첫 구조 경계를 만든다.

*Separate Linux i386 helper process entry from its runtime body to establish the first structural boundary for in-process backend extraction.*

## 완료 기준 / Completion criteria

- 새 entry source는 i386 assertion과 runtime-body 호출만 가진다.
- runtime body는 기존 helper 종료 상태를 그대로 반환한다.
- helper executable 이름과 protocol은 변하지 않는다.
- 전체 Linux helper probe가 x64/x86에서 통과한다.

*The new entry source contains only the i386 assertion and runtime-body call. Runtime body returns existing helper exit statuses unchanged. Helper executable name and protocol remain unchanged. Full Linux helper probe passes on x64/x86.*
