# #3 작업 지시서 — 릴리스 노트에 해결된 이슈와 커밋 ID 표시 / #3 work order — resolved issues and commit IDs in release notes

이슈: [#3](https://github.com/nworkers/re2DJ/issues/3) · 설계: [20261005-i003-release-notes-issue-refs.md](../design/20261005-i003-release-notes-issue-refs.md)

## 절차 / Steps

1. `scripts/release/release_refs.py`를 만든다. `scripts/README.md` 표에도 넣는다.
   *Add `scripts/release/release_refs.py` and list it in `scripts/README.md`.*
2. `.github/workflows/release.yml`의 `publish`: `fetch-depth: 0`으로 checkout하고, 노트 파일에 스크립트 출력을 붙여 Release 본문으로 쓴다.
   *In `publish`, check out with `fetch-depth: 0` and use the note file plus the script's output as the Release body.*
3. `docs/release-notes/v0.0.65.md`와 `RELEASE_NOTES.md`의 v0.0.65 절에 해결된 이슈 #1, PR #2, 커밋 `26a9151`을 적는다.
   *Add resolved issue #1, PR #2 and commit `26a9151` to `docs/release-notes/v0.0.65.md` and the v0.0.65 section of `RELEASE_NOTES.md`.*
4. `AGENTS.md`(두 언어)와 `docs/release-notes/README.md`(두 언어)에 규칙을 적는다.
   *Write the rule into `AGENTS.md` and `docs/release-notes/README.md`, in both languages.*
5. 설계의 검증 항목을 실행한다.
   *Run the design's verification.*

## 완료 조건 / Done when

스크립트가 v0.0.65에 대해 #1·PR #2·`26a9151`을 담은 절을 만들고, `release.yml`이 그 절을 Release 본문에 붙인다.

*The script produces a section holding #1, PR #2 and `26a9151` for v0.0.65, and `release.yml` appends it to the Release body.*
