# 작업 지시: HDD 디렉터리 입력

## 목표

원본 HDD 내용을 디렉터리 경로로 입력받아, 대소문자를 무시하고 경로를 해석하고, 실행 대상 바이너리를 식별하는 경로를 구현한다.

## 범위

* `re2dj::storage::GuestPath` — Win32 경로 파싱, 정규화, 결합
* `re2dj::hdd::HddRoot` — 디렉터리 검증, 대소문자 무시 해석, 나열 캐시
* `re2dj::exe::PeImageInfo` — PE32 헤더·섹션·데이터 디렉터리 판독
* `re2dj::hdd::ScanHdd` — 디렉터리 순회와 실행 파일 후보 순위
* `re2dj::target::TargetProfile` — 프로파일 자료구조와 스캔 기반 감지
* `re2dj` 명령행 호스트 — `--hdd`, `--target`, `--list-targets`, `--resolve`, `--run`
* `re2dj_hdd_probe`, `re2dj_pe_analyzer` 도구
* 위 전부에 대한 단위 테스트
* `docs/guides/hdd-directory-setup.md` 절차 문서

## 검증

* 단위 테스트가 통과한다.
* `re2dj_pe_analyzer` 출력이 실제 32비트 PE에 대해 `dumpbin /headers`와 일치한다.
* 32비트와 64비트 실행 파일이 섞인 디렉터리에서 32비트 항목만 후보로 잡힌다.
* 요청한 대소문자와 무관하게 경로가 해석되고, 결과가 디스크에 있는 철자로 나온다.
* 루트를 벗어나는 게스트 경로가 거부된다.
* `--run`이 미구현임을 명확히 알리고 종료 코드 3으로 끝난다.

## 비범위

* 이미지 적재, 재배치, import 해석 — Stage 2
* overlay 쓰기 구현 — Stage 5
* 실행 backend — Stage 3

## Work Order: HDD Directory Input

## Goal

Implement the path that takes original HDD contents as a directory, resolves paths case-insensitively, and identifies the binary to run.

## Scope

`GuestPath` parsing, normalisation, and combination; `HddRoot` validation, case-insensitive resolution, and listing cache; `PeImageInfo` header, section, and data-directory reading; `ScanHdd` traversal and candidate ranking; `TargetProfile` and scan-based detection; the `re2dj` command-line host with its options; the `re2dj_hdd_probe` and `re2dj_pe_analyzer` tools; unit tests for all of it; and the setup guide.

## Verification

Unit tests pass; `re2dj_pe_analyzer` agrees with `dumpbin /headers` on a real 32-bit PE; only the 32-bit entry is a candidate in a mixed directory; paths resolve regardless of requested case and report the on-disk spelling; a guest path escaping the root is refused; and `--run` reports clearly that it is unimplemented and exits with code 3.

## Out of scope

Image loading, relocation, and import resolution (Stage 2); overlay write implementation (Stage 5); the execution backend (Stage 3).
