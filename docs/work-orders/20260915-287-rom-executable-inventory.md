# 작업 287 작업 지시 — `roms/` 미분석 실행 파일 추가 분석 / Task 287 work order — Adding the unanalyzed `roms/` executables

선행 작업: [작업 286](20260914-286-exe-analysis-refresh.md)

## 한국어

### 배경

작업 286은 1st SE·3rd·4th·`ez2d2m` 정식 빌드를 기준으로 실행 파일 분석을 다시 측정했다. 그 범위에는 사용자가 `roms/`에 이미 제공한 다른 실행 파일이 들어가지 않았다. 현재 `docs/analysis/ez2dj-exe-structures.md`에는 5th와 6th를 다루는 절이 하나도 없고, 1st Tracks는 `docs/analysis/ez2dj-hdd-layout.md`에 한 문단만 있다.

### 목표

`roms/` 아래에서 아직 구조 분석이 없는 **게임 실행 파일**을 찾아 `docs/analysis/`에 누적한다. 서드파티 드라이버·설치 관리자·OS 구성요소는 대상이 아니다.

사용자 추가 요구로, 실행 파일 식별에 MD5와 SHA-1 해시를 함께 기록한다.

### 대상

| 대상 | 경로 | 현재 상태 |
| --- | --- | --- |
| 1st Tracks 보호 빌드 | `roms/ez2dj1st/ez2dj/Ez2DJ.exe` | HDD 레이아웃에 한 문단만 존재 |
| 1st Tracks 중복 배치 | `roms/ez2dj1st/ez2dj1/` | 미기록 |
| 1st SE `.protect` 빌드 | `roms/ez2dj1stse/ez2dj1stse.chd` 내부 | CHD 문서에만 존재, 구조 문서에 없음 |
| 5th Trax | `roms/ez2dj5th/ez2dj/EZ2DJ.exe` | 어느 문서에도 없음 |
| 6th bootstrap | `roms/ez2dj6th/.../EZ2DJ.EXE` | CHD 문서에 두 줄 |
| 6th 게임 본체 | `roms/ez2dj6th/.../EZ2DJ6th.EXE` | CHD 문서에 두 줄 |
| 6th 동봉 1st Tracks | `6th.chd` 내부 `EZ2DJ/Ez2Dj1st/` | 미기록 |
| 보조 도구 | `Test.exe`, `PlzPowerOff.exe`, `AllowIo.exe`, `PortTalk.sys` | 1st SE 것만 기록 |

### 절차

1. `re2dj_pe_analyzer`로 헤더·섹션·데이터 디렉터리를 측정한다.
2. import table은 `re2dj_pe_loader`와 별도 IMAGE_IMPORT_DESCRIPTOR 해석 두 경로로 교차 확인한다. 보호된 빌드는 원본 `.idata`와 packed table을 따로 센다.
3. CHD 안의 파일은 `re2dj_chd_probe --dump`로 꺼내 측정한다.
4. 각 파일의 크기, MD5, SHA-1, SHA-256을 기록한다.
5. 같은 제품의 서로 다른 입력(디렉터리 덤프 / CHD)은 해시로 동일 여부를 판정한다.
6. 확인된 사실만 **확인됨**으로 적고, 출처 추정은 **추정**, 측정으로 닿지 않은 것은 **미확정**으로 적는다.

### 산출물

* `docs/analysis/ez2dj-exe-structures.md` — 1st Tracks·5th·6th 절 추가, 해시 표 추가, 절 색인 추가.
* `docs/analysis/ez2dj-import-surface.md` — 1st Tracks·5th·6th 열 추가.
* `docs/analysis/ez2dj-hdd-layout.md` — PE 특성 표 확장.
* `docs/analysis/ez2dj5th-6th-chd-filesystem.md` — 디렉터리 덤프 확인 결과 반영.
* `docs/analysis/ez2dj1stse-chd-filesystem.md` — 두 보호 빌드 비교 결과 반영.
* `docs/analysis/README.md` — 색인 상태 갱신.
* `docs/EXE_DESIGN.ko.md`, `docs/EXE_DESIGN.en.md` — 결론 반영.
* `docs/work-logs/20260915-287-rom-executable-inventory.md`.

### 범위에서 뺀 것

* 코드 변경. 이 작업은 문서만 바꾼다.
* 원본 자산 저장소 반입. 구조·오프셋·개수·해시만 기록한다.
* 서드파티 드라이버와 설치 관리자 바이너리.
* 새로 확인한 실행 파일의 런타임 실행. 정적 측정만 한다.

## English

### Background

Task 286 re-measured the executable analysis against the canonical 1st SE, 3rd, 4th and `ez2d2m` builds. That scope left out the other executables the user had already staged under `roms/`. `docs/analysis/ez2dj-exe-structures.md` currently has no section for 5th or 6th at all, and 1st Tracks appears only as a single paragraph in `docs/analysis/ez2dj-hdd-layout.md`.

### Goal

Find the **game executables** under `roms/` that have no structural analysis yet and accumulate them into `docs/analysis/`. Third-party drivers, installers and OS components are out of scope.

At the user's additional request, record MD5 and SHA-1 hashes alongside each executable identification.

### Targets

| Target | Path | Current state |
| --- | --- | --- |
| 1st Tracks protected build | `roms/ez2dj1st/ez2dj/Ez2DJ.exe` | one paragraph in the HDD layout |
| 1st Tracks duplicate layout | `roms/ez2dj1st/ez2dj1/` | unrecorded |
| 1st SE `.protect` build | inside `roms/ez2dj1stse/ez2dj1stse.chd` | in the CHD document only, not in structures |
| 5th Trax | `roms/ez2dj5th/ez2dj/EZ2DJ.exe` | in no document |
| 6th bootstrap | `roms/ez2dj6th/.../EZ2DJ.EXE` | two lines in the CHD document |
| 6th game body | `roms/ez2dj6th/.../EZ2DJ6th.EXE` | two lines in the CHD document |
| 6th bundled 1st Tracks | `EZ2DJ/Ez2Dj1st/` inside `6th.chd` | unrecorded |
| Auxiliary tools | `Test.exe`, `PlzPowerOff.exe`, `AllowIo.exe`, `PortTalk.sys` | 1st SE copies only |

### Procedure

1. Measure headers, sections and data directories with `re2dj_pe_analyzer`.
2. Cross-check every import table through two paths — `re2dj_pe_loader` and a separate IMAGE_IMPORT_DESCRIPTOR parse. For protected builds, count the original `.idata` and the packed table separately.
3. Extract files inside a CHD with `re2dj_chd_probe --dump` before measuring.
4. Record size, MD5, SHA-1 and SHA-256 for every file.
5. Decide whether two inputs of the same product (directory dump vs CHD) are the same build by hash.
6. Mark only measured facts as **confirmed**; mark provenance guesses as **inferred** and anything the measurement does not reach as **unresolved**.

### Deliverables

* `docs/analysis/ez2dj-exe-structures.md` — new 1st Tracks, 5th and 6th sections, a hash table, and a section index.
* `docs/analysis/ez2dj-import-surface.md` — new 1st Tracks, 5th and 6th columns.
* `docs/analysis/ez2dj-hdd-layout.md` — extended PE characteristics table.
* `docs/analysis/ez2dj5th-6th-chd-filesystem.md` — directory-dump findings folded in.
* `docs/analysis/ez2dj1stse-chd-filesystem.md` — the two-protected-build comparison folded in.
* `docs/analysis/README.md` — index status refreshed.
* `docs/EXE_DESIGN.ko.md`, `docs/EXE_DESIGN.en.md` — conclusions folded in.
* `docs/work-logs/20260915-287-rom-executable-inventory.md`.

### Excluded from scope

* Code changes. This task edits documents only.
* Bringing original assets into the repository. Only structures, offsets, counts and hashes are recorded.
* Third-party driver and installer binaries.
* Running the newly identified executables. Static measurement only.
