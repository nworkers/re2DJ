# Windows Helper Staging

## 한국어

1. Windows x64와 x86 helper preset을 빌드하는 PowerShell staging script를 추가합니다.
2. helper를 observer output의 `helpers/win32/`로 복사합니다.
3. helper 없이 observer를 실행해 자동 탐색 경로가 동작하는지 확인합니다.

## English

1. Add a PowerShell staging script that builds the Windows x64 and x86 helper presets.
2. Copy the helper to `helpers/win32/` below the observer output.
3. Run the observer without `--helper` to confirm default discovery.
