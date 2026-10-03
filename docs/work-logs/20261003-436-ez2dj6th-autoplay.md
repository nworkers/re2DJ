# 작업 436 작업 로그 — 6th와 Remember 1st의 autoplay / Task 436 work log — autoplay for 6th and Remember 1st

설계: [20261003-436-ez2dj6th-autoplay.md](../design/20261003-436-ez2dj6th-autoplay.md) · 지시서: [20261003-436-ez2dj6th-autoplay.md](../work-orders/20261003-436-ez2dj6th-autoplay.md) · 분석: [ez2dj6th-demo-play.md](../analysis/ez2dj6th-demo-play.md)

## 2026-10-03

- **준비**
  - 이 머신(Ubuntu 26.04)에는 pip·venv가 없어, Capstone 5.0.6 wheel(BSD-3-Clause)을 scratchpad에 풀어 `PYTHONPATH`로 썼다. 저장소와 시스템에는 설치하지 않았다.
  - `/tmp/re2dj/chd/ez2dj6th/`에 Linux 실행이 꺼내 둔 `EZ2DJ6th.EXE`(`0x411f6d44`)와 `EZ2DJ1ST/Ez2DJ.exe`(`0x411bbf5c`)는 보호 섹션 없이 `.text`·`.rdata`·`.data`뿐이었다. 그래서 파일을 섹션 배치대로 펼쳐 `logs/autoplay436/`에 두고 분석했다(`file_image.py`로 스킬에 추가).

  *Preparation: this machine (Ubuntu 26.04) has no pip or venv, so the Capstone 5.0.6 wheel (BSD-3-Clause) was unpacked into the scratchpad and used through `PYTHONPATH`, installed neither in the repository nor the system. `EZ2DJ6th.EXE` (`0x411f6d44`) and `EZ2DJ1ST/Ez2DJ.exe` (`0x411bbf5c`), taken out under `/tmp/re2dj/chd/ez2dj6th/` by a Linux run, have only `.text`, `.rdata` and `.data`, with no protection section, so the files were laid out by section under `logs/autoplay436/` and analysed (added to the skill as `file_image.py`).*

- **6th 분석**
  1. `find_strings.py`: `DEMOPLAY.bmp`가 `0x00415378`에서 참조된다. 그 앞의 `cmp [0x008895f8], 0`이 로드를 막는다.
  2. `xrefs.py 0x8895f8`: 쓰기 2곳(`0x0044c02c`, `0x0044c1de`), 읽기 15곳. 함수 경계를 못 잡는 최적화 빌드라 구간 역어셈블로 읽었다.
  3. 데모 시작 루틴 `0x0044c020`이 `[0x008895f8]`과 `[0x008896ac]`를 함께 1로 쓰고, 끝에서 함께 0으로 쓴다. `paired_writes.py`가 잡는 setter는 인라인되어 없었다.
  4. `xrefs.py 0x8896ac`: 쓰기 3곳, 읽기 17곳. `0x00413a2b`의 입력 슬롯 `0x1b` 뒤집기, 노트 데이터 저장 직후 읽기(`0x004225a6`)가 5th와 대응한다.
  5. Linux 읽기 전용 폴링 180초: 64.78초에 두 값이 함께 1, 108.55초에 함께 0.

  *6th analysis: `DEMOPLAY.bmp` is referenced at `0x00415378`, gated by `cmp [0x008895f8], 0`; that global has two writes (`0x0044c02c`, `0x0044c1de`) and fifteen reads, read by range disassembly since the optimised build gives no function bounds; the demo start routine `0x0044c020` writes `[0x008895f8]` and `[0x008896ac]` to 1 together and to 0 together at its end, with no setter for `paired_writes.py` to find since it is inlined; `[0x008896ac]` has three writes and seventeen reads, including the input-slot `0x1b` flip at `0x00413a2b` and the read right after storing note data (`0x004225a6`), matching 5th; a 180-second read-only poll on Linux saw both become 1 at 64.78 s and 0 at 108.55 s.*

- **Remember 1st 분석**: 장면 이름(`DemoGame`, `DemoPlayer`, `ClubMixDemoGame`, `ShowDemoPlay`)과 `EZModule(ezPlay)`가 1st Tracks와 같다. 곡 재생기 `0x00411a40`의 호출처 7곳 중 데모 장면 두 곳만 `push 1`이고, 나머지는 0 또는 0으로 만든 레지스터다. 인자 저장 `[0x0055795c]`의 참조 4곳은 모두 곡 진행 모듈 안이다. 1st Tracks의 결론(작업 304)과 같이 변수 없음으로 추정하고 선언하지 않았다.
  *Remember 1st analysis: the scene names (`DemoGame`, `DemoPlayer`, `ClubMixDemoGame`, `ShowDemoPlay`) and `EZModule(ezPlay)` match 1st Tracks; of the seven calls to the chart player `0x00411a40` only the two demo scenes push 1, the rest 0 or a zeroed register; all four references to the argument's store `[0x0055795c]` are inside the chart module. As with 1st Tracks (task 304) it is inferred to have no variable, and nothing is declared.*

- **`guest_memory.py` Linux 지원**: 처음 실행에서 두 문제가 있었다.
  - 프로세스 검색이 `--process EZ2DJ6TH.EXE`를 명령줄에 가진 상위 셸을 잡아 `/proc/<pid>/mem`이 거부됐다. 이제 `--launch`의 자손만, 아니면 자기 조상을 뺀 프로세스만 찾는다.
  - 찾자마자 읽어 게스트 이미지가 아직 매핑되지 않았다. 이제 첫 주소를 읽을 수 있을 때까지 `--wait` 동안 기다린다.
  - 끝날 때 launcher가 자식을 기다리므로, 띄운 트리를 자식부터 정리한다.

  *`guest_memory.py` on Linux: the first run found the parent shell, whose command line carried `--process EZ2DJ6TH.EXE`, and was refused `/proc/<pid>/mem`; the search now takes only `--launch`'s descendants, or else skips its own ancestors. It also read before the guest image was mapped; it now waits up to `--wait` until the first address reads. Since a launcher waits on its child, the launched tree is stopped child first.*

- **구현**
  - 공용 `target`: `game_controls`를 `std::vector<GameControls>`로 바꾸고 목록용 `ArmedAutoplayFlagRva`를 더했다. 6th에 `{0x004896ac, 0x411f6d44}`를 선언했다.
  - Linux: `src/platform/linux/game_controls.*`, `OriginalRunEnvironment::autoplay_flag_rva`, 실행기의 무장, OSD 토글, CLI의 `game controls` 로그 줄.
  - Windows: 주 debuggee의 `osd_controls` 진단을 목록에 맞췄다(`build_timestamp` 필드는 뺐다). `BootstrapChildHandoffOptions::game_controls`, 자식의 대상 id·실행 파일 이름·autoplay 주소 쓰기, `child_runtime_prepared`의 `executable_timestamp`·`autoplay_armed`.

  *Implementation: the shared `target` makes `game_controls` a `std::vector<GameControls>` with a list overload of `ArmedAutoplayFlagRva`, declaring `{0x004896ac, 0x411f6d44}` for 6th; Linux gains `src/platform/linux/game_controls.*`, `OriginalRunEnvironment::autoplay_flag_rva`, arming in the runner, the OSD toggle and the CLI's `game controls` log line; Windows fits the main debuggee's `osd_controls` diagnostic to the list (dropping its `build_timestamp` field), and adds `BootstrapChildHandoffOptions::game_controls`, the child's target id, executable name and autoplay address, and `executable_timestamp` and `autoplay_armed` in `child_runtime_prepared`.*

- **검증**
  - `scripts/test_all.sh linux-x64-debug`(경고를 오류로): build 성공, CTest 4개 통과. 단위 테스트는 6th 목록이 6th timestamp만 무장하고 launcher·Remember 1st timestamp는 0을 주는 것, 목록의 위치와 무관하게 맞는 빌드를 고르는 것을 검사한다.
  - Linux 실행: launcher `game controls : autoplay declared for another build (build 0x411646a8)`, 6th 자식 `autoplay armed (build 0x411f6d44)`. Remember 1st 직접 실행(reserved는 launcher처럼 앞 4바이트 0 뒤 `"256"`): `declared for another build (build 0x411bbf5c)`, 25초 시간 제한까지 `Flip` 반복.
  - Windows: 이 머신에는 MSVC가 없어 build와 실행을 하지 못했다. 변경은 같은 파일의 기존 `WriteRemoteAnsi`·`WriteRemoteU32` 쓰기 형태를 따랐다. Windows build와 6th 자식의 `child_runtime_prepared` `autoplay_armed: true` 확인은 사용자에게 남긴다.

  *Verification: `scripts/test_all.sh linux-x64-debug` (warnings as errors) builds and passes 4 CTest tests; the unit tests check that the 6th list arms only the 6th timestamp and gives 0 for the launcher's and Remember 1st's, and that a list picks the matching build wherever it sits. Linux runs: the launcher logs `game controls : autoplay declared for another build (build 0x411646a8)` and the 6th child `autoplay armed (build 0x411f6d44)`; a direct Remember 1st run (reserved bytes as the launcher gives them, four zero bytes then `"256"`) logs `declared for another build (build 0x411bbf5c)` and repeats `Flip` until its 25-second timeout. Windows: this machine has no MSVC, so the Windows product was neither built nor run; the change follows the file's existing `WriteRemoteAnsi` and `WriteRemoteU32` pattern, and the Windows build and the 6th child's `child_runtime_prepared` `autoplay_armed: true` are left to the user.*

- **사용자 확인**: Linux에서 OSD(백틱)의 Autoplay로 6th의 autoplay가 동작하는 것을 사용자가 확인했다. 이어 Remember 1st로 넘어갈 때 6th가 `DeleteFileA`에서 멈추는 문제를 보고했고, [작업 437](20261003-437-remember-1st-handoff-files.md)에서 다룬다.
  *User check: the user confirmed 6th's autoplay works on Linux through the OSD's (backtick) Autoplay. They then reported 6th stopping at `DeleteFileA` on the way to Remember 1st, handled in [task 437](20261003-437-remember-1st-handoff-files.md).*
