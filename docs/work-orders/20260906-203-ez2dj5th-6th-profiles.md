# ez2dj5th·ez2dj6th 프로파일 확장 작업 지시서

## 관련 설계

[ez2dj5th·ez2dj6th 프로파일 확장 설계](../design/20260906-203-ez2dj5th-6th-profiles.md)

## 작업 목표

`ez2dj4th`의 CHD/HLE 실행 기본값을 기준으로 `ez2dj5th`와 `ez2dj6th` 프로파일을 추가하고, 개발용 `ez2dj1stse_unpacked` 내장 프로파일을 제거합니다. 원본 CHD와 실행 파일은 저장소에 추가하지 않습니다.

## 작업 항목

1. `target_profile.cpp`에 5th·6th CHD 프로파일을 추가합니다.
2. 4th 기반 실행 정책과 버전별 기본 CHD 경로를 확인합니다.
3. `ez2dj1stse_unpacked` 프로파일과 이를 전제로 한 실행 도구 기본값을 제거합니다.
4. detected `ez2dj1` 동작과 5th·6th 프로파일 정책을 unit/product-loader probe에 추가합니다.
5. 5th·6th CHD 관찰 결과를 analysis 문서와 색인에 기록합니다.
6. README, architecture, 사용자 가이드와 작업 로그를 갱신합니다.

## 제외 범위

- 5th CHD의 FAT16 또는 기타 파일시스템 reader 구현
- 5th·6th의 독립적인 Hardlock, raw I/O RVA, 그래픽 계약 추출
- 원본 자산 수정·복사·저장소 반입

## 완료 조건

- `ez2dj5th`와 `ez2dj6th`가 built-in CHD profiles로 조회됩니다.
- 두 프로파일이 4th 기반 실행 기본값과 각자의 `roms` 이미지 경로를 가집니다.
- `ez2dj1stse_unpacked`가 built-in 목록과 실행 도구 기본값에서 제거됩니다.
- 1st SE의 `ez2dj1.exe`는 built-in이 아닌 detected 항목으로만 남습니다.
- Windows x86 build와 관련 테스트가 통과하고, 5th의 reader 미지원 상태가 문서에 명시됩니다.

---

# ez2dj5th and ez2dj6th Profile Expansion Work Order

## Related design

[ez2dj5th and ez2dj6th Profile Expansion Design](../design/20260906-203-ez2dj5th-6th-profiles.md)

## Objective

Add `ez2dj5th` and `ez2dj6th` profiles using the `ez2dj4th` CHD/HLE execution defaults, and remove the development-only `ez2dj1stse_unpacked` built-in profile. Original CHDs and executables must not be added to the repository.

## Work items

1. Add the 5th and 6th CHD profiles to `target_profile.cpp`.
2. Verify the copied 4th execution policy and version-specific default CHD paths.
3. Remove the `ez2dj1stse_unpacked` profile and stale execution-tool defaults that require it.
4. Add unit/product-loader coverage for detected `ez2dj1` behavior and the 5th/6th profile policy.
5. Record the 5th/6th CHD observations in an analysis document and its index.
6. Update the README, architecture, user guide, and work log.

## Out of scope

- Implementing a FAT16 or other filesystem reader for the 5th CHD
- Extracting independent Hardlock, raw-I/O RVA, or graphics contracts for 5th/6th
- Modifying, copying, or importing original assets into the repository

## Completion criteria

- `ez2dj5th` and `ez2dj6th` resolve as built-in CHD profiles.
- Both profiles carry the 4th-based execution defaults and their own `roms` image paths.
- `ez2dj1stse_unpacked` is absent from the built-in list and execution-tool defaults.
- `ez2dj1.exe` in a 1st SE dump remains detected rather than built-in.
- The Windows x86 build and related tests pass, and the 5th reader limitation is documented.
