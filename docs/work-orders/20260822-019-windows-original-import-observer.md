# Windows Original Import Observer

## 한국어

1. Windows 전용 observer CLI를 추가합니다.
2. HDD root와 target profile로 `ez2dj1.exe`를 해석하고 PE32 metadata를 읽습니다.
3. Windows `NativeHelperBackend`로 preferred base 적재·시작합니다.
4. 첫 import gate를 JSON Lines로 기록하고 helper를 중지합니다.
5. synthetic 입력 단위 테스트 또는 Windows build로 인자·오류 경로를 검증합니다. 실제 HDD 실행은 사용자가 제공한 경로에서만 수행합니다.

## English

1. Add a Windows-only observer CLI.
2. Resolve `ez2dj1.exe` through HDD root and target profile, then read PE32 metadata.
3. Prepare and start it through Windows `NativeHelperBackend` at preferred base.
4. Record the first import gate as JSON Lines and stop the helper.
5. Verify argument/error paths with synthetic input or a Windows build. Run real HDD input only from a user-provided path.
