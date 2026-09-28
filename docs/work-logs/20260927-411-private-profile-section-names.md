# 작업 411 작업 로그 — GetPrivateProfileSectionNamesA / Task 411 work log — GetPrivateProfileSectionNamesA

설계: [20260927-411-private-profile-section-names.md](../design/20260927-411-private-profile-section-names.md) · 지시서: [20260927-411-private-profile-section-names.md](../work-orders/20260927-411-private-profile-section-names.md)

## 2026-09-27

- 측정: 설계의 표와 같다.
  *Measured as in the design's table.*
- 결과: Windows x86 CTest 6개 통과(단위 4693 checks), Linux x64·x86 CTest 4개 통과(단위 4690 checks), 실패 0. 긴 실제 실행 회귀는 하지 않았다.
  *Windows x86: all 6 CTest tests pass (4693 unit checks); Linux x64 and x86: all 4 CTest tests pass (4690 unit checks); no failures. No long real-run regression.*
- Linux 1st(x64): `Songs\music.ini`의 섹션 이름으로 167을 돌려준다. Windows 기록과 같다. 이어서 `bookkeeping.ini`의 `[STATISTICS]`를 읽어 0, 12, 845, 3, 1을 얻는다. Windows 기록의 `SERVICECOIN`~`TOTALCONTINUE`와 같다. 그다음 호출 965번째 `ddraw!DirectDrawEnumerateA`(DirectDraw 1)에서 멈춘다.
  *Linux 1st (x64) gets 167 for `Songs\music.ini`'s section names, as in the Windows log, then reads `[STATISTICS]` from `bookkeeping.ini` as 0, 12, 845, 3, 1, the Windows log's `SERVICECOIN` through `TOTALCONTINUE`, and stops at `ddraw!DirectDrawEnumerateA` (DirectDraw 1, call 965).*
