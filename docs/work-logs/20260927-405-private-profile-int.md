# 작업 405 작업 로그 — GetPrivateProfileIntA와 디렉터리 덤프 파일 / Task 405 work log — GetPrivateProfileIntA and directory-dump files

설계: [20260927-405-private-profile-int.md](../design/20260927-405-private-profile-int.md) · 지시서: [20260927-405-private-profile-int.md](../work-orders/20260927-405-private-profile-int.md)

## 2026-09-27

- 측정: 설계의 표와 같다. 빈 키(`""`) 조회는 Windows 11에서 돌아오지 않아 측정 항목에서 뺐다.
  *Measured as in the design's table. A lookup with an empty key did not return on Windows 11 and was dropped from the probe.*
- 1st의 Windows 기록에 있는 이름은 동적 resolver가 가로챈 것뿐이었다. 전체 목록은 원래 프로그램의 `.idata` 섹션에 평문으로 남은 import 이름에서 얻었다. facade에 없는 33개를 resolve-only로 등록했다.
  *1st's Windows log listed only the names its dynamic resolver routes; the full list came from the import names left in plain text in the original program's `.idata` section, and the 33 the facades lacked were registered resolve-only.*
- 실제 `bookkeeping.ini`를 공용 core로 읽은 12개 값(`PlayCoins` 2, `DemoVolume` 3, `TOTALCOIN` 12 등)이 Windows 제품 기록과 모두 같다. Windows 제품은 정책을 core로 옮긴 뒤에도 같은 값을 기록했다.
  *The 12 values the shared core reads from the real `bookkeeping.ini` all match the Windows product's log, which records the same values after the policy moved to the core.*
- 결과: Windows x86 CTest 6개 통과(단위 4540 checks), Linux x64·x86 단위 4537 checks, 실패 0. 4th는 두 폭 모두 창을 닫을 때까지 돌았다.
  *Windows x86: all 6 CTest tests pass (4540 unit checks); Linux x64 and x86: 4537 unit checks; no failures. 4th runs until the window closes on both widths.*
- Linux 1st(두 폭): 이름 조회를 모두 지나 원래 프로그램의 CRT 시작 코드에 들어가고, `kernel32!InitializeCriticalSection`(호출 805번째)에서 멈춘다. `GetPrivateProfileIntA`는 아직 호출되지 않는다.
  *Linux 1st (both widths) gets past every name lookup into the original program's CRT startup and stops at `kernel32!InitializeCriticalSection` (call 805); `GetPrivateProfileIntA` is not called yet.*
