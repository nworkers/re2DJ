# ez2dj3rd Hardlock 세 seed SMT 복구 작업 로그

관련 설계: [ez2dj3rd Hardlock 세 seed SMT 복구](../design/20260831-107-ez2dj3rd-hardlock-seed-smt.md)

*Related design: [ez2dj3rd Hardlock three-seed SMT recovery](../design/20260831-107-ez2dj3rd-hardlock-seed-smt.md).*

## 결과

- 두 번 확인된 `ID_Ref=478c8b793f201f8a`, `ID_Verify=cc22ae2da344b2a2`를 사용했습니다.
- 공식 Z3 5.1.0 Windows x64 배포판을 시스템 임시 디렉터리에서 실행했습니다. Z3는 MIT 라이선스입니다.
- 사용자가 승인한 예외에 따라 GPL 분석 자료는 저장소 밖 임시 분석에만 사용했습니다.
- 첫 SMT 단계는 유일한 control bytes `74 6c 2c 1c f0`를 얻었고 배제 제약은 `unsat`였습니다.
- seed 단계의 순수 SMT는 전체 탐색에 느렸으나, 발견 후보를 고정한 검증에서는 `sat`를 반환했습니다.
- scalar evaluator와 32-lane AVX2 evaluator를 128개 synthetic 후보에서 교차 검증한 뒤 후보 탐색에 사용했습니다.
- 완전 열거 전에 서로 다른 후보 11개가 발견됐고 모두 scalar shape/relation 검증을 통과했습니다. 따라서 단일 ID pair로 실제 seed를 유일하게 식별할 수 없습니다.
- 임시 분석 source·binary·SMT와 Z3 배포 디렉터리는 확인된 시스템 임시 경로에서 제거했습니다. 원본 자산과 분석 도구는 커밋하지 않았습니다.

*The analysis used the twice-confirmed `ID_Ref=478c8b793f201f8a` and `ID_Verify=cc22ae2da344b2a2`. Official MIT-licensed Z3 5.1.0 for Windows x64 ran from the system temporary directory. Under the user's exception, GPL analysis material was used only temporarily outside the repository. The first SMT stage produced the unique control bytes `74 6c 2c 1c f0`, with the exclusion query returning `unsat`. Pure SMT was slow for open seed search but returned `sat` for fixed discovered candidates. A scalar evaluator and 32-lane AVX2 evaluator agreed on 128 synthetic candidates before search. Eleven distinct candidates were found and scalar-verified before exhaustive enumeration was stopped, proving that one ID pair cannot uniquely identify the physical seeds. Temporary analysis source, binaries, SMT files, and the Z3 distribution were removed from the verified system-temporary path; no analysis tool or original asset is committed.*

## 검증

- Z3 stage 1: `sat`, 고유 해 배제: `unsat`
- 대표 seed 후보 2개 Z3 고정 검증: 각각 `sat`
- 후보 11개 scalar 검증: 모두 `shape=1`, `relation=1`
- AVX2/scalar 교차 검증: 128/128 일치
- `git diff --check`

*Verification: stage 1 is satisfiable and its unique-model exclusion is unsatisfiable; two representative fixed seed models are satisfiable in Z3; all eleven candidates report scalar `shape=1` and `relation=1`; AVX2 and scalar evaluation agree for 128/128 test candidates; and `git diff --check` is run.*

## 다음 입력

두 번째 독립 E-Y-E challenge/response pair 또는 후보별 Function `0x0e` 응답을 원본 실행에서 판별할 oracle이 필요합니다. 그 전에는 11개 후보 중 어느 것도 실제 seed로 확정하지 않습니다.

*A second independent E-Y-E challenge/response pair, or an original-execution oracle that distinguishes candidate Function `0x0e` outputs, is required. None of the eleven candidates is identified as the physical seed set before that evidence exists.*
