# CHD 프로파일 실행파일 선택과 최소 staging 작업 지시서

## 관련 설계

[CHD 프로파일 실행파일 선택과 최소 staging 설계](../design/20260906-204-chd-minimal-staging.md)

## 목표

CHD 실행 전에 프로파일이 지정한 원본 실행 파일만 staging하여 6th의 불필요한 `FONTKR.DAT` eager read 실패를 제거하고, bootstrap 대신 실제 `EZ2DJ6th.EXE`를 실행합니다.

## 작업 항목

1. CHD 프로파일에 내부 실행파일 경로를 명시하고 6th를 `EZ2DJ/EZ2DJ6th.EXE`로 지정합니다.
2. CHD staging 대상에서 fingerprint용 INI와 font 파일을 제거합니다.
3. staging root를 profile ID별로 분리하고 선택한 실행파일의 상대 경로를 보존합니다.
4. materialization 오류에 CHD 내부 경로를 추가합니다.
5. 6th CHD로 실제 게임 executable의 launcher 진입을 확인합니다.
6. 읽기 전용 IAT slot을 임시 쓰기 가능하게 패치한 뒤 원래 보호 속성을 복원합니다.
7. 6th에서 확인되지 않은 4th raw-I/O helper RVA를 비활성화합니다.
8. Windows x86 빌드와 관련 테스트를 실행합니다.
9. 분석 문서, 아키텍처, 구현 완료 목록과 작업 로그를 갱신합니다.

## 범위 제외

- 손상되거나 비표준인 FAT chain의 추측 복구
- FAT32 쓰기
- 5th 파일시스템 지원
- 6th 전용 Hardlock·raw I/O·그래픽 계약 확정

---

# CHD Profile Executable Selection and Minimal Staging Work Order

## Related design

[CHD Profile Executable Selection and Minimal Staging Design](../design/20260906-204-chd-minimal-staging.md)

## Objective

Stage only the profile-selected original executable before CHD launch, removing the unnecessary eager `FONTKR.DAT` read and launching the actual `EZ2DJ6th.EXE` game instead of its bootstrap.

## Work items

1. Add an explicit internal executable path to CHD profiles and select `EZ2DJ/EZ2DJ6th.EXE` for 6th.
2. Remove fingerprint-only INI and font files from CHD staging.
3. Isolate the staging root by profile ID while preserving the selected executable's relative path.
4. Add the internal CHD path to materialization errors.
5. Confirm that the actual 6th game executable reaches the launcher after staging.
6. Patch read-only IAT slots under temporary write access and restore their original protection.
7. Disable the unconfirmed 4th raw-I/O helper RVAs for 6th.
8. Run the Windows x86 build and related tests.
9. Update analysis, architecture, the implemented list, and the work log.

## Out of scope

- Guessing recovery for damaged or nonstandard FAT chains
- FAT32 writes
- Support for the 5th filesystem
- Confirmation of 6th-specific Hardlock, raw-I/O, or graphics contracts
