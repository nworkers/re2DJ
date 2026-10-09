# #3 설계 — 릴리스 노트에 해결된 이슈와 커밋 ID 표시 / #3 design — resolved issues and commit IDs in release notes

이슈: [#3](https://github.com/reexec/re2DJ/issues/3) · 선행: [#1 설계(이슈·PR 기반 작업 규칙)](20261005-i001-branch-ci-and-issue-workflow.md)

## 배경 / Background

#1부터 작업은 GitHub 이슈이고 `main` 머지는 PR squash merge다. 그런데 v0.0.65 노트(`RELEASE_NOTES.md`, `docs/release-notes/v0.0.65.md`)에는 해결한 이슈 #1도, 커밋 ID도 없었다. 커밋 ID에는 순서 문제가 있다. squash 커밋은 머지가 끝나야 ID가 생기고, 노트 파일은 그 커밋 안에 들어 있으므로 자기 ID를 미리 적을 수 없다. GitHub Release 본문은 `release.yml`의 `publish`가 태그 시점의 `docs/release-notes/<tag>.md`로 만든다. 같은 파일을 프로젝트 사이트의 릴리스 타임라인도 쓴다.

*Since #1, tasks are GitHub issues and merges into `main` are PR squash merges, yet the v0.0.65 notes (`RELEASE_NOTES.md`, `docs/release-notes/v0.0.65.md`) named neither the resolved issue #1 nor any commit ID. Commit IDs have an ordering problem: a squash commit gets its ID only when the merge is done, and the note file lives inside that commit, so it cannot carry its own ID in advance. The GitHub Release body comes from `docs/release-notes/<tag>.md` at the tag through `release.yml`'s `publish` job, and the project site's release timeline reads the same file.*

## 결정 / Decisions

1. **노트 파일에 적는 것**: 머지 전에 알 수 있는 것, 곧 해결한 이슈 `#N`(제목)과 PR 번호다. `docs/release-notes/vX.md`의 한국어·영어 본문과 `RELEASE_NOTES.md`의 해당 버전 절에 "해결된 이슈 / Resolved issues"를 둔다. 해결한 이슈가 없으면 "없음 / None"이라고 적는다.
2. **Release 본문에 자동으로 붙이는 것**: `scripts/release/release_refs.py <tag>`가 직전 `v*` 태그부터 `<tag>`까지(`git log prev..tag`)의 커밋을 읽어 Markdown 절을 만든다.
   - 이슈: 커밋 본문의 `Closes`·`Fixes`·`Resolves #N`(대소문자와 활용형 포함)을 모은다. 제목은 `gh issue view`로 가져오고, 가져오지 못하면 번호만 적는다.
   - PR: `gh api repos/<repo>/commits/<sha>/pulls`로 찾는다.
   - 커밋: 짧은 ID(링크)와 제목을 적는다.
   - 출력 형식:
     ```markdown
     ## 해결된 이슈와 커밋 / Resolved issues and commits

     | 커밋 / Commit | 제목 / Subject | 이슈 / Issues | PR |
     | --- | --- | --- | --- |
     | [`26a9151`](…/commit/…) | … | [#1](…/issues/1) … | [#2](…/pull/2) |

     비교 / Compare: [v0.0.64...v0.0.65](…/compare/v0.0.64...v0.0.65)
     ```
   - `publish`는 노트 파일 뒤에 이 절을 붙여 Release를 만들거나 고친다. `publish`의 checkout은 이력과 태그가 필요하므로 `fetch-depth: 0`으로 한다. 노트 파일이 없으면 이 절만으로 본문을 만든다(이전의 `--generate-notes` 대체).
   - 노트 파일 자체는 바꾸지 않으므로, 사이트 타임라인은 1번의 이슈 목록을 보여 준다.
3. **이미 있는 v0.0.65**: 태그 커밋(`26a9151`)의 `release.yml`은 이 절을 만들지 않는다. 사용자가 태그를 push해 Release가 올라가면, 이 작업이 `main`에 들어간 뒤 `scripts/release/release_refs.py v0.0.65`의 출력을 붙여 `gh release edit v0.0.65 --notes-file`로 본문을 고친다. 노트 파일과 `RELEASE_NOTES.md`의 v0.0.65 절에는 이 작업에서 해결된 이슈 #1과 PR #2, 커밋 `26a9151`을 적는다(이미 머지된 버전이라 ID를 알고 있다).
4. **규칙**: `AGENTS.md` Git 규칙과 `docs/release-notes/README.md`에 1·2를 적는다.

*1. The note files carry what is known before the merge: the resolved issues `#N` with titles and the PR number, as a "Resolved issues" part in both languages of `docs/release-notes/vX.md` and in that version's section of `RELEASE_NOTES.md`, or "None" when nothing was resolved. 2. The Release body gets an automatic section: `scripts/release/release_refs.py <tag>` reads the commits from the previous `v*` tag to `<tag>` (`git log prev..tag`), gathers issues from `Closes`/`Fixes`/`Resolves #N` in commit bodies (any case and inflection), titled through `gh issue view` or left as numbers when that fails, finds PRs through `gh api repos/<repo>/commits/<sha>/pulls`, and writes the Markdown section above with each commit's short ID (linked) and subject plus a compare link. `publish` appends it to the note file when creating or editing the Release, checks out with `fetch-depth: 0` for the history and tags, and uses the section alone when there is no note file (replacing `--generate-notes`); the note file itself is unchanged, so the site timeline shows the issue list from 1. 3. v0.0.65 already exists: the `release.yml` at its tag commit (`26a9151`) does not build this section, so once the user pushes the tag and the Release is up, and this task is in `main`, the body is edited with `gh release edit v0.0.65 --notes-file` adding the output of `scripts/release/release_refs.py v0.0.65`; its note file and `RELEASE_NOTES.md` section name issue #1, PR #2 and commit `26a9151` in this task, as that version is already merged. 4. The rules for 1 and 2 go into the Git rules of `AGENTS.md` and into `docs/release-notes/README.md`.*

## 검증 / Verification

- `release_refs.py v0.0.65`가 로컬 태그로 #1·PR #2·`26a9151`이 담긴 표를 만드는지 본다. 이슈 표기가 없는 이전 범위(`v0.0.64`)에서는 "없음"으로 나오는지, `gh` 없이도 번호만으로 동작하는지도 본다.
- `release.yml`을 YAML로 읽고, `publish` 단계의 셸 부분을 `bash -n`으로 확인한다. 브랜치 push로 CI를 확인한다.

*Run `release_refs.py v0.0.65` against the local tag and check the table holds #1, PR #2 and `26a9151`; check that an earlier range without issue references (`v0.0.64`) gives "None", and that it works with numbers only without `gh`. Load `release.yml` as YAML, check the `publish` step's shell with `bash -n`, and check CI through the branch push.*
