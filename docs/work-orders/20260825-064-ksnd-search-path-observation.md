# KSND search-path 관찰 작업 지시

관련 설계: [KSND search-path와 파일 후보 관찰](../design/20260825-064-ksnd-search-path-observation.md)

## 상태

**완료**

## 작업 범위

1. 확인된 1st SE controlled-exit caller에서 search-path count와 entry를 bounded 진단으로 기록한다.
2. Windows x86 Debug build와 CTest를 수행한다.
3. `--api-trace` canonical 실행 2회에서 search state, CreateFile 후보, `av_access` 유무를 비교한다.
4. 확인 결과를 analysis, TODO, architecture와 작업 로그에 반영한다.

## 완료 조건

- `coin0.wav` 이전 search-path 등록 상태가 두 번 동일하게 기록된다.
- 실제 `Re2djVfsCreateFileA` 후보 유무가 search state와 함께 설명된다.
- 원본 흐름이나 VFS 정책을 변경하지 않는다.

---

# KSND Search-Path Observation Work Order

Related design: [KSND Search-Path and File-Candidate Observation](../design/20260825-064-ksnd-search-path-observation.md)

## Status

**Complete**

## Scope and completion

Record bounded search-path state at the confirmed 1st SE controlled-exit caller, build and test Windows x86, then compare two `--api-trace` canonical runs for search entries, actual `Re2djVfsCreateFileA` candidates, and access violations. Update cumulative documentation without changing guest flow or VFS policy.
