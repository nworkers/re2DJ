# 릴리즈 노트 입력 / Release-note input

`v*` tag를 push하면 GitHub Actions Release workflow가 실행됩니다. 특정 릴리즈에 사람이 작성한 노트를 사용하려면 tag 이름과 같은 파일을 추가합니다.

```text
docs/release-notes/v0.0.40.md
```

파일이 있으면 해당 내용이 GitHub Release body로 사용되고, 없으면 GitHub가 이전 릴리즈 이후의 commit을 바탕으로 자동 생성한 노트를 사용합니다. 기존 Release가 있으면 tag의 asset은 갱신되며, handwritten notes가 있는 경우 body도 갱신됩니다.

수동 `workflow_dispatch` 실행은 Release를 publish하지 않고 workflow artifact만 생성합니다. 원본 HDD, CHD, 실행 파일 asset은 릴리즈 노트나 package에 포함하지 않습니다.

## English

Pushing a `v*` tag starts the GitHub Actions Release workflow. To provide handwritten notes for a release, add a file whose name matches the tag:

```text
docs/release-notes/v0.0.40.md
```

When the file exists, its contents become the GitHub Release body. Otherwise GitHub generates notes from commits since the previous release. If the Release already exists, tag assets are refreshed and handwritten notes replace the body when present.

A manual `workflow_dispatch` run builds and uploads a workflow artifact but does not publish a Release. Original HDDs, CHDs, and game executable assets are never included in release notes or packages.
