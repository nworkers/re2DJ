# Windows Helper Staging

## 한국어

`stage_windows_native_helper.ps1`을 추가했습니다. 이 script는 `windows-x64-debug`의 observer와 `windows-x86-native-probe`의 Win32 helper를 빌드한 뒤 helper를 다음 위치로 staging합니다.

```text
build/windows-x64/bin/Debug/helpers/win32/re2dj_native_ipc_helper.exe
```

실행 결과 staging된 helper 크기는 236,544 bytes였습니다. 이 경로는 observer의 기본 helper 탐색 두 번째 후보와 일치합니다.

## English

Added `stage_windows_native_helper.ps1`. It builds the `windows-x64-debug` observer and `windows-x86-native-probe` Win32 helper, then stages the helper at:

```text
build/windows-x64/bin/Debug/helpers/win32/re2dj_native_ipc_helper.exe
```

The staged helper was 236,544 bytes. This path matches the observer's second default helper-discovery candidate.
