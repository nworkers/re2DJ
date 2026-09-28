# 작업 408 작업 로그 — EnumDisplaySettingsA와 ChangeDisplaySettingsExA / Task 408 work log — EnumDisplaySettingsA and ChangeDisplaySettingsExA

설계: [20260927-408-display-settings.md](../design/20260927-408-display-settings.md) · 지시서: [20260927-408-display-settings.md](../work-orders/20260927-408-display-settings.md)

## 2026-09-27

- Windows 제품의 1st 기록: `ChangeDisplaySettingsExA`가 흡수되었고 요청은 640×480×16, `CDS_FULLSCREEN`이었다.
  *The Windows product's 1st log shows `ChangeDisplaySettingsExA` absorbed, asking for 640x480x16 with `CDS_FULLSCREEN`.*
- 측정: 설계의 표와 같다. 바이트 지도를 얻을 때 bash heredoc이 C 문자열의 `\n`을 실제 줄바꿈으로 바꿔 한 번 빌드가 실패했다(알려진 함정). Edit 도구로 고쳤다.
  *Measured as in the design's table. Once, a bash heredoc turned a `\n` in a C string into a real newline (a known pitfall); the Edit tool fixed it.*
- 결과: Windows x86 CTest 6개 통과(단위 4641 checks), Linux x64·x86 CTest 4개 통과(단위 4638 checks), 실패 0. 사용자 지시대로 긴 실제 실행 회귀는 하지 않았다.
  *Windows x86: all 6 CTest tests pass (4641 unit checks); Linux x64 and x86: all 4 CTest tests pass (4638 unit checks); no failures. No long real-run regression, per the user's direction.*
- Linux 1st(x64, WSLg): `EnumDisplaySettingsA`가 host 데스크톱 모드로 답하고, 게임이 같은 DEVMODE로 `ChangeDisplaySettingsExA`를 부른다. 이것이 흡수된 뒤 호출 943번째 `kernel32!Sleep(3000)`에서 멈춘다.
  *Linux 1st (x64, WSLg): `EnumDisplaySettingsA` answers with the host desktop mode, the game calls `ChangeDisplaySettingsExA` with the same DEVMODE, which is absorbed, and it stops at `kernel32!Sleep(3000)` (call 943).*
