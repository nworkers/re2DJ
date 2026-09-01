# TODO

현재 진행 중인 작업과 아직 결정되지 않은 항목만 기록합니다. 완료 항목은 [구현 완료 항목](IMPLEMENTED.md)으로 이동합니다.

*This file contains only active work and unresolved items. Completed items are moved to [Implemented](IMPLEMENTED.md).*

작업 086의 `DemoVolume` HLE로 확인된 title 음량 저하 원인은 제거됐다. 실제 전체 곡·효과음 청취 정확성은 작업 072의 사용자 재검증 항목으로 유지하며 Linux 작업 077은 사용자 결정에 따라 잠시 보류한다.

*Task 086's `DemoVolume` HLE removes the confirmed title-volume attenuation. Audible accuracy across complete songs and effects remains a user-revalidation item under Task 072; Linux Task 077 remains temporarily paused by user decision.*

## 현재 진행 / In progress

- [ ] 작업 127 — ez2dj4th Hardlock Function `0x0e` 변환 독립 복원
  - [x] Git-ignore `cfg/hardlock.ini` 기본 경로와 profile별 memory-only 적재
  - [x] 실제 `0x468`/active-console `0x450` 및 synthetic 분기의 matching `0x44c/0x458` 재확인
  - [x] 플랫폼 중립 IOCTL sequence/shape/descriptor 검증 oracle 구현
  - [x] 작업 128 — 서명된 vendor driver로 네 IOCTL framing과 device-transport 경계 독립 확인
  - [ ] 원본 실행 또는 허용 라이선스 자료로 bit-level transform 근거 확보
  - [ ] 외부 설정의 memory-only seed를 사용하는 플랫폼 중립 transform 구현
  - [ ] 알려진 입출력 vector와 원본 다음 경계로 응답 검증

  *Task 127 — Git-ignored default configuration, memory-only profile loading, real `0x468`/active-console `0x450` reacquisition, and a platform-neutral sequence/shape/descriptor oracle are complete. Task 128 independently confirms the four IOCTL framing contracts in a signed vendor driver and establishes that Function `0x0e` crosses into device transport rather than a host-side three-seed transform. Independently reconstruct Function `0x0e`, implement it only after a policy-compatible bit-level basis is established, and verify it against a known input/output vector and the original execution's next boundary.*

- [x] 작업 114 — ez2dj4th FAT32 CHD 파일시스템 및 실행 연결
  - [x] 실제 `4thTrax.chd`의 MBR/BPB/FAT/LFN과 `EZ2DJ/EZ2DJ.EXE` 확인
  - [x] `Fat32Volume` read-only file-range API 및 PE32 검증 연결
  - [x] Windows x86 executable staging과 CHD-backed runtime pseudo handle 경계 추가
  - [x] 실제 Windows 장비에서 첫 HLE 장치 open 경계(<code>\\.\NTICE</code>, <code>\\.\FEnteDev</code>) 확인

  *Task 114 — Add a read-only FAT32 view over the real CHD, locate and stage
  `EZ2DJ/EZ2DJ.EXE`, and connect guest reads to CHD-backed runtime handles.
  The first protected 4th runtime device-open boundary is now confirmed as
  <code>\\.\NTICE</code>, followed by <code>\\.\FEnteDev</code>.*

- [x] 작업 118 — ez2dj4th 보호 stub bounded API trace
  - [x] <code>ExitProcess</code> 정적 import가 없는 target의 API trace 준비 경계 추가
  - [x] 4th의 첫 동적 <code>GetProcAddress</code> 대상(<code>GetVersion</code>, <code>CreateFileA</code>) 확인
  - [x] 동적 <code>GetProcAddress</code> 결과를 4th CHD VFS wrapper로 연결

  *Task 118 — Add a bounded API-trace boundary for targets without a static
  <code>ExitProcess</code> import and confirm 4th's first dynamic
  <code>GetProcAddress</code> targets. Connecting those dynamic results to the
  4th CHD VFS wrapper is now confirmed; the protected continuation remains
  unresolved.*

- [x] 작업 119 — ez2dj4th 동적 VFS resolver
  - [x] 4th profile의 <code>hle_dynamic_vfs</code> capability와 runtime export 추가
  - [x] 원본 <code>GetProcAddress</code> IAT 2개를 injected resolver thunk로 연결
  - [x] 실제 CHD VFS log에서 <code>CreateFileA:route=hle</code> 확인
  - [ ] asset-open 이후 보호 응답과 정상 게임 실행 경계 확인

  *Task 119 — Add the 4th-only dynamic VFS resolver capability, patch the
  original <code>GetProcAddress</code> IAT to the injected thunk, and confirm
  <code>CreateFileA:route=hle</code> in the real CHD VFS log. Asset opening,
  protection response, and normal game execution remain unresolved.*

- [x] 작업 120 — ez2dj4th bounded VFS open trace
  - [x] <code>Re2djVfsCreateFileA</code> request/result bounded trace 추가
  - [x] 실제 CHD trace에서 resolver route와 wrapper request event 분리 확인
  - [x] API software watch 없는 trace에서 반환 함수 포인터의 실제 wrapper 호출 확인

  *Task 120 — Add bounded request/result diagnostics for
  <code>Re2djVfsCreateFileA</code> and distinguish resolver routing from an
  actual wrapper request in the real CHD trace. Task 125 confirms wrapper entry
  for <code>\\.\NTICE</code> and <code>\\.\FEnteDev</code> when broad API
  software watches are disabled.*

- [x] 작업 121 — ez2dj4th 동적 resolver 반환 ABI trace
  - [x] HLE/native 반환 주소와 원본 resolver caller 기록
  - [x] 반환 주소가 runtime·kernel32 module 범위에 있는지 확인
  - [ ] 반환 포인터 실제 호출과 <code>eip=0</code> fault 원인 확인

  *Task 121 — Record HLE/native return addresses and original resolver callers,
  and confirm their expected runtime/system-module ranges. The actual returned
  pointer call and the cause of the <code>eip=0</code> fault remain unresolved.*

- [x] 작업 122 — ez2dj4th resolver caller instruction window
  - [x] runtime memory에서 <code>CreateFileA</code> caller window readable 확인
  - [x] 반환 EAX의 <code>[EBP-0x24]</code> 저장 instruction 확인
  - [ ] 저장된 pointer consumer와 후속 indirect call 경계 확인

  *Task 122 — Read the live runtime memory around the
  <code>CreateFileA</code> resolver caller and confirm the returned EAX store at
  <code>[EBP-0x24]</code>. The stored-pointer consumer and later indirect-call
  boundary remain unresolved.*

- [x] 작업 123 — ez2dj4th EIP=0 fault 호출 대상 귀속
  - [x] fault stack return address 직전 x86 indirect call encoding 관찰
  - [x] <code>FF 15 [0x00AF0CF4]</code> pointer slot과 현재 target 값 기록
  - [x] zero target indirect call과 정상 HLE/보호 응답을 구분
  - [ ] slot이 0이 된 보호 코드 원인과 그 이전 continuation 확인

  *Task 123 — Attribute the <code>EIP=0</code> fault to the x86 indirect-call
  encoding immediately before a fault-stack return address, record the
  <code>FF 15 [0x00AF0CF4]</code> pointer slot and current target, and keep a
  zero target separate from HLE or protection success. The code path that left
  the slot zero and the earlier continuation remain unresolved.*

- [x] 작업 124 — ez2dj4th zero pointer-slot 참조 추적
  - [x] <code>0x00AF0CF4</code>가 정식 PE IAT가 아님을 확인
  - [x] HLE 없는 native baseline에서도 동일한 zero slot과 fault 확인
  - [x] live main image의 참조 12개와 명확한 EAX 기록 명령 3개 확인
  - [ ] 어느 기록 명령이 실행되는지와 실행 시 EAX 값 추적

  *Task 124 — Distinguish <code>0x00AF0CF4</code> from the formal PE IAT,
  reproduce the same zero slot and fault without HLE, and locate 12 live-image
  references including three unambiguous EAX stores. Which writer executes and
  the EAX value at that point remain unresolved.*

- [x] 작업 125 — ez2dj4th pointer-slot writer 실행 추적
  - [x] 세 writer RVA에 원본 memory를 수정하지 않는 hardware breakpoint 적용
  - [x] <code>0x00AEFE62</code>에서 EAX <code>0x00B17B00</code> 저장 확인
  - [x] broad API software watch가 writer를 우회하고 zero-slot fault를 재현함을 확인
  - [x] 정상 trace에서 <code>\\.\NTICE</code>와 <code>\\.\FEnteDev</code> VFS wrapper request 확인
  - [ ] 두 장치 경로의 보호 driver protocol과 응답 정책 분석

  *Task 125 — Use hardware execution breakpoints to confirm that writer
  <code>0x00AEFE62</code> stores EAX <code>0x00B17B00</code>. Broad API software
  watches bypass that writer and reproduce the zero-slot fault. Without those
  watches, the protected path reaches VFS requests for <code>\\.\NTICE</code>
  and <code>\\.\FEnteDev</code>; their driver protocols remain unresolved.*

- [x] 작업 113 — ez2dj4th MAME CHD HDD 입력 및 libchdr adapter
  - [x] `ez2dj4th` built-in profile과 shortcut 경로 `roms/ez2dj4th` 선언
  - [x] libchdr vendoring 및 공용 CHD header/metadata/hunk/sector adapter 구현
  - [x] 실제 CHD의 geometry metadata와 LBA 0 sector 판독 검증
  - [x] CHD 내부 FAT/VFS mount, executable fingerprint와 `re2dj --run ez2dj4th` 연결

  *Task 113 — Add the `ez2dj4th` MAME CHD HDD input and libchdr adapter. The
  profile shortcut is `roms/ez2dj4th`; the follow-up FAT32 and runtime boundary
  is recorded in Task 114.*

- [ ] 작업 077 — Linux 원본 실행 경로
  - [x] Linux x86-64 host/i386 helper synthetic PE32 mapping·relocation·TLS·import gate IPC 검증
  - [x] X11·Wayland SDL3/OpenGL 공용 backend build 검증
  - [x] production i386 helper와 Linux `re2dj --run`을 연결해 원본 첫 import/fault/exit 보고
  - [x] guest stack·TEB/PEB·FS와 signal fault 경계 구현
  - [ ] 공용 Win32 import dispatcher, ABI marshalling, guest memory·handle·module service 구현
  - [ ] kernel32·USER32·VFS·INI·GDI HLE로 창과 첫 자산 접근 도달
  - [ ] guest callback, nested import, thread·TLS·동기화 구현
  - [ ] 공용 DirectDraw/Direct3D/DirectSound COM facade를 SDL graphics/audio/input에 연결
  - [x] 보호된 `ez2dj.exe` self-modifying code·LPTDI 환경·의미 기반 I/O board 지원
  - [ ] helper 자동 탐색, overlay CLI, synthetic CI와 Linux 원본 실행 가이드 완성

  *Task 077 — Execute the original x86 PE32 in a Linux i386 helper behind the shared Win32 HLE and x86-64 SDL services. Bring up the unprotected build first, then add the protected cabinet executable's environment boundaries.*

## 사용자 보류 / Paused by user

- [ ] 작업 111 — ez2dj3rd Hardlock `0x9c402458` Function `0x0e` 응답 판별 경계 복원
  - [ ] 264바이트 in-place descriptor 마지막 8바이트의 반환 뒤 소비 경로 추적
  - [ ] Task 107 seed 후보별 synthetic 응답을 기본 비활성 분석 경계에서 생성·검증
  - [ ] 원본 실행의 다음 분기 oracle로 후보 교집합을 축소하되 실제 seed로 성급히 확정하지 않기
  - 재개 기준: [Hardlock 보류 체크포인트](work-logs/20260901-112-3rd-hardlock-pause-checkpoint.md)의 `0x450` replay, `0x44c` tail 분기와 남은 미확정 목록을 먼저 읽습니다.

  *Task 111 — reconstruct the post-return consumer for the final eight bytes of the 264-byte Function-`0x0e` descriptor, generate and test Task 107 candidate responses only behind a default-off analysis boundary, and reduce their intersection using the original execution's next-branch oracle without prematurely identifying physical seeds. Resume from the [Hardlock pause checkpoint](work-logs/20260901-112-3rd-hardlock-pause-checkpoint.md), which records the `0x450` replay, the `0x44c` tail branch, and unresolved items.*

- [ ] 작업 072 — 렌더링 정확성·성능 회복
  - [x] surface별 OpenGL texture cache와 dirty revision으로 매 draw 전체 upload를 제거
  - [x] RGB565 source color-key 범위와 관찰된 texture-stage/render state를 적용
  - [x] draw/I/O 고빈도 진단을 bounded trace로 바꾸고 debugger 분리 실행을 추가
  - [ ] 작업 073 offscreen/blit 크래시 수정본에서 누락 그림, 투명 테두리, 합성 순서와 애니메이션 속도를 사용자 화면으로 재검증
  - [ ] 작업 083 streaming 및 작업 086 `DemoVolume` 수정 뒤 `title.wav`, 효과음, volume/pan/frequency와 duplicate 동시 재생을 청취 확인
  - [ ] `--io-config` 키보드 입력으로 메뉴·게임 상태 전이가 가능한지 실제 사용자 검증
  - [ ] 작업 087 counter 수정 뒤 F3 press/release 한 번마다 credit이 정확히 1 증가하는지 실제 사용자 검증
  - [x] 작업 088 `Texture2::Load` 직접 원인 추정은 사용자 재검증과 `TextureLoad` 호출 0회 trace로 기각
  - [x] 작업 089 변환 전 FVF `0x112`/`0x1e2` 지원 뒤 Music Select 중앙 곡 그림 표시를 사용자 검증
  - [x] 작업 090 current-process hard-termination 보정 뒤 창 닫기 시 `ez2dj.exe`와 대기 중인 `re2dj.exe` 종료를 사용자 검증

  *Task 072 — Rendering and detached-runtime implementation is complete. The user confirmed that Task 089 restores the Music Select center artwork and Task 090 terminates both `ez2dj.exe` and its waiting `re2dj.exe` on window close. Audio and input revalidation remains active.*

- [ ] 작업 074 — 누락 이미지 합성 추적
  - [x] `USER32!LoadImageA` VFS wrapper 구현과 IAT 패치 동작 확인
  - [x] 원본 BMP 로딩 경로(검색 경로 테이블 → `CreateFileA` 존재 확인 → `LoadImageA`) 정적 확인 및 분석 문서화
  - [x] 자산 진단을 `.bmp`/`.str` + 호출 API 태그 + 확장자별 상한으로 확장
  - [x] detached 재실행으로 `System\CompanyLogo\logo.str` 요청 유무와 결과 수집 (`20260827-015256`에서 open 성공)
  - [x] 원인 확정 — `.str` 로더가 `FILE_FLAG_NO_BUFFERING`으로 열어 sector 배수가 아닌 크기를 통째로 읽어 `ReadFile`이 실패
  - [x] VFS `CreateFileA` 경계에서 `FILE_FLAG_NO_BUFFERING` 제거와 probe 회귀 추가
  - [ ] detached 재실행으로 로고·Title 장면 그래픽 표시와 기존 마스킹·컬러키 무회귀 확인

  *Task 074 — The `LoadImageA` boundary is verified, the `.str` read failure is attributed to `FILE_FLAG_NO_BUFFERING`, and the VFS boundary now strips it. A detached re-run must confirm the logo and Title scene graphics appear without regressing the corrected mask and color key.*

- [ ] 작업 075 — 컬러키 discard 의미 복구
  - [x] 원인 확인 — 컬러키를 alpha로만 표현해 `srcblend=ONE`/`dstblend=ZERO` 복사 blend에서 keyed texel이 검정으로 기록
  - [x] 게스트 `COLORKEYENABLE`로만 gate되는 shader discard 구현과 `Blt` 경로 alpha test 흉내 제거
  - [x] `LateDraw` 진단에 게스트 `colorkey=`, `alphatest=` 추가
  - [ ] detached 재실행으로 로고 투명도와 배경·mask 계층 무회귀 확인

  *Task 075 — Color keying is now a shader discard gated on the guest `COLORKEYENABLE`. A detached re-run must confirm logo transparency without regressing backgrounds or mask layers.*

- [ ] Windows x86 VFS guest write/overlay 검증
  - [ ] canonical 실행에서 guest write가 원본 HDD가 아닌 overlay에 기록되는지 확인

  *Verify that canonical guest writes go to the overlay rather than modifying the original HDD directory. The read/seek/size/close path already runs through the original main-loop startup.*

## 다음 작업 / Next work

- [ ] 작업 119 — Windows x86 4th dynamic <code>GetProcAddress</code> VFS HLE
- [ ] Windows x86 INI API HLE (`GetPrivateProfile*`, `WritePrivateProfileStringA`)
- [ ] Windows x86 directory enumeration (`FindFirstFileA`, `FindNextFileA`, `FindClose`)
- [ ] Web(Emscripten) build verification
- [ ] GitHub Actions first workflow verification

## 분석 미완료 / Analysis remaining

- [ ] `Songs/` 아래 자산 파일 형식 분석
- [ ] 3rd target guest drive and working-directory evidence
- [ ] 실패 경로 continuation buffer(힙 페이지)의 의미 추적 — entry 직후 XOR 루프 대상 버퍼와 `[0x01ed7074]` 플래그 기입 지점
- [ ] 3rd 보호 빌드(`EZ2DJ.EXE`)의 import 표면과 런타임 흐름

## 보류 / Deferred

- [ ] Windows x64 host expansion
- [ ] Custom x86-32 interpreter, only if no permitted Web execution engine is suitable

- [ ] Task 096 visual revalidation: confirm scene-transition flicker/fade-out and z-order behavior with the user's current display setup
- [ ] 작업 097 Music Select 좌표·창 pixel viewport 재검증 (창 크기/DPI 변경 포함)

*Task 097's trace shows internally centered logical coordinates for the Music Select composition, while the SDL/OpenGL backend now refreshes its pixel viewport after native child-window or DPI size changes. The exact user-visible artwork position still needs a reproducible Music Select capture.*

- [ ] Task 097 coordinate revalidation: confirm Music Select artwork placement after window resize or DPI changes
