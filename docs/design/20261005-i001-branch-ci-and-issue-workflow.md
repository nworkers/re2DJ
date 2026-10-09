# #1 설계 — 브랜치 push CI와 GitHub 이슈·PR 기반 작업 규칙 / #1 design — branch-push CI and an issue- and PR-based task workflow

이슈: [#1](https://github.com/reexec/re2DJ/issues/1) · 선행: [작업 458](../work-logs/20261005-458-linux-desktop-validation.md), [작업 459](../work-logs/20261005-459-linux-x86-gcc15-host-abi.md), [작업 460](../work-logs/20261005-460-spdlog-fmt-clang21.md)

## 배경 / Background

`ci.yml`은 `main` push와 `pull_request`에서만 돌았다. 작업 459·460은 Windows MSVC와 CI의 GCC 12·clang 18에서 확인해야 하는데, 작업 브랜치를 push해도 CI가 돌지 않았다. 사용자는 브랜치 push마다 모든 타깃을 검증하고, 작업과 머지를 GitHub 이슈·PR로 관리하기로 했다. 저장소에는 아직 이슈와 PR이 없어서 이슈 번호가 #1부터 시작하고, 기존 작업 번호 001~460과 겹친다.

*`ci.yml` ran only on pushes to `main` and on `pull_request`, so pushing a task branch did not check tasks 459 and 460 against Windows MSVC and CI's GCC 12 and clang 18. The user chose to verify every target on every branch push and to manage tasks and merges through GitHub issues and PRs. The repository has no issues or PRs yet, so issue numbers start at #1 and overlap the earlier task numbers 001–460.*

## 결정 / Decisions

### 1. CI 트리거 / CI triggers

```yaml
on:
  push:
    branches: ['**']
  pull_request:
  workflow_dispatch:

concurrency:
  group: ci-${{ github.ref }}
  cancel-in-progress: ${{ github.ref != 'refs/heads/main' }}
```

- 모든 브랜치 push에서 세 작업(`windows-x86`, `linux-x64` gcc·clang, `linux-x86`)이 돈다. `branches` 필터가 있으므로 태그 push는 CI가 아니라 `release.yml`만 받는다.
- 같은 저장소 브랜치에서 연 PR은 push 실행과 같은 작업을 한 번 더 돌린다. 그래서 각 작업에 `if: github.event_name != 'pull_request' || github.event.pull_request.head.repo.full_name != github.repository`를 둔다. 같은 저장소 PR은 push 실행의 결과가 PR의 head commit에 붙어 PR 화면에 보인다. fork에서 온 PR은 push 실행이 없으므로 `pull_request`로 돈다.
- 같은 브랜치에 새 push가 오면 앞선 실행을 취소한다. `main`은 커밋마다 결과를 남기려고 취소하지 않는다.
- `workflow_dispatch`로 손으로도 실행할 수 있다.
- 감수하는 점: 같은 저장소 PR에서는 "PR을 `main`에 합친 결과"가 아니라 브랜치 head를 검증한다. 머지 직전에 `main`이 앞서 있으면 브랜치를 갱신(rebase 또는 merge)해 다시 push한다.

*Every branch push runs the three jobs (`windows-x86`, `linux-x64` gcc and clang, `linux-x86`); with a `branches` filter, tag pushes go only to `release.yml`. A PR opened from a branch of the same repository would run the same jobs a second time, so each job carries `if: github.event_name != 'pull_request' || github.event.pull_request.head.repo.full_name != github.repository`; for same-repository PRs the push run's results attach to the PR's head commit and show on the PR, while PRs from forks, having no push run, run on `pull_request`. A newer push to the same branch cancels the earlier run, except on `main`, where every commit keeps its result. `workflow_dispatch` allows a manual run. The trade-off: a same-repository PR checks the branch head rather than the PR merged into `main`, so when `main` has moved ahead before a merge, the branch is updated (rebased or merged) and pushed again.*

### 2. 작업 규칙 / Task workflow (`AGENTS.md`)

| 항목 / item | 이전 / before | 이후 / after |
| --- | --- | --- |
| 작업 생성 / creating a task | 로컬 번호 / a local number | GitHub 이슈 / a GitHub issue (`gh issue create`) |
| 작업 번호 / task number | 001~460 | 이슈 번호, `#N`으로 표기 / the issue number, written `#N` |
| 작업 문서 파일명 / task document names | `YYYYMMDD-###-slug.md` | `YYYYMMDD-iNNN-slug.md` (예 / e.g. `20261005-i001-…`) |
| `main` 머지 / merging into `main` | 로컬 squash / a local squash | PR → GitHub squash merge → 로컬 `main` 갱신 / PR → GitHub squash merge → update the local `main` |
| 버전·태그 / version and tag | 머지 전 patch +1, 머지 커밋에 로컬 annotated tag | 같음. 태그는 갱신한 로컬 `main`의 squash 커밋에 붙인다 / the same, the tag going on the squash commit in the updated local `main` |
| 브랜치 삭제 / branch deletion | 머지 뒤 삭제 | 같음(로컬과 원격) / the same (local and remote) |

- 기존 001~460의 문서와 참조는 그대로 둔다. 문서에서 기존 작업은 "작업 NNN", 새 작업은 "#N"으로 쓴다.
- 머지 순서: `VERSION`의 patch를 올려 커밋 → push → PR이 없으면 만든다 → CI 통과를 확인 → 브랜치 커밋 제목을 보고 전체를 나타내는 제목으로 `gh pr merge --squash --delete-branch` → `git checkout main && git pull --ff-only` → `v<VERSION>` annotated tag(원격 push는 사용자) → 남은 로컬 브랜치 삭제.
- PR 본문은 작업 이슈를 `Closes #N`으로 연결해, 머지되면 이슈가 닫히게 한다.
- 단순 질문·확인 요청은 지금처럼 이슈를 만들지 않는다.

*Earlier tasks 001–460 keep their documents and references; documents call them "task NNN" and new tasks "#N". Merge order: bump the patch in `VERSION` and commit → push → open a PR if none exists → confirm CI passes → `gh pr merge --squash --delete-branch` with a title that sums up the branch's commit titles → `git checkout main && git pull --ff-only` → annotated tag `v<VERSION>` (the user pushes it) → delete any remaining local branch. The PR body links the task issue with `Closes #N` so the merge closes it. Simple questions and confirmation requests still get no issue.*

## 검증 / Verification

- `ci.yml`을 YAML로 읽어 트리거·concurrency·각 작업의 `if`를 확인한다.
- 브랜치를 push해 `ci` 실행이 생기고, 세 작업이 작업 459·460을 포함한 브랜치 head에서 도는지 본다. 초안 PR을 열어 `pull_request` 실행의 작업이 건너뛰어지는지도 본다.

*Load `ci.yml` as YAML and check the triggers, concurrency and each job's `if`; push the branch and see a `ci` run start with all three jobs on the branch head that includes tasks 459 and 460; open the draft PR and see the `pull_request` run's jobs skipped.*
