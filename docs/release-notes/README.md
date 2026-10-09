# 릴리즈 노트 입력 / Release-note input

`v*` tag를 push하면 GitHub Actions Release workflow가 실행됩니다. 특정 릴리즈에 사람이 작성한 노트를 사용하려면 tag 이름과 같은 파일을 추가합니다.

```text
docs/release-notes/v0.0.40.md
```

파일이 있으면 그 내용 뒤에 "해결된 이슈와 커밋 / Resolved issues and commits" 표를 붙여 GitHub Release body로 씁니다. 표는 `scripts/release/release_refs.py <tag>`가 직전 `v*` 태그 이후의 커밋에서 만듭니다. 각 행은 짧은 커밋 ID(링크), 제목, 커밋 본문의 `Closes #N`으로 닫은 이슈, PR입니다. 파일이 없으면 이 표만으로 body를 만듭니다. 기존 Release가 있으면 tag의 asset과 body를 갱신합니다.

노트 파일에는 한국어·영어 본문 각각에 "해결된 이슈 / Resolved issues" 절을 두고, 해결한 이슈 `#N`(제목)과 PR 번호를 링크로 적습니다. 해결한 이슈가 없으면 "없음"이라고 적습니다. squash 커밋 ID는 머지 뒤에야 생기므로 파일에는 적지 않고, 위 표가 Release body에 보여 줍니다. 이미 올라간 Release의 body는 `python3 scripts/release/release_refs.py <tag>` 출력을 노트 파일 뒤에 붙여 `gh release edit <tag> --notes-file`로 고칠 수 있습니다.

수동 `workflow_dispatch` 실행은 Release를 publish하지 않고 workflow artifact만 생성합니다. 원본 HDD, CHD, 실행 파일 asset은 릴리즈 노트나 package에 포함하지 않습니다.

이 노트는 프로젝트 사이트(<https://reexec.github.io/re2DJ/>)의 릴리스 타임라인에도 게시됩니다. 한국어 본문 뒤 `## English` 아래 영어 본문을 두면 두 언어 페이지로 나뉩니다. 자세한 내용은 [docs/sites/README.md](../sites/README.md)를 참고하십시오.

## English

Pushing a `v*` tag starts the GitHub Actions Release workflow. To provide handwritten notes for a release, add a file whose name matches the tag:

```text
docs/release-notes/v0.0.40.md
```

When the file exists, its contents followed by a "Resolved issues and commits" table become the GitHub Release body. `scripts/release/release_refs.py <tag>` builds the table from the commits since the previous `v*` tag, one row per commit: the short ID (linked), the subject, the issues its body closes with `Closes #N`, and the PR. Without a file, the table alone is the body. If the Release already exists, its assets and body are refreshed.

Note files carry a "해결된 이슈 / Resolved issues" part in both the Korean and English bodies, linking each resolved issue `#N` (with its title) and the PR number, or "None" when nothing was resolved. A squash commit's ID exists only after the merge, so files do not hold it; the table shows it in the Release body. An existing Release's body can be fixed with `gh release edit <tag> --notes-file` on the note file followed by the output of `python3 scripts/release/release_refs.py <tag>`.

A manual `workflow_dispatch` run builds and uploads a workflow artifact but does not publish a Release. Original HDDs, CHDs, and game executable assets are never included in release notes or packages.

These notes are also published on the project site's release timeline (<https://reexec.github.io/re2DJ/>). Put the English body under a `## English` line after the Korean body to split the two language pages; see [docs/sites/README.md](../sites/README.md).
