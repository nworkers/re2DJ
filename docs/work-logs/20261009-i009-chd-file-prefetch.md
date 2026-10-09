# #9 작업 로그 — CHD 파일 백그라운드 미리 읽기 / #9 work log — prefetching CHD files in the background

설계: [20261009-i009-chd-file-prefetch.md](../design/20261009-i009-chd-file-prefetch.md) · 지시서: [20261009-i009-chd-file-prefetch.md](../work-orders/20261009-i009-chd-file-prefetch.md)

## 2026-10-09 — 원인 조사 / Investigation

- 증상: Windows에서 6th SpaceMix를 플레이하면 중간에 멈칫거리고 배경음이 끊길 때가 있다. CHD(`roms/ez2dj6th/6th.chd`, 4.4GB, 4KB hunk, LZMA)는 E: HDD(WD Red 4TB)에 있다.
- 임시 계측(커밋하지 않음): 40ms를 넘는 프레임과 그 프레임의 import 시간 상위 3개, 25ms를 넘는 import, CHD hunk 캐시 적중·실패와 실패 시간, overlay 경로 조회 시간, 배경음 스트림의 덮어쓰기(SDL이 이미 가져간 구간을 게임이 바꿈)와 SDL이 가져가는 간격.
- 확인한 것
  - 6th에는 소리 스레드가 없다. 메인 스레드가 매 프레임 배경음 버퍼(360,448바이트, 약 2.04초)의 재생 위치를 보고, 약 133ms마다 `.ezw`에서 22,528바이트를 읽어 쓴다. 재생 위치보다 약 1.5초 앞선다.
  - 전체 API 기록(`--api-log-calls 0`)으로 본 한 곡: 곡 전 로딩 2.4초 동안 파일 437개를 열고, 플레이 70초 동안은 `bgm.ezw` 읽기 532번(12.3MB)뿐이다.
  - 느린 `ReadFile`의 시간은 거의 전부 hunk 읽기(`chd_read`)였다. 처음 읽는 조각은 25~30ms, 한 실행에서는 482ms까지 걸렸고 그만큼 프레임이 밀렸다. 같은 구간을 다시 실행하면 사라진다(Windows 파일 캐시).
  - 배경음 덮어쓰기와 SDL 공백은 한 번도 없었다. 끊김은 메인 스레드 멈춤이 1.5초 앞섬보다 길 때 나는 것으로 추정한다(화면 전환에서 1.6초 멈춤이 한 번 있었다).

  *Symptom: on Windows, 6th's SpaceMix stutters now and then and the background music can break up; the CHD (`roms/ez2dj6th/6th.chd`, 4.4 GB, 4 KB hunks, LZMA) is on the E: HDD (WD Red 4 TB). Temporary instrumentation (not committed) logged frames over 40 ms with their top three imports by time, imports over 25 ms, CHD hunk-cache hits, misses and miss time, overlay lookup time, and on the music stream both overwrites (the game changing bytes SDL had already taken) and the gaps between SDL's pulls. Findings: 6th has no sound thread; its main thread checks the play cursor of the music buffer (360,448 bytes, about 2.04 s) every frame and writes 22,528 bytes read from an `.ezw` about every 133 ms, about 1.5 s ahead of the cursor. A full API record (`--api-log-calls 0`) of one song shows 437 files opened in the 2.4 s of loading and, over 70 s of play, only 532 reads of `bgm.ezw` (12.3 MB). Nearly all the time of slow `ReadFile`s went to hunk reads (`chd_read`): 25 to 30 ms for a piece read for the first time, up to 482 ms in one run, the frame slipping as much; running the same stretch again shows none (the Windows file cache). There was never an overwrite or an SDL gap; the music presumably breaks up when the main thread stalls longer than its 1.5 s lead (one screen transition stalled 1.6 s).*

## 2026-10-09 — 구현 / Implementation

- `include/re2dj/hle/guest_file_prefetcher.h`, `src/hle/guest_file_prefetcher.cpp`: 작업 스레드 하나와 FIFO 큐, `Job`(버퍼, atomic `loaded`, 취소·실패·완료), `Start`·`Copy`·`Cancel`, 256MiB 한도, `BeginForeground`/`EndForeground`. 시작·끝·실패·중단을 로그에 남긴다.
- `GuestFiles`: 이미지 파일의 첫 `Read` 뒤 256KiB 이상 남으면 시작(`kPrefetchMinimumRemaining`), `Read`가 미리 읽은 범위를 먼저 사용, `Close`가 취소, `PrefetchedBytes`·`WaitForPrefetch`(테스트). source는 `ForegroundSource` wrapper로 감싸 게스트 호출을 표시한다.
- 첫 실제 실행에서 모드 선택 진입 때 작업 스레드가 `ModeSelect.ezw`(14MB)를 읽는 동안, 게임의 4KB `ReadFile`이 273ms, `CreateFileA`가 203ms를 기다렸다. 작업 스레드가 CHD 잠금을 조각마다 바로 다시 잡았기 때문으로 보고, 게스트 호출 중에는 다음 조각을 시작하지 않게 했다(커밋 `35ccb74`).
- 테스트 `tests/unit/guest_file_prefetcher_test.cpp`: 나눠 읽는 파일의 미리 읽기와 이후 source 미호출·내용 일치, 한 번에 읽는 파일과 작은 파일 제외, 작업 스레드를 붙잡은 동안의 직접 읽기와 `Close` 뒤 한 조각에서 멈춤, 한도와 실패, 게스트 호출 중 대기, 붙잡힌 상태에서의 종료.

  *`guest_file_prefetcher.{h,cpp}`: one worker thread and a FIFO queue, `Job` (buffer, atomic `loaded`, cancelled, failed, finished), `Start`, `Copy`, `Cancel`, a 256 MiB budget, and `BeginForeground`/`EndForeground`, logging start, end, failure and stop. `GuestFiles` starts a job when at least 256 KiB of an image file remain after its first `Read` (`kPrefetchMinimumRemaining`), serves `Read` from the prefetched range first, cancels on `Close`, offers `PrefetchedBytes` and `WaitForPrefetch` for tests, and wraps the source in `ForegroundSource` to mark guest calls. In the first real run, while the worker read `ModeSelect.ezw` (14 MB) on entering mode select, a 4 KB guest `ReadFile` waited 273 ms and a `CreateFileA` 203 ms, the worker presumably retaking the CHD lock right after each piece; the worker now starts no piece while a guest call is under way (commit `35ccb74`). Tests in `guest_file_prefetcher_test.cpp`: prefetching a file read in pieces, then no source calls and the same bytes; no prefetch for a file read whole or a small one; direct reads while the worker is held and a stop after one piece on `Close`; the budget and failure; waiting during a guest call; and shutting down while held.*

## 검증 / Verification

- Windows x86 Debug(MSVC, 경고를 오류로): 빌드 성공, `re2dj_unit_tests` 6,193개 검사 통과, 30번 반복해 실패 없음, CTest 4개 통과. Release 빌드 성공.
- WSL Linux x64·x86 Debug(경고를 오류로): 빌드 성공, CTest 각 5개 통과.
- 실제 실행(Windows Release, CHD가 HDD, 임시 계측을 다시 얹은 빌드)
  - 1차(사용자 SpaceMix 플레이, `bgm-01.ezw` 16.9MB): 곡 시작 직전 167ms에 다 읽음. 플레이 약 1분 40초 동안 25ms를 넘는 `ReadFile`과 40ms를 넘는 프레임이 없었다. 수정 전 같은 상황에서는 25~480ms 읽기가 있었다.
  - 2차(작업 스레드 양보 수정 뒤, 어트랙트 데모 4곡이 35분 동안 9바퀴): 곡 배경음은 모두 곡 시작 전에 읽혔고(캐시 없는 첫 바퀴 152~1,015ms, 이후 150~300ms), 곡 재생 중 느린 읽기와 긴 프레임은 한 번도 없었다.
- **남은 것**
  - 곡 로딩·화면 전환의 멈춤은 그대로다(캐시 없는 첫 바퀴 1.1~2.9초). 처음 읽는 작은 파일(키음 등) 하나마다 HDD 접근 25~48ms가 쌓이기 때문이며 이 작업의 범위 밖이다.
  - `..\..\ranking\ranking_*.bin`을 여는 `CreateFileA`는 CHD를 읽지 않는데도 65~80ms 걸린다. 수정 전에도 같았다.
  - 2차 실행은 사람이 직접 플레이하지 않았다. 사용자가 플레이 중 끊김이 사라졌는지 다시 확인한다.

  *Verification: the Windows x86 Debug build (MSVC, warnings as errors) passes `re2dj_unit_tests` with 6,193 checks, 30 repeated runs without a failure, and 4 CTest tests, and the Release build passes; the WSL Linux x64 and x86 Debug builds (warnings as errors) pass 5 CTest tests each. Real runs (Windows Release, CHD on the HDD, with the temporary instrumentation put back): first, the user playing SpaceMix, `bgm-01.ezw` (16.9 MB) was read whole 167 ms before the song began, and over about 1 min 40 s of play no `ReadFile` took over 25 ms and no frame over 40 ms, where the same situation showed 25 to 480 ms reads before; second, after the worker learned to step aside, the attract's four demo songs looped 9 times over 35 minutes, every song's music was read before the song began (152 to 1,015 ms on the cold first loop, 150 to 300 ms after), and no slow read or long frame came during a song. Left: song loading and screen transitions still stall (1.1 to 2.9 s on the cold loop), each small file read for the first time (key sounds and the like) adding 25 to 48 ms of HDD access, outside this task; `CreateFileA` of `..\..\ranking\ranking_*.bin` takes 65 to 80 ms without reading the CHD, as before the change; and no person played in the second run, so the user checks again that the music no longer breaks up during play.*

## 2026-10-09 — 사용자 확인 / User check

- 임시 계측을 뺀 최종 Release(`8da38ac`)로 사용자가 6th SpaceMix를 세 곡(`blue`, `madrobot`, `lookout`) 플레이했다. 멈칫거림이나 배경음 끊김을 느끼지 못했다. 세 곡의 배경음(13.7~19.7MB)은 모두 곡 시작 전에 261~587ms 만에 다 읽혔고, 로그에 경고·오류가 없었다.

  *With the final Release (`8da38ac`, no temporary instrumentation) the user played three SpaceMix songs on 6th (`blue`, `madrobot`, `lookout`) and noticed no stutter or music break-up; each song's music (13.7 to 19.7 MB) was read whole 261 to 587 ms before the song began, and the log held no warning or error.*

## 2026-10-10 — Linux 실제 실행 / Real runs on Linux

- 환경: Ubuntu 데스크톱(WSL 아님), GCC 15.2, RTX 4090(NVIDIA 595.99.02), GNOME Wayland. CHD는 NVMe에 있다. HEAD `82d92e4`.
- 빌드·테스트: `linux-x64-release`·`linux-x86-release` 빌드 성공, CTest 각 5개 통과, `re2dj_unit_tests` checks 6,190, failures 0. 미리 읽기 테스트의 스레드 타이밍을 보려고 두 빌드를 각각 30번 반복했고 실패가 없었다.
- x64 Release 실제 실행(`--hdd roms/ez2dj6th --target ez2dj6th --run --windowed`, 사용자가 SpaceMix로 `blue`·`delight` 진행): 시작 27건 중 25건이 `prefetched`로 끝났다. 곡 배경음 `0-blue-bgm.ezw`(18.6MB) 1,287ms, `0-delight-bgm.ezw`(20.8MB) 448ms, `ModeSelect.ezw`(14.3MB) 103~106ms, `Title.ezw`(8.6MB) 51~60ms. 경고·오류 없이 창 닫힘으로 끝났다.
- x86 Release 실제 실행: 네이티브 Wayland 기본 설정에서는 `IDirectDraw7::SetCooperativeLevel`에서 멈췄다. 작업 459에 적은 NVIDIA 32비트 `egl-wayland2` 문제와 같다. 가이드의 우회 방법 1(`egl-wayland` v1 지정)로 다시 실행하니 `6getthebeatrm/bgm-01.ezw`(16.9MB)까지 293ms에 다 읽혔고, 경고·오류 없이 끝났다.
- 관찰
  - 끝 로그가 없던 2건(`1-fx-18.ezw`, `1-cym-02.ezw`)은 큰 배경음 뒤 큐에서 기다리다 게임이 `Close`해 취소된 작업이다. 작업 스레드는 큐에서 꺼낸 작업이 이미 취소됐으면 로그 없이 건너뛴다(`GuestFilePrefetcher::Run`). 동작은 의도대로이고 로그에 그 경우가 빠져 있을 뿐이다.
  - 아주 작은 파일은 `prefetched` 줄이 `prefetch` 줄보다 먼저 찍힐 수 있다(x86 실행의 `STAGE1.ezw`). `Start`가 잠금을 놓고 작업 스레드를 깨운 뒤 로그를 남기기 때문이다.
  - CHD가 NVMe에 있고 같은 파일을 여러 번 읽어 운영체제 캐시가 찼으므로, HDD에서의 멈춤 개선은 Linux에서 따로 재지 않았다.

  *Environment: an Ubuntu desktop (not WSL), GCC 15.2, RTX 4090 (NVIDIA 595.99.02), GNOME on Wayland, the CHD on NVMe, HEAD `82d92e4`. Build and tests: `linux-x64-release` and `linux-x86-release` build, pass 5 CTest tests each and report 6,190 checks with 0 failures in `re2dj_unit_tests`; to probe the prefetch tests' thread timing, each build ran them 30 times without a failure. The x64 Release real run (`--hdd roms/ez2dj6th --target ez2dj6th --run --windowed`, the user taking SpaceMix through `blue` and `delight`): 25 of 27 started jobs ended in `prefetched`; the song music `0-blue-bgm.ezw` (18.6 MB) took 1,287 ms and `0-delight-bgm.ezw` (20.8 MB) 448 ms, `ModeSelect.ezw` (14.3 MB) 103 to 106 ms and `Title.ezw` (8.6 MB) 51 to 60 ms; the run ended on the window closing with no warning or error. The x86 Release real run stopped at `IDirectDraw7::SetCooperativeLevel` under the default native Wayland setup, the NVIDIA 32-bit `egl-wayland2` problem recorded in task 459; with the guide's first workaround (`egl-wayland` v1 pinned) it read `6getthebeatrm/bgm-01.ezw` (16.9 MB) whole in 293 ms and ended with no warning or error. Observations: the two jobs with no end line (`1-fx-18.ezw`, `1-cym-02.ezw`) waited in the queue behind large music and were cancelled by the game's `Close`; the worker skips a job already cancelled when it leaves the queue without logging (`GuestFilePrefetcher::Run`), so the behaviour is as intended and only the log misses that case. For a very small file the `prefetched` line can come before the `prefetch` line (`STAGE1.ezw` in the x86 run), since `Start` logs after releasing the lock and waking the worker. With the CHD on NVMe and the OS cache warm from repeated reads, the HDD stall improvement was not measured again on Linux.*
