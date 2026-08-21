# 작업 지시: 내장 타깃 프로파일

## 목표

[작업 로그 004](../work-logs/20260822-004-first-dump-analysis.md)가 기록한 기본 타깃 선택 결함을 내장 타깃 프로파일로 해결한다. 설계는 [20260822-005](../design/20260822-005-built-in-target-profiles.md).

## 범위

* `re2dj::exe::EntryPointSectionName()`, `HasEntryPointOutsideTextSection()` 추가
* `re2dj::target::TargetFingerprint`와 `BuiltInTargetProfile` 추가
* 확인한 덤프 두 개에 대한 내장 프로파일 세 개 추가
* `TargetProfile`에 `guest_drive_letter`, `guest_directory`, `bring_up_target`, `note` 추가
* `MatchBuiltInTargetProfiles()` 추가, `DetectTargetProfiles()`에 제외 목록 추가
* `BuildTargetProfiles()`가 `HddRoot`를 받도록 변경
* CLI와 `re2dj_hdd_probe` 출력 갱신
* `TemporaryTree`를 테스트 공용 헬퍼로 분리
* 단위 테스트 추가
* 문서 갱신: `ARCHITECTURE.md`, `docs/analysis/ez2dj-hdd-layout.md`, `docs/EXE_DESIGN.{ko,en}.md`, `docs/TODO.md`, `docs/guides/hdd-directory-setup.md`, `README.md`

## 검증

* 단위 테스트 통과
* 1st SE 덤프 기본 타깃이 `ez2dj1stse` → `ez2dj.exe`
* 3rd 덤프 기본 타깃이 `ez2dj3rd` → `EZ2DJ.EXE`
* 두 덤프가 서로의 프로파일에 걸리지 않는다
* 중첩 루트에서도 같은 프로파일이 잡힌다
* 지문이 깨진 덤프는 내장 프로파일을 가져가지 않고 감지로 넘어간다
* 내장 프로파일이 없는 디렉터리에서 감지가 그대로 동작한다
* 경고를 오류로 하여 빌드된다

## 비범위

* 이미지 적재와 실행 — Stage 2 이후
* 비ASCII 경로 출력 결함 — 별도 작업
* 추가 버전의 내장 프로파일 — 덤프를 확인한 뒤에만 추가한다

## Work Order: Built-In Target Profiles

## Goal

Resolve the default-target defect recorded in work log 004 with built-in target profiles, per design 20260822-005.

## Scope

Add the entry-point section helpers to the PE reader; add `TargetFingerprint` and `BuiltInTargetProfile` with three profiles covering the two inspected dumps; extend `TargetProfile` with guest path, bring-up, and note fields; add fingerprint matching and an exclusion list for detection; give `BuildTargetProfiles()` the `HddRoot`; update the CLI and probe output; extract `TemporaryTree` as a shared test helper; add unit tests; and update the affected documents.

## Verification

Unit tests pass; both real dumps select the correct default target; neither dump matches the other's profile; a nested root still matches; a broken fingerprint falls through to detection; detection still works with no built-in match; and the build is clean with warnings as errors.

## Out of scope

Image loading and execution (Stage 2 onward); the non-ASCII path output defect; and built-in profiles for versions whose dumps have not been inspected.
