# 작업 286 — `ez2dj1.exe` 서술 제거와 실행 파일 분석 갱신 / Task 286 — Remove `ez2dj1.exe` narrative and refresh executable analysis

## 한국어

### 배경

누적 문서가 `ez2dj1.exe`를 "보호되지 않은 별도 빌드"이자 "Stage 2·3의 첫 실행 대상"으로
기술하고, import 표면 전체를 그 파일에서 측정한 값으로 제시한다. 현재 제품은 1st SE,
3rd, 4th, 5th, 6th와 EZ2Dancer 2nd MOVE의 **보호된 정식 실행 파일**을 직접 적재하므로
이 서술은 더 이상 현행 구조를 설명하지 않는다. 수치 자체도 정식 빌드와 다르다.

### 목표

1. 누적·현행 문서에서 `ez2dj1.exe`를 근거로 삼는 항목을 제거한다.
2. 실행 파일 분석을 정식 빌드 단독 측정값으로 다시 세운다.

### 범위

포함: `README.md`, `ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, `docs/EXE_DESIGN.en.md`,
`docs/IMPLEMENTED.md`, `docs/WIN32_HLE_PORTING_PLAN.md`, `docs/analysis/`, `docs/guides/`.

제외: `docs/work-logs/`, `docs/work-orders/`, `docs/design/`. 날짜가 박힌 작업 증거이며
AGENTS.md의 "work-logs는 시간순 작업 증거로 유지" 규칙을 따른다. 사용자 결정 사항이다.

제외: 소스 코드. 이번 작업은 문서만 다룬다. `src/target/target_profile.cpp`의 1st SE
덤프 fingerprint 목록과 두 legacy probe 도구의 기본 target id는 그대로 둔다.

### 측정 근거

사용자 제공 자산에 대해 저장소 도구로 재측정한다. 원본 파일은 저장소에 넣지 않는다.

* `re2dj_pe_analyzer` — PE header와 섹션
* `re2dj_pe_loader` — 데이터 디렉터리가 가리키는 import table
* `re2dj_chd_probe` — CHD에서 실행 파일 추출
* import descriptor 직접 해석 — 원본 `.idata` table

### 완료 조건

* 대상 문서에 `ez2dj1.exe` 언급이 남지 않는다.
* import 표면 문서가 정식 빌드 측정값에 근거한다.
* 확인됨 / 추정 / 미확정 표기를 유지한다.
* `docs/analysis/README.md` 색인이 갱신된다.
* 작업 로그를 남긴다.

## English

### Background

The cumulative documents describe `ez2dj1.exe` as a separate unprotected build and the
first execution target for Stages 2 and 3, and present the whole import surface as measured
on that file. The product now loads the **protected canonical executables** of 1st SE, 3rd,
4th, 5th, 6th and EZ2Dancer 2nd MOVE directly, so that narrative no longer describes the
current structure, and its numbers differ from the canonical builds.

### Goals

1. Remove items that rest on `ez2dj1.exe` from the cumulative and current documents.
2. Rebuild the executable analysis on measurements taken from canonical builds alone.

### Scope

Included: `README.md`, `ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, `docs/EXE_DESIGN.en.md`,
`docs/IMPLEMENTED.md`, `docs/WIN32_HLE_PORTING_PLAN.md`, `docs/analysis/`, `docs/guides/`.

Excluded: `docs/work-logs/`, `docs/work-orders/`, `docs/design/` — dated task evidence kept
under the AGENTS.md rule that work logs remain chronological evidence. This is a user decision.

Excluded: source code. This task covers documents only. The 1st SE dump fingerprint list in
`src/target/target_profile.cpp` and the default target id of two legacy probe tools stay as they are.

### Measurement basis

Re-measured with repository tools against user-supplied assets. No original file enters the repository.

* `re2dj_pe_analyzer` — PE headers and sections
* `re2dj_pe_loader` — the import table named by the data directory
* `re2dj_chd_probe` — executable extraction from CHD
* Direct import-descriptor parsing — the original `.idata` table

### Completion criteria

* No `ez2dj1.exe` mention remains in the target documents.
* The import-surface document rests on canonical-build measurements.
* Confirmed / inferred / unresolved marking is preserved.
* The `docs/analysis/README.md` index is updated.
* A work log is left behind.
