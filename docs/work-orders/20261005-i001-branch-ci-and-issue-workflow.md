# #1 작업 지시서 — 브랜치 push CI와 GitHub 이슈·PR 기반 작업 규칙 / #1 work order — branch-push CI and an issue- and PR-based task workflow

이슈: [#1](https://github.com/reexec/re2DJ/issues/1) · 설계: [20261005-i001-branch-ci-and-issue-workflow.md](../design/20261005-i001-branch-ci-and-issue-workflow.md)

## 절차 / Steps

1. `.github/workflows/ci.yml`: 모든 브랜치 push, `workflow_dispatch`, 브랜치별 concurrency를 넣고, 같은 저장소 PR의 중복 실행을 각 작업의 `if`로 건너뛴다.
   *Add every-branch pushes, `workflow_dispatch` and per-branch concurrency, and skip duplicate same-repository PR runs through each job's `if`.*
2. `AGENTS.md`(두 언어): 요구사항 처리 절차, 문서 파일명, 작업 단위, Git 작업 규칙을 설계 2대로 바꾼다.
   *Change the requirement procedure, document file names, task units and Git rules in both languages as in design 2.*
3. 브랜치를 push하고 초안 PR을 연다(`Closes #1`). CI 결과를 확인한다.
   *Push the branch, open a draft PR (`Closes #1`) and check the CI results.*

## 완료 조건 / Done when

브랜치 push로 세 CI 작업이 돌고, 규칙이 `AGENTS.md`에 반영되어 있다.

*A branch push runs the three CI jobs and the rules are in `AGENTS.md`.*
