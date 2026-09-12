# 작업 지시서: GitHub Actions 릴리즈 CI 구성

## 관련 설계

[GitHub Actions 릴리즈 CI와 릴리즈 노트 설계](../design/20260913-261-github-release-ci.md)

## 작업 범위

1. `rePIU`의 공개 `release.yml`과 현재 `re2DJ` CI/Release build 구조를 비교한다.
2. Windows x86 Release 출력만 포함하는 package script를 추가한다.
3. `v*` tag push와 수동 실행을 지원하는 `.github/workflows/release.yml`을 추가한다.
4. `VERSION`-tag 검증, artifact 업로드, 기존 release 갱신, handwritten/generated release notes 선택을 구현한다.
5. `docs/release-notes/` 입력 규칙과 scripts 사용법을 문서화한다.
6. Release build/package 검증과 문서 작업 로그를 남기고 커밋한다.

## 제외 범위

- GitHub 원격 tag/release의 실제 publish
- repository secret 또는 GitHub 환경 설정 변경
- 원본 HDD/CHD asset 포함
- 게임 runtime/HLE 동작 변경
- Linux/Web 배포 archive 추가

## 완료 조건

- tag push가 version gate를 통과한 Windows x86 Release package를 만든다.
- package에는 `re2dj.exe`, injected runtime DLL, 예제 config와 사용자 문서가 포함된다.
- `docs/release-notes/vX.Y.Z.md`가 있으면 해당 노트로 Release가 생성되고, 없으면 GitHub generated notes가 사용된다.
- 수동 workflow 실행은 Release를 만들지 않고 artifact를 남긴다.
- 로컬 package 검증과 YAML/스크립트 문법 검증 결과를 작업 로그에 기록한다.

## English

### Related design

[GitHub Actions release CI and release-note design](../design/20260913-261-github-release-ci.md)

### Scope

1. Compare the public `rePIU` `release.yml` with the current `re2DJ` CI and Release-build structure.
2. Add a package script containing only the Windows x86 Release output needed by users.
3. Add `.github/workflows/release.yml` for `v*` tag pushes and manual runs.
4. Implement the `VERSION`/tag gate, artifact upload, existing-release update, and handwritten/generated release-note selection.
5. Document the `docs/release-notes/` input convention and script usage.
6. Verify the Release build/package, leave a work log, and commit the task.

### Out of scope

- Publishing a real remote GitHub tag or Release from this local task
- Changing repository secrets or GitHub environment settings
- Including original HDD/CHD assets
- Changing game runtime/HLE behavior
- Adding Linux/Web distribution archives

### Completion criteria

- A tag push creates a version-gated Windows x86 Release package.
- The package contains `re2dj.exe`, the injected runtime DLL, example configuration, and user documentation.
- `docs/release-notes/vX.Y.Z.md` is used when present; otherwise GitHub-generated notes are used.
- A manual workflow run uploads artifacts without creating a Release.
- Local package and YAML/script syntax verification are recorded in the work log.
