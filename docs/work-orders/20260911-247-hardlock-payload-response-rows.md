# 작업 지시서: Hardlock 요청 단위 응답 행

## 한국어

### 배경

`ez2d2m`은 12번째 transform(`function=0x0011`, 7블록)에서 응답을 받지 못하고 종료합니다([작업 246](../work-logs/20260910-246-chd-root-from-profile.md)). 이 요청의 응답은 블록마다 독립이 아니고, 입력 일부가 실행마다 바뀌어 현재의 블록 단위 map으로는 담을 수 없습니다. 근거와 구조는 [설계](../design/20260911-247-hardlock-payload-response-rows.md)에 있습니다.

### 작업 항목

1. 이 머신에서 `ez2d2m` 기준 동작을 재현하고, `0x0011` 입력을 두 번 받아 결정성을 확인합니다.
2. 2EZConfig-V2의 `API_CRYPT`·`API_CODE` 상위 계약을 사실 대조로만 기록합니다.
3. `payload_responses` 모듈을 추가합니다. 요청 행 자료형, `??` 패턴 파싱, 모호성 판정, 일치·적용, pack/unpack.
4. map 파서를 `HardlockTransformResponseMap`으로 바꾸고 행 길이로 블록 행과 요청 행을 가릅니다.
5. `HardlockDevice`가 요청 행을 먼저 적용하고, 맞는 행이 없으면 기존 블록 조회로 돌아가게 합니다.
6. 주입 runtime에 요청 행 export 두 개와 trace `payload=` 필드를 추가합니다.
7. launcher와 자식 프로세스 인계가 두 종류 행을 모두 전달하고, 용량을 넘는 map을 거절하게 합니다.
8. 단위 시험을 추가·갱신합니다.
9. 실행 검증: `ez2d2m` 기준 동작 유지, `ez2dj3rd` 회귀 없음, 항등 요청 행으로 `payload=1` 확인.
10. analysis(`ez2d2m-chd-filesystem.md`, `ez2dj6th-hardlock.md`), kb(Hardlock API function 코드), `ARCHITECTURE.md`를 갱신합니다.

### 제외 범위

- `API_CODE` 응답 계산과 유효한 요청 행 생성. 외부 도구의 몫입니다.
- reSoftlock 저장소 변경
- `ez2d2m` 프로파일의 입력 helper RVA

### 완료 조건

- 기존 map 파일이 이전과 같은 결과로 로드됩니다(`ez2d2m` `entries=11`).
- `ez2d2m` 기본 실행이 261줄, 12번째 transform `unmapped=7:payload=0`으로 유지됩니다.
- 항등 요청 행 실행에서 12번째 transform이 `payload=1`을 보고합니다.
- `ez2dj3rd` 실행 결과가 바뀌지 않습니다.
- Windows x86 Debug 전체 build 경고 0건, 단위 시험 실패 0건.

## English

### Background

`ez2d2m` exits at its twelfth transform — `function=0x0011`, seven blocks — because that request gets no answer ([task 246](../work-logs/20260910-246-chd-root-from-profile.md)). The answer is not independent per block and part of the input changes between runs, so the current block-keyed map cannot hold it. The rationale and structure are in [the design](../design/20260911-247-hardlock-payload-response-rows.md).

### Work items

1. Reproduce `ez2d2m`'s baseline on this machine and capture the `0x0011` input twice to check determinism.
2. Record 2EZConfig-V2's `API_CRYPT` and `API_CODE` high-level contract as fact comparison only.
3. Add the `payload_responses` module: the request-row type, `??` pattern parsing, the ambiguity test, matching and applying, and pack/unpack.
4. Change the map parser to produce `HardlockTransformResponseMap`, separating block rows from request rows by row length.
5. Make `HardlockDevice` apply request rows first and fall back to the existing block lookup when none matches.
6. Add the two request-row exports and a `payload=` trace field to the injected runtime.
7. Make the launcher and the child-process handoff transfer both row kinds and refuse a map that exceeds capacity.
8. Add and update unit tests.
9. Verify at run time: `ez2d2m`'s baseline is kept, `ez2dj3rd` does not regress, and an identity request row reports `payload=1`.
10. Update the analysis (`ez2d2m-chd-filesystem.md`, `ez2dj6th-hardlock.md`), the knowledge base (Hardlock API function codes) and `ARCHITECTURE.md`.

### Out of scope

- Computing `API_CODE` answers or generating a valid request row, which belong to the external tool.
- Changes to the reSoftlock repository.
- The `ez2d2m` profile's input-helper RVA.

### Completion criteria

- Existing map files load with the same result as before (`ez2d2m` `entries=11`).
- `ez2d2m`'s default run stays at 261 lines with a twelfth transform reporting `unmapped=7:payload=0`.
- With an identity request row, the twelfth transform reports `payload=1`.
- `ez2dj3rd`'s run is unchanged.
- The full Windows x86 Debug build has zero warnings and unit tests report zero failures.
