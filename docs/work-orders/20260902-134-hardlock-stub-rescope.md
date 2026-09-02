# Hardlock 스텁 재정의와 4th descriptor ID 확보 작업 지시

관련 설계: [Hardlock 우회 스텁](../design/20260901-131-hardlock-bypass-stub.md), [ez2dj3rd Hardlock 세 seed SMT 복구](../design/20260831-107-ez2dj3rd-hardlock-seed-smt.md)

*Related designs: [Hardlock bypass stub](../design/20260901-131-hardlock-bypass-stub.md) and [ez2dj3rd Hardlock three-seed SMT recovery](../design/20260831-107-ez2dj3rd-hardlock-seed-smt.md).*

## 배경

[Task 133](../work-logs/20260902-133-ez2dj4th-protection-shape.md)이 보호를 건너뛴 진입점 직행이 불가능함을 확인했으므로, "우회"라는 이름은 이 구성요소가 하지 못하는 일을 약속합니다. 동시에 [Task 107](../work-logs/20260831-107-ez2dj3rd-hardlock-seed-smt.md)은 단일 ID pair로 seed를 유일 식별할 수 없고 후보를 판별할 oracle이 필요하다고 남겼습니다. 스텁은 그 oracle의 주입 지점이므로 유지하되 성격을 정확히 다시 붙입니다.

*Task 133 confirmed that jumping past the protection to the entry point is impossible, so the name "bypass" promises something this component cannot do. At the same time Task 107 recorded that a single ID pair cannot uniquely identify the seeds and that an oracle distinguishing candidates is required. The stub is the injection point for that oracle, so it is kept and renamed to match what it actually is.*

## 범위

1. 제품 CLI와 Windows backend에서 `--hardlock-bypass`를 제거합니다. 제품 실행 경로는 합성 응답 옵션을 전혀 갖지 않습니다.
2. launcher 옵션을 `--hardlock-stub`으로, runtime export와 trace/JSONL 표기를 stub 명칭으로 바꿉니다. 플랫폼 중립 `HardlockStubDevice`와 test는 유지합니다.
3. descriptor의 `ID_Ref`/`ID_Verify`를 기록하는 명시적 진단 옵션 `--hardlock-descriptor-ids`를 추가합니다. 기본은 기존 redaction 유지입니다.
4. 4th 원본을 두 번 독립 실행해 module address와 두 ID를 확보하고 안정성을 확인합니다.
5. 문서를 갱신합니다.

*Remove `--hardlock-bypass` from the product CLI and Windows backend so the product path carries no synthetic-response option; rename the launcher option to `--hardlock-stub` along with the runtime export and trace/JSONL labels, keeping the platform-neutral `HardlockStubDevice` and its tests; add an explicit `--hardlock-descriptor-ids` diagnostic that records the descriptor `ID_Ref` and `ID_Verify` while the default redaction stays; run the 4th original twice independently to obtain the module address and both IDs and confirm stability; and update the documents.*

## 비범위

- Function `0x0e` 구현. 허용 가능한 독립 근거가 없습니다.
- 외부 seed 도구 코드의 도입. `hl_seed.c`는 라이선스 명시가 없어 사용할 수 없습니다.
- 후보 응답 주입과 판별기. 다음 작업입니다.

*Out of scope: implementing Function `0x0e`, which lacks a policy-compatible independent basis; adopting external seed-tool source, since `hl_seed.c` states no license; and candidate-response injection with its judge, which is the next task.*

## 완료 조건

- 제품 CLI `--help`와 인자 생성기에 합성 응답 옵션이 없습니다.
- `--hardlock-stub`으로 스텁이 동작하고 trace가 `hardlock-stub`으로 기록됩니다.
- 4th의 두 ID가 두 실행에서 동일하게 확인되어 분석 문서에 기록됩니다.
- Windows x86 build와 선택 CTest가 통과합니다.

*Completion requires no synthetic-response option in the product CLI `--help` or argument builder, a working `--hardlock-stub` whose trace is labeled `hardlock-stub`, both 4th IDs confirmed identical across two runs and recorded in the analysis document, and a passing Windows x86 build and selected CTest.*
