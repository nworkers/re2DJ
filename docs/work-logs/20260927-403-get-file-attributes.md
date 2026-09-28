# 작업 403 작업 로그 — GetFileAttributesA / Task 403 work log — GetFileAttributesA

설계: [20260927-403-get-file-attributes.md](../design/20260927-403-get-file-attributes.md) · 지시서: [20260927-403-get-file-attributes.md](../work-orders/20260927-403-get-file-attributes.md)

## 2026-09-27

- 경로 확인: continuation이 `GetFileAttributesA`의 문자열 인자를 기록하게 했다. 요청은 `..\..\ranking\ranking_StreetMix.bin`이고, CHD의 `EZ2DJ/SYSTEM/Ranking/ranking_StreetMix.bin`(79,992바이트)이다. 디렉터리 목록의 이름과 크기만 확인했고 내용은 읽지 않았다.
  *The continuation now records the string argument. The request is the CHD's `EZ2DJ/SYSTEM/Ranking/ranking_StreetMix.bin` (79,992 bytes); only the listing's names and sizes were read, not the contents.*
- 측정: 설계의 표와 같다.
  *Measured as in the design's table.*
- 첫 Windows VFS runtime probe 실행에서, 성공했는데 last error가 3으로 바뀌어 있었다. 내부의 오버레이 경로 조회(진짜 `GetFileAttributesA`)가 실패하면서 바꾼 값이었다. 호출 전 값을 되돌리게 고쳤다.
  *On the first Windows VFS runtime probe run a success left last error 3, set by the failing overlay lookup inside; the caller's value is now restored.*
- 결과: Windows x86 CTest 6개 통과(단위 테스트 4448 checks, VFS runtime probe 포함), Linux x64·x86 단위 테스트 4445 checks, 실패 0.
  *Windows x86: all 6 CTest tests pass (4448 unit checks, the VFS runtime probe included); Linux x64 and x86: 4445 unit checks; no failures.*
- Windows 실제 실행(기준 7d6ece3과 각 60초): 그래픽·VFS·runtime 기록에서 PID, 주소, 시간 값을 지우고 비교했다.
  - 차이는 다음뿐이다: `dynamic-resolver:name=GetFileAttributesA`의 `route=win32` → `route=hle`(의도한 변경), 창 제목의 FPS 자릿수, 오디오 lock 횟수(시간 의존, 이전 작업들과 같음).
  - 60초 안에는 `GetFileAttributesA` 호출이 없었다.

  *Windows real runs against baseline 7d6ece3 (60 s each), normalized for PIDs, addresses, and timing:*
  - *The only differences are the intended resolver route change, the FPS digits in the window title, and the timing-dependent audio lock counts seen in earlier tasks.*
  - *No `GetFileAttributesA` call happened within 60 s.*
- Linux 실제 실행(두 폭 각 150초): WSL 재시작으로 입력 보조 도구가 없어져 키 입력 없이 돌았다.
  - attract 모드가 랭킹 파일 조회를 지나 DEMO PLAY(자동 연주 화면)와 HIGH SCORE 표를 그렸다. 이어 타이틀로 돌아가 데모를 반복했다.
  - 멈추는 import 없이 창이 닫힐 때까지 돌았다(x64 호출 3,150,212번, x86 2,011,072번).

  *Linux real runs (both widths, 150 s each) ran without key input, as the key helper was gone after a WSL restart.*
  - *Attract mode got past the ranking lookup and drew DEMO PLAY with its HIGH SCORE table, then returned to the title and repeated the demo.*
  - *It ran until the window closed with no stopping import (3,150,212 calls on x64, 2,011,072 on x86).*
