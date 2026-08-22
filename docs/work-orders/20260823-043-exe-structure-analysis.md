# 실행 파일 구조 문서 정리 작업 지시

## 목표

실행 파일별 구조 분석이 여러 문서와 작업 로그에 흩어져 있어, 실행 파일 하나를 볼 때마다 여러 곳을 오가야 한다. 실행 파일별 구조를 한곳에 모은 주제별 누적 문서를 만들고, 새 실행 파일이 나올 때 같은 형식으로 섹션을 추가하는 절차를 문서화한다.

## 설계 결정

* 위치는 `docs/analysis/ez2dj-exe-structures.md`다. 주제는 "실행 파일 구조" 하나이고, 실행 파일마다 섹션 하나를 가지는 단일 누적 문서로 한다. 실행 파일 수가 지금은 다섯 개뿐이고 대부분 아직 표면 수준이라 문서당 하나의 파일이 적절하다. 특정 실행 파일의 분석이 깊어지면 그때 해당 섹션을 독립 문서로 분리하고 색인을 갱신한다.
* 각 실행 파일 섹션은 같은 골격을 따른다: 식별·헤더·섹션 테이블 → 진입점 → 보호 계층 해부(있다면) → 문자열·데이터 인벤토리 → 관찰된 런타임 흐름 → 추정·미확정.
* 모든 서술에 확인됨/추정/미확정 표기를 유지하고, 측정 도구(`re2dj_pe_analyzer`)와 갱신 절차를 문서 머리에 둔다.
* `docs/analysis/README.md` 색인과 `docs/EXE_DESIGN.ko.md`·`.en.md` 링크를 같은 작업에서 갱신한다.

## 작업

1. 두 덤프의 다섯 실행 파일 전체에 대해 `re2dj_pe_analyzer` 헤더·섹션 표를 확보한다.
2. `docs/analysis/ez2dj-exe-structures.md`를 작성한다. 기존 흩어진 확인 사항(hdd-layout, import-surface, work logs 036~042)을 실행 파일별로 재편하고, 오프셋·VA 인벤토리와 런타임 흐름 도식을 포함한다.
3. `docs/analysis/README.md` 색인과 `EXE_DESIGN.ko/en.md` 링크를 갱신한다.
4. 작업 로그를 남기고 커밋한다. 코드 변경은 없으므로 빌드 검증은 생략하고 그 이유를 작업 로그에 남긴다.

---

# Executable Structure Documentation Work Order

## Goal

Executable-structure findings are scattered across several documents and work logs. Consolidate them into one topic-based cumulative document and document the procedure for adding a section whenever a new executable appears.

## Design decision

* Location: `docs/analysis/ez2dj-exe-structures.md`. One cumulative document whose sections are per-executable. Only five executables exist today and most are known at surface level, so a single file fits; split a section into its own document when one binary's analysis grows deep, updating the index then.
* Every executable section follows one skeleton: identification, header, and section table → entry point → protection anatomy when present → string/data inventory → observed runtime flow → inferred/unresolved items.
* Keep confirmed/inferred/unresolved marking, and put the measurement tool (`re2dj_pe_analyzer`) plus the update procedure at the top.
* Update the `docs/analysis/README.md` index and the `EXE_DESIGN.ko/en.md` links in the same task.

## Tasks

1. Capture header/section tables for all five executables across both dumps with `re2dj_pe_analyzer`.
2. Write `docs/analysis/ez2dj-exe-structures.md`, reorganizing existing confirmed findings (hdd-layout, import-surface, work logs 036~042) per executable with offset/VA inventories and a runtime-flow diagram.
3. Update the `docs/analysis/README.md` index and `EXE_DESIGN.ko/en.md` links.
4. Leave a work log and commit. There is no code change, so build verification is skipped and the reason is recorded in the work log.
