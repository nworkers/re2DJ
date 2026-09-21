# 작업 327: Linux native PE image 수명주기 분리 / Task 327: Linux native PE image lifecycle extraction

## 목표 / Goal

helper 내부 PE mapping과 relocation 수명주기를 전용 Linux platform component로 추출한다.

*Extract helper-internal PE mapping and relocation lifecycle into a dedicated Linux platform component.*

## 완료 기준 / Completion criteria

- helper가 새 component로 같은 synthetic PE를 mapping하고 실행한다.
- map 실패가 mapping을 남기지 않는다.
- x64/x86 full helper probe가 통과한다.
- protocol 및 original target behavior 변경 주장을 하지 않는다.

*The helper maps and runs the same synthetic PE through the new component. A failed map leaves no mapping. The x64/x86 full helper probe passes. Make no claim of protocol or original-target behavior change.*
