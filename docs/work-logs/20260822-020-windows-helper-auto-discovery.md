# Windows Helper Auto-Discovery

## 한국어

`re2dj_windows_import_observer`에서 `--helper`를 선택 override로 변경했습니다. 기본 경로는 observer executable과 같은 디렉터리의 `re2dj_native_ipc_helper.exe`, 이어서 `helpers/win32/re2dj_native_ipc_helper.exe`입니다.

Windows Visual Studio 2022 x64 Debug에서 observer target을 단일 MSBuild worker로 빌드했습니다. 원본 HDD 및 packaged helper layout은 이 환경에 없으므로 실제 자동 탐색 실행은 수행하지 않았습니다.

## English

Changed `--helper` in `re2dj_windows_import_observer` to an optional override. Default discovery checks `re2dj_native_ipc_helper.exe` beside the observer executable, then `helpers/win32/re2dj_native_ipc_helper.exe`.

Built the observer target with one MSBuild worker in Windows Visual Studio 2022 x64 Debug. No original HDD or packaged helper layout is available in this environment, so real automatic-discovery execution was not run.
