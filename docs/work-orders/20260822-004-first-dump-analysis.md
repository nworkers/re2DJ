# 작업 지시: 첫 원본 덤프 정적 분석

## 배경

작업 중 사용자가 EZ2DJ 1st Trax Special Edition과 3rd Trax의 HDD 덤프를 `roms/` 아래에 배치했다. 그 시점까지 `docs/analysis/`와 `docs/EXE_DESIGN.*`의 거의 모든 항목이 "미확정 — 덤프 미확인" 상태로 막혀 있었다.

## 목표

두 덤프를 정적으로 분석해 실행 파일을 식별하고, import 표면을 확정하고, 분석 문서의 미확정 항목을 확인된 사실로 바꾼다.

## 범위

* `.gitignore`가 원본 덤프를 차단하는지 먼저 확인하고, 막지 못하면 즉시 수정한다
* `re2dj_hdd_probe`로 두 덤프의 구조와 실행 파일 확인
* `re2dj_pe_analyzer`로 실행 파일의 PE 특성 확인
* import 테이블 해석으로 API 표면 확정
* `docs/analysis/ez2dj-hdd-layout.md` 갱신
* `docs/analysis/ez2dj-import-surface.md` 추가와 색인 갱신
* `docs/EXE_DESIGN.ko.md`, `docs/EXE_DESIGN.en.md` 갱신
* `ARCHITECTURE.md`의 HLE 우선순위 표를 근거 기반으로 교체
* `docs/TODO.md` 갱신

## 비범위

* 실행, 적재, 재배치 — Stage 2 이후
* 보호 계층 해제 — 실행 backend가 성숙한 뒤
* 내장 타깃 프로파일 추가 — 코드 변경이므로 별도 설계와 작업 지시가 필요하다

## 검증

* `git status`에 덤프 파일이 하나도 나타나지 않는다
* 도구 출력과 문서 서술이 일치한다
* 확인됨 / 추정 / 미확정 표기가 근거와 어긋나지 않는다

## Work Order: First Original Dump Static Analysis

## Background

During the work the user placed EZ2DJ 1st Trax Special Edition and 3rd Trax HDD dumps under `roms/`. Until then nearly every item in `docs/analysis/` and the `EXE_DESIGN` documents was blocked as "unresolved — no dump inspected".

## Goal

Analyse both dumps statically to identify the executables, fix the import surface, and convert the unresolved items in the analysis documents into confirmed facts.

## Scope

First confirm `.gitignore` blocks the dumps and fix it immediately if it does not; then probe both dumps, read the PE characteristics, parse the import tables, and update the layout analysis, the new import-surface analysis and its index, both `EXE_DESIGN` documents, the HLE priority table in `ARCHITECTURE.md`, and `docs/TODO.md`.

## Out of scope

Execution, loading, and relocation (Stage 2 onward); defeating the protection layer (after the interpreter matures); and adding built-in target profiles, which is a code change needing its own design and work order.

## Verification

No dump file appears in `git status`; tool output and documented statements agree; and the confirmed, inferred, and unresolved markers match the evidence.
