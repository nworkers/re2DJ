# 작업 지시서: ez2dj1st·ez2dj5th Hardlock descriptor 확보

## 한국어

### 관련 설계

[ez2dj1st·ez2dj5th Hardlock descriptor 확보 설계](../design/20260909-237-ez2dj1st-5th-hardlock-descriptors.md)

### 작업 항목

1. 두 실행 파일의 PE 배치를 확인해 `.protect` 계열인지, fingerprint가 맞는지 봅니다.
2. `--hle-dynamic-vfs`와 synthetic handshake 응답을 더해 두 프로파일의 descriptor에 도달합니다.
3. `--hardlock-descriptor-dump`로 `cfg/hardlock-id.ini`에 두 section을 기록하고 기존 section이 보존되는지 확인합니다.
4. 관측이 뒤집은 `ez2dj1st` 실행 기본값을 정정합니다.
5. `tests/unit/target_profile_test.cpp`를 정정된 값에 맞춥니다.
6. Windows x86 build와 시험을 검증합니다.
7. 분석 문서와 작업 로그를 작성합니다.

### 제외 범위

- seed 탐색과 transform map 생성
- 실제 `response450`·`tail44c` 확보
- raw I/O helper RVA 확인
- 두 제품의 end-to-end 실행

### 완료 조건

- 두 프로파일에서 `header_valid=1` descriptor를 관측합니다.
- `cfg/hardlock-id.ini`의 `[ez2dj1st]`·`[ez2dj5th]` section에 세 값이 채워지고 기존 section이 남습니다.
- `module_address`는 분석 문서에 기록하고 `id_ref`·`id_verify` 원문은 저장소에 남기지 않습니다.
- unit test, product loader probe가 통과합니다.

## English

### Related design

[ez2dj1st and ez2dj5th Hardlock Descriptor Design](../design/20260909-237-ez2dj1st-5th-hardlock-descriptors.md)

### Work items

1. Inspect both executables' PE layout to confirm the `.protect` family and the fingerprints.
2. Reach the descriptor for both profiles by adding `--hle-dynamic-vfs` and a synthetic handshake response.
3. Record both sections into `cfg/hardlock-id.ini` with `--hardlock-descriptor-dump` and confirm the existing sections survive.
4. Correct the `ez2dj1st` execution defaults the observation contradicts.
5. Update `tests/unit/target_profile_test.cpp` to the corrected values.
6. Verify the Windows x86 build and tests.
7. Write the analysis document and the work log.

### Out of scope

- Seed recovery and transform-map generation
- Obtaining the real `response450` and `tail44c`
- Confirming the raw-I/O helper RVAs
- End-to-end runs of either product

### Completion criteria

- A descriptor with `header_valid=1` is observed for both profiles.
- `cfg/hardlock-id.ini` carries all three values under `[ez2dj1st]` and `[ez2dj5th]`, with the existing sections retained.
- `module_address` appears in the analysis document; raw `id_ref` and `id_verify` do not appear in the repository.
- The unit tests and the product-loader probe pass.
