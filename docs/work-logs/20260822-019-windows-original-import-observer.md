# Windows Original Import Observer

## 한국어

### 결과

Windows 전용 `re2dj_windows_import_observer` CLI를 추가했습니다. `--hdd`, `--helper`, 선택 `--target`을 받아 HDD target profile의 bring-up executable을 해석하고, preferred base로 Windows native helper를 시작합니다. 첫 import gate의 event ID, IP, SP, module/name/ordinal을 JSON Lines로 출력하고 `kStop` completion으로 helper를 종료합니다.

### 검증

Visual Studio 2022 x64 Debug에서 `re2dj_windows_import_observer` target을 단일 MSBuild worker로 빌드했습니다. 성공한 산출물은 `build/vs2022_debug/bin/Debug/re2dj_windows_import_observer.exe`입니다.

원본 HDD 디렉터리는 이 작업 환경에 제공되지 않았으므로 실제 `ez2dj1.exe` gate 관찰은 수행하지 않았습니다.

## English

### Result

Added the Windows-only `re2dj_windows_import_observer` CLI. It accepts `--hdd`, `--helper`, and optional `--target`, resolves the bring-up executable through HDD target profiles, and starts the Windows native helper at the preferred base. It emits the first import gate's event ID, IP, SP, module/name/ordinal as JSON Lines and stops the helper with `kStop` completion.

### Verification

Built the `re2dj_windows_import_observer` target with one MSBuild worker in Visual Studio 2022 x64 Debug. The resulting executable is `build/vs2022_debug/bin/Debug/re2dj_windows_import_observer.exe`.

No original HDD directory was provided in this environment, so no real `ez2dj1.exe` gate observation was run.
