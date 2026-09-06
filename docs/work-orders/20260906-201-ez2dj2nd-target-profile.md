# ez2dj2nd 타깃 프로파일 작업 지시서

## 목적

`roms/ez2dj2nd`에 있는 EZ2DJ 2nd Trax HDD 구조를 내장 `ez2dj2nd` 프로파일로 등록한다. 사용자가 요청한 대로 1st SE의 기본 HLE 실행 정책을 복제하되, 2nd 바이너리에서 확인하지 못한 보호·게스트 부트 계약은 사실로 확정하지 않는다.

## 선행 설계

- 설계 문서: `docs/design/20260906-201-ez2dj2nd-target-profile.md`
- 관련 누적 분석: `docs/analysis/ez2dj-hdd-layout.md`

## 작업 항목

1. 실제 `EZ2DJ.exe`의 PE fingerprint와 `EZ2DJ.ini`, `bg`, `sound`, `system` sibling 조건을 사용해 내장 매칭을 추가한다.
2. 1st SE의 HLE command-line, Windows directory, VFS, D3D3, DirectSound, LPTDI 기본값을 호환성 기준으로 복제한다.
3. `System.ini` 부재로 확인할 수 없는 게스트 드라이브·디렉터리는 비워 둔다.
4. 2nd 프로파일 매칭, 기존 프로파일 비간섭, 불완전한 구조의 비매칭을 단위 테스트로 검증한다.
5. 설계 문서와 누적 분석 문서를 갱신하고 빌드·테스트·실제 `--list-targets` 결과를 작업 로그에 남긴다.

## 제외 범위

- `roms/ez2dj2nd` 원본 파일의 복사, 수정, 삭제
- 2nd 전용 Hardlock `id_ref`/`id_verify` 추출 또는 에뮬레이션
- 2nd 바이너리의 legacy I/O 주소를 1st SE 값으로 확정하는 작업
- 게임 로직 또는 그래픽 로직 재구현

## 완료 조건

- 프로파일 ID가 `ez2dj2nd`이고 `ez2dj/EZ2DJ.exe`를 built-in으로 선택한다.
- 단위 테스트와 Windows x86 빌드가 통과한다.
- 변경 사항이 작업 로그와 Git 커밋으로 남는다.

---

# ez2dj2nd Target Profile Work Order

## Objective

Register the EZ2DJ 2nd Trax HDD layout under `roms/ez2dj2nd` as the built-in `ez2dj2nd` profile. As requested, clone the 1st SE HLE execution policy as a compatibility baseline while avoiding claims about protection or guest-boot behavior not verified in the 2nd binary.

## Preceding design

- Design: `docs/design/20260906-201-ez2dj2nd-target-profile.md`
- Related cumulative analysis: `docs/analysis/ez2dj-hdd-layout.md`

## Work items

1. Add built-in matching using the representative `EZ2DJ.exe` PE fingerprint and the `EZ2DJ.ini`, `bg`, `sound`, and `system` sibling conditions.
2. Clone the 1st SE command-line, Windows-directory, VFS, D3D3, DirectSound, and LPTDI defaults as compatibility settings.
3. Leave the guest drive and directory empty because `System.ini` was not found.
4. Cover 2nd matching, non-interference with existing profiles, and incomplete-layout rejection in unit tests.
5. Update the design and cumulative analysis documents, then record build, test, and real `--list-targets` evidence in the work log.

## Out of scope

- Copying, modifying, or deleting original files under `roms/ez2dj2nd`
- Extracting or emulating 2nd-specific Hardlock `id_ref`/`id_verify`
- Treating 1st SE legacy-I/O addresses as confirmed 2nd values
- Reimplementing game or graphics logic

## Completion criteria

- Profile ID is `ez2dj2nd`, and `ez2dj/EZ2DJ.exe` is selected as built-in.
- Unit tests and the Windows x86 build pass.
- The changes are recorded in a work log and Git commit.
