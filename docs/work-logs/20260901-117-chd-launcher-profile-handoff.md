# CHD staging Windows launcher 프로파일 handoff 작업 로그

## 한국어

### 작업 요약

부모 제품 CLI가 FAT32 CHD에서 확인한 `ez2dj4th` 프로파일과 `EZ2DJ/EZ2DJ.EXE` 경로를 Windows x86 launcher에 명시적으로 전달하도록 연결했습니다. 기존 launcher가 임시 staging 디렉터리를 다시 검색하면서 CHD 전용 프로파일을 잃어버리던 문제를 제거했습니다.

### 변경 사항

- `OriginalProcessOptions`에 staging root 기준 executable 상대 경로를 추가했습니다.
- CHD 실행 시 부모 CLI가 `--target-executable EZ2DJ/EZ2DJ.EXE`를 launcher에 전달합니다.
- launcher는 명시 경로가 있으면 built-in `ez2dj4th` 프로파일을 복사하고 경로와 working directory를 채운 뒤 staging root 아래에서 안전하게 해석합니다.
- 절대 경로와 기존 `..` 탈출 거부 규칙을 유지했습니다.
- product-loader probe에 CHD 이미지와 explicit executable 인자 동시 전달 검증을 추가했습니다.
- 실제 실행 결과를 `docs/analysis/ez2dj4th-chd-filesystem.md`와 `docs/analysis/ez2dj-exe-structures.md`에 기록했습니다.

### 검증

다음 명령으로 Debug 빌드와 회귀 검사를 수행했습니다.

```powershell
cmake --build build\windows-x86 --config Debug --target re2dj_windows_product_loader_probe re2dj_windows_x86_launcher_probe re2dj re2dj_unit_tests
.\build\windows-x86\bin\Debug\re2dj_windows_product_loader_probe.exe
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
& .\build\windows-x86\bin\Debug\re2dj.exe ez2dj4th --run
```

- product-loader probe: `profile-defaults=ok unsupported-target=ok` 및 CHD handoff 검증 통과
- unit tests: `checks: 1030, failures: 0`
- 실제 CHD 실행: DLL 탐색 실패와 `cannot resolve valid bring-up target`가 사라지고, `runtime handoff was not observed before timeout` 단계까지 진행
- 진단 로그: `logs/windows_x86_launcher_probe/ez2dj4th/20260901-030400-180.jsonl`

현재 미확정 경계는 주입 runtime이 첫 `CreateFileA` 관찰 또는 다음 guest 제어 흐름으로 handoff되기 전 timeout이 발생하는 지점입니다. 화면 출력과 Hardlock 응답은 이 작업 범위에서 확정하지 않았습니다.

## English

### Summary

The parent product CLI now passes the selected `ez2dj4th` profile and `EZ2DJ/EZ2DJ.EXE` relative path from the FAT32 CHD to the Windows x86 launcher. This removes the failure caused by rescanning the temporary staging directory and losing a CHD-only profile.

### Changes

- Added a staging-root-relative executable path to `OriginalProcessOptions`.
- CHD launches forward `--target-executable EZ2DJ/EZ2DJ.EXE` to the launcher.
- With an explicit path, the launcher clones the built-in `ez2dj4th` profile, fills the path and working directory, and resolves the file safely below the staging root.
- Absolute paths and the existing `..` escape rejection remain enforced.
- Extended the product-loader probe to verify the CHD image and explicit executable arguments together.
- Recorded the real-run result in the CHD filesystem and executable analysis documents.

### Verification

The Debug build and regression checks used the commands above. The product-loader probe passed its profile-default and unsupported-target checks plus the CHD handoff assertion; unit tests reported `checks: 1030, failures: 0`. A real `ez2dj4th --run` no longer fails on runtime DLL discovery or target rediscovery and reaches `runtime handoff was not observed before timeout`.

The diagnostic log is `logs/windows_x86_launcher_probe/ez2dj4th/20260901-030400-180.jsonl`. The next unresolved boundary is the injected runtime handoff before the first observed `CreateFileA` or later guest control flow. Display output and Hardlock responses remain outside this task's confirmed scope.
