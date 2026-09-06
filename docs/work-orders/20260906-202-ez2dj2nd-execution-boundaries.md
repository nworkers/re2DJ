# ez2dj2nd 실행 경계 보정 작업 지시서

## 목적

2nd 실행이 즉시 종료되는 launcher 준비 실패와 다음 privileged I/O fault를 제거하고, D3D HLE IAT 패치 정책을 타깃별로 분리합니다.

## 선행 설계

- `docs/design/20260906-202-ez2dj2nd-execution-boundaries.md`
- `docs/analysis/ez2dj-exe-structures.md`
- `docs/analysis/ez2dj-demo-volume.md`
- `docs/analysis/ez2dj-io-map.md`

## 작업 항목

1. `ez2dj2nd`에서 demo-volume 기본 주입을 제거합니다.
2. 2nd에서 실행으로 확인된 legacy input helper RVA `0x000782d7`를 프로파일에 반영합니다.
3. `DirectDrawCreateEx` IAT patch를 `ez2dj4th`에만 예외 적용하고, 2nd를 포함한 나머지 타깃에는 수행합니다.
4. launcher diagnostic에 준비 단계 상태와 빈 오류를 위한 fallback 원인을 남깁니다.
5. 프로파일 단위 테스트와 Windows x86 실행 검증을 수행합니다.
6. 누적 분석 문서, 실행 파일 설계 문서, architecture 문서를 갱신하고 작업 로그를 작성합니다.

## 제외 범위

- 원본 HDD와 실행 파일 수정 또는 저장소 반입
- 2nd Hardlock `id_ref`/`id_verify` 추출
- 런타임 관찰 전 2nd output RVA를 사실로 확정
- 게임 로직 또는 Direct3D 렌더링 로직 재구현

## 완료 조건

- 2nd 프로파일이 demo-volume 없이 구성되고 관찰된 input/output helper RVA를 사용합니다.
- 4th 이외 타깃의 `DirectDrawCreateEx` IAT가 runtime HLE thunk로 연결됩니다.
- 준비 실패 시 diagnostic JSONL과 stderr에 원인이 남습니다.
- Windows x86 build, unit test, 2nd 실행 검증이 완료됩니다.

---

# ez2dj2nd Execution-Boundary Correction Work Order

## Objective

Remove the launcher preparation failure and the next privileged I/O fault that stop 2nd execution immediately, and separate the D3D HLE IAT policy by target.

## Work items

1. Remove the default demo-volume injection from `ez2dj2nd`.
2. Apply the runtime-confirmed 2nd legacy input helper RVA `0x000782d7`.
3. Make `DirectDrawCreateEx` IAT patching exceptional only for `ez2dj4th`; patch other targets including 2nd.
4. Record preparation-stage statuses and a fallback reason when the launcher error is empty.
5. Run profile tests and Windows x86 execution verification.
6. Update cumulative analysis, executable-design, architecture, and work-log documents.

## Out of scope

- Modifying or storing original HDD assets or executables
- Extracting 2nd Hardlock `id_ref`/`id_verify`
- Treating the 2nd output RVA as confirmed before runtime observation
- Reimplementing game or Direct3D rendering logic

## Completion criteria

- The 2nd profile has no default demo-volume injection and uses the observed input and output helper RVAs.
- `DirectDrawCreateEx` IAT is connected to the runtime HLE thunk for non-4th targets.
- Diagnostic JSONL and stderr identify preparation failures.
- Windows x86 build, unit tests, and 2nd execution verification are complete.

## Runtime evidence update

후속 진단 실행에서 2nd의 `OUT DX,AL` helper RVA `0x0007832b`를 확인했으므로 프로파일에 반영합니다. 이 값은 더 이상 미확정 범위가 아닙니다.

The follow-up diagnostic run confirmed the 2nd `OUT DX,AL` helper RVA as `0x0007832b`, so it is applied to the profile. The final product-loader contract also covers the 4th profile's DirectSound argument ordering.
