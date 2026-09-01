# CHD staging Windows launcher 프로파일 handoff 작업 지시

## 한국어

### 목표

CHD 실행 시 부모 CLI가 선택한 `ez2dj4th` 프로파일과 `EZ2DJ/EZ2DJ.EXE` 경로를 launcher에 전달하여 staging 재스캔으로 인한 target 미발견을 제거합니다.

### 작업 범위

1. `OriginalProcessOptions`와 인자 생성기에 explicit executable 상대 경로를 추가합니다.
2. CHD 실행 경로에서 부모 profile 경로를 `--target-executable`로 전달합니다.
3. Windows launcher parser와 target 선택 로직에서 explicit path handoff를 처리합니다.
4. product-loader probe와 기존 단위 빌드로 인자 전달 및 회귀를 검증합니다.
5. 실제 CHD 실행의 다음 경계를 작업 로그와 분석 문서에 기록합니다.

### 제외 범위

FAT32 reader, CHD codec, VFS 파일 의미, Hardlock 응답, 일반 디렉터리 target 매칭 정책은 변경하지 않습니다.

## English

### Goal

Pass the parent CLI's selected `ez2dj4th` profile and `EZ2DJ/EZ2DJ.EXE` path into the launcher for CHD runs, avoiding target loss caused by rescanning the staging directory.

### Scope

1. Add an explicit executable-relative path to `OriginalProcessOptions` and argument construction.
2. Forward the parent profile path as `--target-executable` for CHD execution.
3. Handle the explicit-path handoff in the Windows launcher parser and target selection.
4. Verify argument forwarding and regressions with the product-loader probe and existing builds.
5. Record the next boundary from a real CHD execution in the work log and analysis documentation.

### Out of scope

Do not change the FAT32 reader, CHD codec, VFS file semantics, Hardlock response, or normal directory-target matching policy.
