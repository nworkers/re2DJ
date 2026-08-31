# ez2dj3rd Hardlock 세 seed SMT 복구 작업 지시

관련 설계: [ez2dj3rd Hardlock 세 seed SMT 복구](../design/20260831-107-ez2dj3rd-hardlock-seed-smt.md)

*Related design: [ez2dj3rd Hardlock three-seed SMT recovery](../design/20260831-107-ez2dj3rd-hardlock-seed-smt.md).*

## 범위

1. 이전 세션과 저장소에서 SMT 제약식·결과가 보존됐는지 확인합니다.
2. 두 번 관찰한 ID pair를 Task 106 누적 문서에 반영합니다.
3. E-Y-E 관계식의 독립 근거나 허용 라이선스 출처를 찾고 byte order와 연산 폭을 기록합니다. 사용자가 승인한 일회성 예외는 저장소 밖 임시 GPL 자료 확인에만 적용합니다.
4. 근거가 확보되면 세 16-bit seed를 사용하는 SMT-LIB2 생성기와 독립 scalar 검증기를 추가합니다.
5. Z3로 해를 열거하고 입력 pair 재생성 및 가능한 Function `0x0e` 원본 실행으로 검증합니다.
6. 분석 문서, 작업 로그, TODO를 결과에 맞게 갱신하고 범위에 맞는 테스트와 `git diff --check`를 수행합니다.

*Check whether prior SMT constraints or results survived; incorporate the twice-observed ID pair into Task 106; establish an independent or permissively licensed basis for any repository implementation while using the user-approved one-off exception only to inspect temporary GPL material outside the repository; record byte order and operation widths; only then add a clean repository SMT-LIB2 generator and independent scalar evaluator for three 16-bit seeds; enumerate with Z3 and validate by pair regeneration and, if possible, original Function `0x0e` execution; update cumulative documents and run scoped tests plus `git diff --check`.*

## 완료 조건

- 저장소 구현이 있다면 관계식 출처와 라이선스가 프로젝트 정책에 맞습니다. 분석 전용 예외만 사용했다면 임시 경계와 제거 결과를 기록합니다.
- solver의 모든 보고 후보가 독립 scalar 관계 검증을 통과합니다.
- 해가 없거나 여러 개인 경우도 도구 버전, 입력, 검증 상태와 함께 명시적으로 기록합니다.
- 원본 자산, 전체 descriptor, GPL 구현은 저장소에 포함되지 않습니다.
- 실제 원본 경계 검증 전에는 seed 후보를 확정값으로 기술하지 않습니다.

*Completion requires a policy-compatible source for any repository implementation, or a recorded and cleaned temporary boundary when only the analysis exception is used; every reported model passes an independent scalar relation check; unsatisfiable or multiple-model outcomes are recorded with tool version, inputs, and validation state; no original assets, complete descriptor, or GPL implementation enter the repository; and candidates remain unconfirmed until original-boundary verification.*
