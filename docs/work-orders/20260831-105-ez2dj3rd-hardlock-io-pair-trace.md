# ez2dj3rd Hardlock 입출력 쌍 추적 작업 지시

## 목표

3rd `EZ2DJ.EXE`의 런타임 보호 코드에서 `Function 0x0e` 입력과 반환 후 사용되는 출력/평문 후보를 재현 가능하게 확인합니다.

*Reproducibly identify the `Function 0x0e` inputs and the output/plaintext candidates consumed by the runtime protection code in the 3rd `EZ2DJ.EXE`.*

## 작업

1. 디스크상 `.protect`와 런타임 코드의 차이를 확인합니다.
2. post-IOCTL trace가 정적 `ExitProcess` import 없이 bounded 실행되도록 진단 fallback을 구현합니다.
3. `0x9c402458` 이후 명령을 추적하고 output buffer의 읽기·복사·분기 사용을 확인합니다.
4. 입력별 출력 또는 평문 후보와 미확정 사항을 누적 분석 문서에 반영합니다.
5. 빌드와 CTest로 회귀를 확인합니다.

*Confirm the difference between the on-disk `.protect` bytes and runtime code; add a bounded diagnostic fallback for post-IOCTL tracing without a static `ExitProcess` import; trace instructions after `0x9c402458` and identify reads, copies, and branches involving the output buffer; update cumulative analysis with output/plaintext candidates and unresolved points; and run build plus CTest regression verification.*

## 완료 조건

- 3rd post-IOCTL trace가 정적 `ExitProcess` import 부재로 준비 단계에서 실패하지 않습니다.
- 최소 한 개 이상의 `Function 0x0e` 8바이트 입력에 대해 반환 이후 사용 경로가 기록됩니다.
- 확인됨·추정·미확정 상태를 구분한 분석 문서와 작업 로그가 남습니다.

*Completion requires that 3rd post-IOCTL tracing no longer fails during preparation solely because `ExitProcess` is not statically imported, that the post-return use path is recorded for at least one eight-byte `Function 0x0e` input, and that analysis and work-log documents distinguish confirmed, inferred, and unresolved findings.*

## 후속 결과 / Follow-up result

복원 가능한 wrapper 코드는 분석했지만 유효 response 생성이나 평문 비교 경로는 발견되지 않았습니다. 런타임 회귀 확인을 위해 synthetic device IOCTL 진입 로그를 추가하고, 응답을 추측하는 실험 분기는 제거합니다. 유효 입출력 표는 실제 3rd Hardlock 응답 또는 검증된 seed가 필요한 외부 입력으로 남깁니다.

*The recoverable wrapper code was analyzed, but no valid-response generator or plaintext comparison path was found. A synthetic-device IOCTL-entry log is retained for runtime regression evidence, while experimental guessed-response branches are removed. A valid input/output table remains dependent on real 3rd Hardlock responses or independently verified seeds.*
