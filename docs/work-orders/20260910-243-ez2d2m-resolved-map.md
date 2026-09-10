# 작업 지시서: ez2d2m 확정 map 배치와 main 머지

## 한국어

### 관련 설계

선행 판별은 [ez2d2m 런타임 challenge map 재판별](../work-logs/20260910-242-ez2d2m-runtime-map-judgement.md)에 있습니다. 이 작업은 그 결과를 확정 자료로 배치하고 브랜치를 정리하는 마무리 작업이므로 새 설계를 만들지 않습니다.

### 작업 항목

1. `candidate-70`을 `cfg/hardlock-ez2d2m.map`으로 배치합니다.
2. `cfg/hardlock.ini`에 `[ez2d2m]` section을 추가합니다.
3. Hardlock 옵션 없이 프로파일 기본값만으로 같은 결과에 도달하는지 확인합니다.
4. `ez2d2m` 프로파일의 `note`를 관측에 맞게 정정합니다.
5. 분석 문서에 확정 결정과 그 한계를 기록합니다.
6. Windows x86 build와 시험을 검증합니다.
7. patch 버전을 올리고 `main`에 squash 머지한 뒤 annotated tag를 붙이고 작업 브랜치를 삭제합니다.

### 제외 범위

- 16비트 폭 legacy I/O bus와 `0x300` 대역 지원
- 자산 로딩 검증

### 완료 조건

- 프로파일 기본값만으로 `hardlock_cfg_material` 세 항목이 인식됩니다.
- 기존 프로파일 값과 단위 시험이 변하지 않습니다.
- `main`에 머지되고 `VERSION`과 같은 값의 tag가 붙습니다.

## English

### Related design

The judgement this rests on is in [the ez2d2m runtime-challenge map re-judgement](../work-logs/20260910-242-ez2d2m-runtime-map-judgement.md). This task places that result as resolved material and closes the branch, so it introduces no new design.

### Work items

1. Place `candidate-70` as `cfg/hardlock-ez2d2m.map`.
2. Add an `[ez2d2m]` section to `cfg/hardlock.ini`.
3. Confirm the same result is reached from profile defaults alone, with no Hardlock option.
4. Correct the `ez2d2m` profile `note` against observation.
5. Record the adoption decision and its limits in the analysis document.
6. Verify the Windows x86 build and tests.
7. Bump the patch version, squash-merge into `main`, apply the annotated tag, and delete the task branch.

### Out of scope

- A 16-bit-wide legacy I/O bus and the `0x300` band
- Asset-loading validation

### Completion criteria

- All three `hardlock_cfg_material` items are recognised from profile defaults alone.
- Existing profile values and unit tests are unchanged.
- The work is merged into `main` and tagged with the value in `VERSION`.
