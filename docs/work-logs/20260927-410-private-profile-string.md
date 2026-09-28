# 작업 410 작업 로그 — GetPrivateProfileStringA / Task 410 work log — GetPrivateProfileStringA

설계: [20260927-410-private-profile-string.md](../design/20260927-410-private-profile-string.md) · 지시서: [20260927-410-private-profile-string.md](../work-orders/20260927-410-private-profile-string.md)

## 2026-09-27

- 측정: 설계의 표와 같다.
  *Measured as in the design's table.*
- `GetPrivateProfileIntA`를 넣을 때 `GetFileAttributesA`의 주석 아래에 끼워 넣어, 그 주석이 엉뚱한 함수 위에 있었다. 파일 읽기를 떼어 내면서 바로잡았다.
  *When `GetPrivateProfileIntA` went in, it landed below `GetFileAttributesA`'s comment; moving the file reading out put that comment back on its function.*
- 결과: Windows x86 CTest 6개 통과(단위 4682 checks), Linux x64·x86 CTest 4개 통과(단위 4679 checks), 실패 0. 긴 실제 실행 회귀는 하지 않았다.
  *Windows x86: all 6 CTest tests pass (4682 unit checks); Linux x64 and x86: all 4 CTest tests pass (4679 unit checks); no failures. No long real-run regression.*
- Linux 1st(x64): `ez2dj.ini`의 `[DIFFICULTY]` 값을 읽는다. 돌려준 길이 25, 25, 25, 24, 24, 19, 24는 Windows 기록의 값들(25·24·19)과 맞는다. 이어서 호출 959번째 `kernel32!GetPrivateProfileSectionNamesA`에서 멈춘다.
  *Linux 1st (x64) reads `[DIFFICULTY]` from `ez2dj.ini`, with lengths 25, 25, 25, 24, 24, 19, 24 matching the Windows log's values, and stops at `kernel32!GetPrivateProfileSectionNamesA` (call 959).*
