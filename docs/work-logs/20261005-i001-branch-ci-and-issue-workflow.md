# #1 작업 로그 — 브랜치 push CI와 GitHub 이슈·PR 기반 작업 규칙 / #1 work log — branch-push CI and an issue- and PR-based task workflow

이슈: [#1](https://github.com/nworkers/re2DJ/issues/1) · 설계: [20261005-i001-branch-ci-and-issue-workflow.md](../design/20261005-i001-branch-ci-and-issue-workflow.md) · 지시서: [20261005-i001-branch-ci-and-issue-workflow.md](../work-orders/20261005-i001-branch-ci-and-issue-workflow.md)

## 2026-10-05

- **출발점**: 작업 459·460의 Windows MSVC 확인 방법을 묻자, `ci.yml`이 `main` push와 PR에서만 돈다는 것이 확인됐다. 사용자는 브랜치 push마다 모든 타깃을 검증하고, 작업을 GitHub 이슈로, `main` 머지를 PR squash merge로 바꾸기로 했다. 저장소에 이슈·PR이 없어 이슈 번호가 #1부터 시작하므로, 사용자와 정해 새 작업을 `#N`과 `YYYYMMDD-iNNN-slug.md`로 구분했다. 이 작업이 첫 이슈 [#1](https://github.com/nworkers/re2DJ/issues/1)이다.
- **구현**: `ci.yml`에 모든 브랜치 push(`'**'`), `workflow_dispatch`, `ci-${{ github.ref }}` concurrency(`main`이 아닐 때만 앞선 실행 취소)를 넣었다. 세 작업에는 같은 저장소 PR의 `pull_request` 실행을 건너뛰는 `if`를 붙였다. `AGENTS.md`의 요구사항 절차 1단계, 문서 파일명, 작업 단위(이슈 생성과 `#N`), Git 규칙(브랜치 push CI, PR squash merge와 로컬 `main` 갱신, squash 커밋에 태그, 로컬·원격 브랜치 삭제)을 두 언어로 고쳤다.

  *Starting point: asking how to check tasks 459 and 460 against Windows MSVC showed that `ci.yml` ran only on `main` pushes and PRs. The user chose to verify every target on every branch push, track tasks as GitHub issues and merge into `main` through PR squash merges; with no issues or PRs in the repository, issue numbers start at #1, so new tasks were agreed to be written `#N` with documents named `YYYYMMDD-iNNN-slug.md`. This task is the first issue, [#1](https://github.com/nworkers/re2DJ/issues/1). Implementation: `ci.yml` gains every-branch pushes (`'**'`), `workflow_dispatch` and `ci-${{ github.ref }}` concurrency (cancelling earlier runs except on `main`), and each of the three jobs an `if` skipping the `pull_request` run of same-repository PRs; `AGENTS.md` changes, in both languages, step 1 of the requirement procedure, document file names, task units (creating an issue, `#N`) and the Git rules (branch-push CI, PR squash merge and updating the local `main`, the tag on the squash commit, deleting the branch locally and remotely).*

- **검증**
  - `ci.yml`을 PyYAML로 읽었다. 트리거는 `push.branches ['**']`·`pull_request`·`workflow_dispatch`이고, concurrency는 `ci-${{ github.ref }}`, 세 작업 모두 `if`가 있다.
  - 브랜치 push와 초안 PR 뒤의 CI 결과는 아래에 이어 적는다.

  *Verification: PyYAML reads `ci.yml` with triggers `push.branches ['**']`, `pull_request` and `workflow_dispatch`, concurrency `ci-${{ github.ref }}`, and an `if` on all three jobs. The CI results after the branch push and the draft PR follow below.*
  - 브랜치 push(`dedfc32`)로 `ci` [실행 37251046072](https://github.com/nworkers/re2DJ/actions/runs/37251046072)이 시작돼 네 작업이 모두 성공했다: `windows-x86` 6분 47초, `linux-x86` 3분 21초, `linux-x64 (clang)` 3분 20초, `linux-x64 (gcc)` 2분 26초. 초안 PR [#2](https://github.com/nworkers/re2DJ/pull/2)의 `pull_request` 실행은 설계대로 작업이 건너뛰어져 `skipped`로 끝났다.
  - annotation은 두 건이다. `actions/checkout@v4`가 Node.js 20을 대상으로 해 Node.js 24로 강제 실행된다는 경고, 그리고 `ubuntu-latest`가 2026-10-19부터 Ubuntu 26으로 옮겨 간다는 공지다. 옮겨 가면 CI의 clang·GCC가 이 실기와 같은 21·15 계열이 되는데, 그 빌드 문제는 작업 459·460에서 고쳤다. checkout 버전은 이 작업에서 바꾸지 않는다.

  *The branch push (`dedfc32`) started `ci` [run 37251046072](https://github.com/nworkers/re2DJ/actions/runs/37251046072) and all four jobs passed: `windows-x86` in 6 min 47 s, `linux-x86` 3 min 21 s, `linux-x64 (clang)` 3 min 20 s and `linux-x64 (gcc)` 2 min 26 s; the `pull_request` run of draft PR [#2](https://github.com/nworkers/re2DJ/pull/2) ended `skipped` with its jobs skipped, as designed. Two annotations: a warning that `actions/checkout@v4` targets Node.js 20 and is forced onto Node.js 24, and a notice that `ubuntu-latest` moves to Ubuntu 26 from 2026-10-19, which will bring CI's clang and GCC to the 21 and 15 lines of this machine, whose build problems tasks 459 and 460 fixed; the checkout version is left unchanged in this task.*
