# TODO

현재 진행 중인 작업과 아직 결정되지 않은 항목만 기록합니다. 완료 항목은 [구현 완료 항목](IMPLEMENTED.md)으로 이동합니다.

*This file contains only active work and unresolved items. Completed items are moved to [Implemented](IMPLEMENTED.md).*

작업 086의 `DemoVolume` HLE로 확인된 title 음량 저하 원인은 제거됐다. 실제 전체 곡·효과음 청취 정확성은 작업 072의 사용자 재검증 항목으로 유지한다. Linux 작업 077은 WSL x86/x64 제품 host와 공통 i386 helper를 기준으로 계속 진행한다.

*Task 086's `DemoVolume` HLE removes the confirmed title-volume attenuation. Audible accuracy across complete songs and effects remains a user-revalidation item under Task 072; Linux Task 077 continues against the WSL x86/x64 product hosts and shared i386 helper.*

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
  - [x] 공용 Win32 import dispatcher, ABI marshalling, 64KiB guest-memory transport와 pending import `kStop` 제어 구현
  - [x] Linux i386 pseudo module/resolver와 첫 동적 `CreateFileA("\\.\NTICE")` 호출 ABI 검증
    *Validated the Linux i386 pseudo module/resolver and first dynamic `CreateFileA("\\.\NTICE")` call ABI.*
  - [ ] guest handle·module service 구현
  - [ ] kernel32·USER32·VFS·INI·GDI HLE로 창과 첫 자산 접근 도달
  - [ ] guest callback, nested import, thread·TLS·동기화 구현
  - [ ] 공용 DirectDraw/Direct3D/DirectSound COM facade를 SDL graphics/audio/input에 연결
  - [x] 보호된 `ez2dj.exe` self-modifying code·LPTDI 환경·의미 기반 I/O board 지원
  - [ ] helper 자동 탐색, overlay CLI, synthetic CI와 Linux 원본 실행 가이드 완성

- [ ] 작업 311 — Linux guest memory transport 확장
  - [x] import gate pending stack window를 유지하면서 매핑된 PE image read/write 허용
  - [x] host/helper memory transfer 상한을 64KiB로 고정
  - [x] 공용 import dispatcher와 x86 ABI marshalling 연결(작업 312), pending import `kStop` terminal 제어(작업 313)
  - [ ] 원본 import 표면 근거의 실제 Win32 API binding 등록

  *Task 311 — Expand Linux guest memory transport. The pending import stack window and mapped PE image reads/writes are available, the host/helper transfer limit is fixed at 64 KiB, Task 312 connects the shared import dispatcher and x86 ABI marshalling, and Task 313 makes pending-import `kStop` a terminal host action. Actual Win32 API bindings remain next.*

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

- [ ] in-process 러너 성능 분석(2026-10-05 등록, [측정 결과](analysis/windows-in-process-performance.md))
  - [ ] 조건별 반복 측정으로 처리량 차이(4th −17%, 1st SE −22%, 5th −11%, 6th −16%, 2nd MOVE +10%) 확정
  - [ ] vsync off 실행에서 import별 호출 횟수와 샘플링 프로파일로 비용이 큰 경계 찾기
  - [ ] 전역 게스트 잠금의 넘겨주기 비용과 스레드 직렬화 영향 측정
  - [ ] 자주 불리는 단순 import(시간·동기화)의 빠른 경로 검토
  - [ ] Private bytes 증가(최대 1.4 GB)와 6th working set 증가의 원인 확인
  - [ ] 측정용 vsync off 선택(현재 `--vsync`는 거부됨)을 정식 옵션으로 둘지 결정

  *In-process runner performance analysis, registered 2026-10-05 ([measurements](analysis/windows-in-process-performance.md)): settle the throughput differences with repeated runs, find the costly boundaries from per-import call counts and a sampling profile of vsync-off runs, measure the global guest lock's hand-over cost and thread serialization, consider fast paths for simple, frequent imports (time, synchronization), explain the private-bytes growth (up to 1.4 GB) and 6th's larger working set, and decide whether a vsync-off choice for measuring (`--vsync` is refused now) becomes a real option.*

- [ ] EZ2DJ 6th 실행(2026-09-30 시작)
  - [x] Win32: 데모, 코인, 모드 선택, 곡 선택, 플레이까지 진행(작업 430 전에도 진행됨)
  - [x] [작업 430 — DX7 조명과 깊이 기본값](work-logs/20260930-430-d3d7-lighting-defaults.md). 모드 선택 화면의 모드별 3D 그림이 그려지지 않던 문제. 새 장치의 render state 기본값(`ZWRITEENABLE` 1, `ZFUNC` LESSEQUAL, `ALPHAFUNC` ALWAYS, DX7 `LIGHTING` 1)과 광원 없는 조명 색(emissive + AMBIENT × material ambient, alpha는 diffuse alpha)을 측정해 공용 core로. Windows DX7 `SetMaterial`·`GetMaterial`, Linux `GetMaterial`
  - [ ] 데모 플레이의 BGA 자리에 보이는 색 노이즈가 원본 연출인지 확인
  - [x] [작업 431 — Linux 6th와 자식 프로세스](work-logs/20260930-431-linux-6th-child-process.md). launcher의 `CreateProcessA`를 별도 host 프로세스로 실행(측정: 명령줄·현재 디렉터리·`lpReserved2` 전달, 32비트 종료 코드), `GetExitCodeProcess`·`SetPriorityClass`·자식 핸들 대기, `GetStartupInfoA` reserved, `GetKeyState`, `GetFullPathNameA`, 32비트 `StretchDIBits`(모두 측정). Linux 두 폭에서 타이틀·코인·모드 선택까지 확인, 시간 제한까지 멈추지 않음
  - [ ] Remember 1st 모드: 6th이 0x100으로 끝나면 launcher가 `EZ2DJ1ST\EZ2DJ.EXE`를 실행함. 두 host 모두 아직 확인하지 않음. Linux 자식 run은 6th 프로필의 guest root·Hardlock 설정을 쓰므로 1st 자식에 맞는지 확인 필요
  - [ ] Linux 6th 성능: x64 Debug에서 타이틀 약 40 FPS, x86 Debug에서 모드 선택 전환 중 4.5 FPS
  - [ ] 광원(`SetLight`·`LightEnable`): 6th는 쓰지 않음. 쓰는 게임이 나오면 측정해 모델

  *EZ2DJ 6th, started 2026-09-30.*
  - *Win32 gets through the demo, coins, mode select, music select, and play (it already did before task 430).*
  - *Task 430 fixes the per-mode 3D pictures in mode select, which were not drawn. The measured new-device render-state defaults (`ZWRITEENABLE` 1, `ZFUNC` LESSEQUAL, `ALPHAFUNC` ALWAYS, and DX7 `LIGHTING` 1) and the colour lighting gives with no light (emissive + AMBIENT × material ambient, alpha from the diffuse alpha) move into the shared core, with Windows DX7 `SetMaterial` and `GetMaterial` and Linux `GetMaterial`.*
  - *Still to do: check whether the colour noise in the demo's BGA area is the original's own effect; Linux 6th (the child-process `EZ2DJ6th.EXE` structure); and lights (`SetLight`, `LightEnable`), which 6th does not use, to be measured and modelled when a game does.*

- [x] [작업 345 — 플랫폼 비트 폭 재배치](work-orders/20260922-345-linux-platform-width-split.md)
  - [x] Linux — i386 전용 구현을 `linux/x86/`로 이동. 루트에는 두 폭 공용 코드만 남음
  - [x] Windows — 현 상태 유지(2026-09-23 사용자 결정). Windows x64 host가 아직 없어 폭별 분리의 기준이 될 두 번째 폭이 없음. x64 host를 도입할 때 파일 단위로 다시 판단

  *Task 345 — Apply the bit-width directory policy. Linux is done: i386-only implementations moved under `linux/x86/`, leaving only dual-width code at the root. Windows stays as it is by the user's decision on 2026-09-23: there is no Windows x64 host yet, so there is no second width to split against. Revisit per file when an x64 host is introduced.*

- [ ] [작업 340 — 게스트 PE 호환 모듈 계획](work-orders/20260921-340-guest-pe-compatibility-modules.md)
  - [x] [작업 341 — 플랫폼 중립 module descriptor와 `GuestModuleRegistry`](work-logs/20260921-341-guest-module-registry.md)
  - [x] [작업 342 — descriptor 기반 PE32 export facade builder](work-logs/20260922-342-guest-pe-facade-builder.md)
  - [x] [작업 343 — 확인된 네 export의 `kernel32` module과 Linux i386 mapping](work-logs/20260922-343-kernel32-linux-i386-facade.md)
  - [x] [작업 344 — pseudo handle 제거, static/dynamic resolver identity와 실제 4th CHD 회귀](work-logs/20260922-344-guest-module-resolver-convergence.md)
  - [x] [작업 348 — `original_runner.cpp`의 이전 두 진단을 같은 registry 경로로 합류](work-logs/20260923-348-early-resolver-diagnostics-convergence.md). 실제 4th CHD에서 `GetVersion` 실제 호출(caller 복귀 `0x00aefd82`) 확인
  - [x] [작업 349 — Linux i386 in-process 연속 실행 진단](work-logs/20260923-349-linux-inprocess-continuation.md). 실제 4th CHD에서 API 13개 뒤 게스트 자신의 `INT3`(`0x00af1135`, ESI=`'FG'`, EDI=`'JM'`)에서 멈춤. `GetVersion` 값은 이 경계까지 분기를 바꾸지 않음
  - [x] [작업 350 — 게스트 SEH 체인 관찰 및 INT3 핸들러 확인](work-logs/20260923-350-guest-seh-chain-observation.md). 실제 4th CHD 연속 실행에서 게스트 TEB `FS:[0]`에 SEH frame(`0xf7756e0c`), handler(`0x00af159b`) 등록 확인. 핸들러 진입 시 `PCONTEXT ContextRecord` 로드(`mov eax, [esp+0x0c]`) 확인
  - [x] [작업 351 — 게스트 SEH 디스패치 및 INT3 예외 재개](work-logs/20260923-351-linux-guest-seh-dispatch.md). 실제 4th CHD 연속 실행에서 게스트 SEH 핸들러(`0x00af159b`)로 `EXCEPTION_BREAKPOINT` 전달, `ExceptionContinueExecution`(`0`) 반환 및 `0x00af11af` 재개 확인. `#0014 CreateFileA("\\.\FEnteDev")` 통과 후 `#0015 GetProcAddress(0, "GetActiveWindow")` 도달
  - [x] [작업 352 — `user32` facade module](work-logs/20260923-352-user32-facade-module.md). `GetActiveWindow` 하나를 export. 실제 4th CHD에서 `#0002 GetModuleHandleA("user32")`가 `0x6eff0000`을, `#0015`가 thunk `0x6eff2000`을 받음. 게스트는 이 thunk를 호출하지 않고 `#0016 GetProcAddress(kernel32, "ExitProcess")`에서 멈춤
  - [x] [작업 358 — `kernel32` facade의 `ExitProcess`](work-logs/20260924-358-kernel32-exit-process.md). `ImportReturn.exit_process`로 게스트 종료를 표현하고 두 폭 bridge가 정상 완료로 보고. 실제 4th CHD는 두 폭 모두 `#0016`이 facade 주소를 받은 뒤 `GetModuleHandleA("DDRAW.DLL")`(0)을 거쳐 `#0019` 정적 `user32!MessageBoxA`에서 정지
  - [x] [작업 359 — `user32!MessageBoxA` facade](work-logs/20260924-359-user32-message-box.md). 실제 4th CHD가 두 폭 모두 `"Error 1009 : Cannot open Hardlock driver."`(제목 `Hardlock`)를 표시한 뒤 `ExitProcess(9)`로 정상 종료. 작업 358의 종료 경로가 원본에서 처음 쓰임
  - [ ] Linux Hardlock 장치 HLE
    - [x] [작업 360 — Hardlock HLE 연결부 공용화와 WTS 이름 정정](work-logs/20260924-360-hardlock-hle-shared-boundary.md). 설정 조립·`DeviceIoControl` 완료 규칙·장치 경로 판정을 공용 코어로 옮김. Windows 실제 4th의 Hardlock 요청 기록이 변경 전후 동일. WTS class 4는 `WTSSessionId`로 정정
    - [x] [작업 361 — 게스트 장치 handle과 `kernel32` 장치 호출](work-logs/20260924-361-linux-guest-device-handles.md). 실제 4th가 Linux 두 폭에서 `\\.\FEnteDev`를 열고 initialize `0x9c402468`을 처리한 뒤 `GetProcAddress(user32, "CreateCursor")`에서 정지
    - [x] [작업 363 — Hardlock API 시작 환경](work-logs/20260924-363-hardlock-api-startup-environment.md). `GuestProcess`(ID·error mode·heap), 없는 이름 선언, `kernel32` 7개·`user32` cursor 3개, `advapi32`·`wtsapi32` facade. 실제 4th가 두 폭에서 WTS 세션 0을 받고 handshake 2회·descriptor 1회 뒤 `GetProcAddress(kernel32, "OpenProcess")`에서 정지
    - [x] [작업 364 — 게스트 자기 process 메모리](work-logs/20260924-364-guest-process-memory.md). 공용 handle 공간, image·`VirtualAlloc` region의 page 보호 기록, `OpenProcess`·`Virtual*`·`LocalAlloc/Free`. 실제 4th가 두 폭에서 복호화 루프를 끝까지 돌고(descriptor 37·transform 36, Windows와 같음) `GetProcAddress(kernel32, "GetCurrentThreadId")`에서 정지
    - [x] [작업 367 — envelope의 두 번째 층과 원본 진입](work-logs/20260925-367-envelope-handoff-to-original.md). 원본 import 표 재구성(10개 DLL, 해석 전용 export), facade image region과 RWX code, `ExitProcess` hook, `Read/WriteProcessMemory`, thread timer. 실제 4th가 두 폭에서 원본 CRT의 `GetVersion`(`0x004c4424`)을 지나 `HeapCreate`에서 정지
    - [x] [작업 368 — 원본 MSVC CRT 시작](work-logs/20260925-368-original-crt-startup.md). heap, 시작 정보, 명령줄·환경, code page 949(실측), module 경로, stop stub의 SEH 제외. 실제 4th가 두 폭에서 CRT 시작을 마치고 게임 코드의 `CreateEventA`에서 정지
    - [x] [작업 369 — 정적 초기화에서 WinMain까지](work-logs/20260925-369-static-initializers-to-winmain.md). 이름 없는 event, host 시계 서비스와 시간 export, `GetTimeZoneInformation`. 실제 4th가 두 폭에서 게임의 Hardlock 로그인을 지나 WinMain의 `timeBeginPeriod`에서 정지
    - [x] [작업 370 — 게스트 API 호출 기록](work-logs/20260925-370-guest-api-call-log.md). facade 호출의 입력·출력·last error·반환값을 `*.api.log`에 기록(Hardlock buffer는 길이만)
    - [ ] API log 상한(게임 loop가 돌 때 실제 크기를 보고 정함)
    - [x] [작업 371 — winmm 시간과 게스트 파일](work-logs/20260926-371-winmm-timing-and-guest-files.md). `timeBeginPeriod`/`timeEndPeriod`/`timeGetTime`, CHD+overlay 게스트 파일(copy-on-write)과 kernel32 파일 함수. 실제 4th가 `EZ2DJ.ini`를 읽고 `LoadIconA`에서 정지
    - [x] [작업 372 — 게스트 호출과 window 생성](work-logs/20260926-372-guest-callbacks-and-window.md). handler가 게스트 함수를 부르는 `CallGuest`(x86 직접, x64 중첩 전환), `GuestUser`, `RegisterClassA`·`CreateWindowExA`·`DefWindowProcA`·`UpdateWindow`, `gdi32!GetStockObject`. 실제 4th가 두 폭에서 window를 만들고 `DirectDrawEnumerateExA`에서 정지
    - [x] [작업 373 — DirectDraw 진입](work-logs/20260926-373-directdraw-entry.md). facade COM 객체(`GuestComObjects`, vtable은 `"<인터페이스>::<메서드>"` export), `DirectDrawEnumerateExA`(모니터 하나, 실측), `DirectDrawCreateEx`, `IDirectDraw7`. 실제 4th가 두 폭에서 `QueryInterface(IID_IDirect3D7)`에서 정지
    - [x] [작업 374 — 공용 DirectX core와 첫 단계](work-logs/20260926-374-shared-directx-core.md). `re2dj_directx`(guest ABI, caps·열거·표시 모드·식별자)를 Windows facade와 Linux가 함께 씀(Windows 실제 4th 기록 변경 전후 같음). Linux `IDirect3D7`, `lstrcpynA`, `ShowCursor`. 실제 4th가 두 폭에서 DirectDraw 열거를 마치고 `SetCooperativeLevel`에서 정지
    - [x] [작업 375 — 창 제목의 빌드 표시](work-logs/20260926-375-window-title-build-label.md). `re2DJ v<버전> (<OS>/<arch> <구성>)`, `BuildLabel()`
    - [x] [작업 376 — 모든 버전 표시를 한 머리말로](work-logs/20260926-376-consistent-version-banner.md). `VersionBanner()`를 창 제목·OSD·CLI(`--help`, `--version`, 시작 log)·진단 도구 4개가 씀
    - [x] [작업 377 — DirectX core 2단계: 협조 수준과 표시 모드](work-logs/20260926-377-directx-cooperative-level-and-mode.md). `DirectDrawDisplay`와 규칙을 Windows·Linux가 함께 씀(Windows 실제 4th 기록 전후 같음), Linux `SetRect`. 실제 4th가 두 폭에서 `SetDisplayMode`를 지나 `CreateSurface`에서 정지
    - [x] [작업 378 — Linux가 Windows 소스를 참조하지 않게](work-logs/20260926-378-linux-no-windows-sources.md). Linux probe 3개가 포함하던 Windows 파일의 fixture를 `src/platform/native_probe_fixture`로 옮김
    - [x] [작업 379 — native helper IPC 제거](work-logs/20260926-379-remove-native-helper-ipc.md). Linux i386 helper·`--linux-helper`와 Windows native helper(선택 빌드)를 protocol·preset·script와 함께 제거
    - [x] [작업 380 — Linux 호스트 창](work-logs/20260926-380-linux-host-window.md). `HostPresentation`과 Linux SDL3/OpenGL 창(`SetCooperativeLevel` 때, 검은 화면), 공용 `WindowTitle`, `--hold-window`
    - [ ] Linux 창 닫기로 `--hold-window`가 풀리는지 사용자 확인(WSLg는 외부 `WM_CLOSE`를 무시해 자동 확인 불가)
    - [x] [작업 381 — DirectX core 3단계: 표면](work-logs/20260926-381-directx-surfaces.md). `PlanCreateSurface`·attach·표면 설명을 Windows·Linux가 함께 씀(Windows 실제 4th 기록 전후 같음), Linux `IDirectDrawSurface7`(픽셀은 guest 메모리). 실제 4th가 두 폭에서 `CreateSurface`를 지나 `IDirect3D7::CreateDevice`에서 정지
    - [x] [작업 382 — DirectX core 4단계: 장치](work-logs/20260926-382-directx-device.md). `DeviceState`(초기 상태, 상태·장면·viewport 규칙)와 `CheckCreateDevice`를 Windows·Linux가 함께 씀(Windows 실제 4th 기록 전후 같음), Linux `IDirect3DDevice7`. 실제 4th가 두 폭에서 장치 설정과 글꼴 파일 읽기를 지나 `user32!GetForegroundWindow`에서 정지
    - [x] [작업 383 — DirectSound 진입과 창 조회](work-logs/20260926-383-directsound-entry.md). DirectSound core(버퍼 생성·caps·복제·lock)를 Windows·Linux가 함께 씀(Windows 실제 4th 그래픽·오디오 기록 전후 같음), Linux `dsound.dll`(sample은 guest 메모리, 출력 없음), `GetForegroundWindow`, `GetWindowLongA`(측정). 실제 4th가 두 폭에서 `dinput.dll!DirectInputCreateA`에서 정지
    - [x] [작업 384 — DirectInput 진입](work-logs/20260926-384-directinput-entry.md). DirectInput core(인터페이스·장치 판정, 장치 상태 배치)를 Windows·Linux가 함께 씀(Windows 실제 4th 그래픽·입력 기록 전후 같음, 7A IID 바로잡음), Linux `dinput.dll`(host 입력 미연결). 실제 4th가 두 폭에서 입력 장치 설정을 지나 IO 보드 `in al, dx`(port `0x0103`)에서 SIGSEGV
    - [x] [작업 385 — Linux IO 보드 포트 입출력](work-logs/20260926-385-linux-legacy-io-ports.md). 포트 접근 판정 규칙을 core로 옮겨 Windows·Linux가 함께 씀(Windows 실제 4th 그래픽·IO 포트 기록 전후 같음), Linux 두 폭 signal handler가 `LegacyIoPortBus`로 답함. 실제 4th가 포트 읽기 3번을 지나 `winmm!mixerGetNumDevs`에서 정지
    - [x] [작업 386 — winmm mixer](work-logs/20260926-386-winmm-mixer.md). Windows 11 host mixer를 측정해 Linux에 mixer 하나를 모델링(line·control·값·오류, 이름은 "re2DJ Audio"). 실제 4th가 mixer를 연 뒤 secondary 버퍼를 만들고 `IDirectSoundBuffer::Stop`에서 정지
    - [x] [작업 387 — DirectSound 2단계: 버퍼 제어](work-logs/20260926-387-directsound-controls.md). 제어 규칙과 무음 재생(시계로 진행하는 cursor)을 core로, Windows `LegacyAudioBuffer`·facade가 함께 씀(Windows 실제 4th 그래픽·오디오 기록 전후 같음), Linux 버퍼 메서드 21개 모두 구현, 호출 한도 32,768. 실제 4th가 효과음 로딩(약 12,000번 호출)을 마치고 `user32!GetAsyncKeyState`에서 정지
    - [x] [작업 388 — 키 상태 조회와 mixer 식별](work-logs/20260926-388-input-queries.md). `GetAsyncKeyState`(측정, host 입력 미연결로 모두 안 눌림), mixer ID 자리의 열린 handle 수용(측정). 실제 4th가 mixer 조회·설정을 지나 텍스처 표면 `GetDC`에서 정지
    - [x] [작업 389 — 표면 DC와 StretchDIBits](work-logs/20260926-389-surface-dc-and-stretchdibits.md). DC 규칙을 core로(Windows 실제 4th 기록 전후 같음), Linux GDI 모델(`GuestGdi`, 측정한 `gdi_raster`), 표면 `GetDC`/`ReleaseDC`/`SetColorKey`, `StretchDIBits`(측정). 실제 4th가 텍스처 업로드를 마치고 메인 루프의 `user32!GetCursorPos`에서 정지
    - [x] [작업 390 — 메인 루프의 입력과 메시지](work-logs/20260926-390-main-loop-input-and-messages.md). `GetCursorPos`·`ScreenToClient`·`PeekMessageA`·`TranslateMessage`·`DispatchMessageA`(측정), 커서 위치와 timer 기준 시각, 메시지 큐(WM_PAINT, WM_TIMER). 실제 4th가 첫 프레임 그리기에 들어가 `IDirect3DDevice7::Clear`에서 정지
    - [x] [작업 391 — DirectX 5단계: 그리기와 Linux 창 표시](work-logs/20260926-391-directx-drawing.md). 그리기 규칙(draw 계획, 고정 기능 상태, DX7 변환, fade, clear 색)을 `direct3d_draw.h` core로(Windows 실제 4th 그래픽 기록 전후 같음), `HostPresentation` 그리기 계약과 Linux SDL3/OpenGL 구현, Linux `Clear`·`SetTexture`·`DrawPrimitive`·`Flip`, 텍스처 revision·폐기. 실제 4th가 두 폭의 Linux 창에 WARNING 화면을 그리고 약 490프레임 뒤 `IDirectDraw7::EnumSurfaces`에서 정지
    - [x] [작업 392 — 표면 점검: EnumSurfaces와 RestoreAllSurfaces](work-logs/20260927-392-surface-sweep.md). Windows 11 `EnumSurfaces`·`RestoreAllSurfaces` 측정, 플래그 규칙을 core로(Windows 실제 4th 그래픽 기록 전후 같음), Linux 열거(최신 것부터, AddRef, `GetSurfaceDesc`와 같은 설명). 실제 4th가 표면 4개를 점검하고 `kernel32!GetCurrentDirectoryA`에서 정지
    - [x] [작업 393 — 현재 디렉터리](work-logs/20260927-393-current-directory.md). `GetCurrentDirectoryA`·`SetCurrentDirectoryA`(측정), `GuestFiles` 현재 디렉터리와 상대 경로 해석. 실제 4th가 `System\Common`·`System\AmuseLogo`로 옮겨 다음 장면 텍스처를 읽고, CRT의 `GetFileType` 뒤 SIGSEGV로 정지
    - [x] [작업 394 — 파일 handle의 GetFileType](work-logs/20260927-394-file-type.md). 열린 guest 파일은 `FILE_TYPE_DISK`(측정). 실제 4th가 CRT 파일 열기를 지나 Amuse World 로고 장면을 그리고 진단용 호출 한도 32,768번에서 정지
    - [x] [작업 395 — 창을 닫을 때까지 실행](work-logs/20260927-395-run-until-closed.md). 호출 한도는 `--call-limit`로만, backend 이벤트 관찰자와 창 닫기 경계, API 기록 32,768번 제한. 실제 4th가 타이틀 화면까지 돌고 창을 닫으면 exit 0
    - [x] [작업 396 — Linux 창 크기와 단축키](work-logs/20260927-396-linux-window-policy.md). 공용 창 정책(`window_policy.h`), 기본 2배(1280×960), Alt+1/2/3, 더블클릭 전체 화면, 제목 FPS, Linux `--fullscreen`/`--windowed`
    - [ ] Windows facade `EnumSurfaces`가 측정대로 존재하는 표면을 열거(지금은 facade에 표면 목록이 없어 빈 열거)
    - [x] [작업 397 — Linux host 입력](work-logs/20260927-397-linux-host-input.md). VK·키 이름·EZ2DJ 키 배치·턴테이블을 core로(Windows 그래픽·IO 기록 전후 같음), host 입력 상태, Linux SDL 키·마우스·커서가 `GetAsyncKeyState`·DirectInput·`GetCursorPos`·IO 보드로. 사용자 조작으로 코인 투입과 스타일 선택 화면 진입, `kernel32!FindFirstFileA`에서 정지
    - [ ] Linux 소리의 노이즈(사용자 청취 확인 2026-09-27: 소리는 들리나 노이즈가 섞임). 원인 후보: 스트리밍 링 재채움 시점, 샘플 복사 경계, WSLg PulseAudio 버퍼 크기
    - [x] [작업 400 — DX7 vertex buffer](work-logs/20260927-400-vertex-buffers.md). VB 규칙을 core로(Windows 그래픽·VB 60초 기록 전후 같음), Linux `IDirect3DVertexBuffer7`·`CreateVertexBuffer`·`DrawPrimitiveVB`·`DrawIndexedPrimitiveVB`. 곡 정보 화면의 `gdi32!CreateSolidBrush`에서 정지
    - [x] [작업 401 — 표면 DC의 GDI 그리기](work-logs/20260927-401-surface-gdi-drawing.md). `CreateSolidBrush`·`DeleteObject`·`FillRect`·`SetTextColor`·`SetBkMode`·`DrawTextA`(측정). 곡 재킷이 없을 때의 대체 텍스처를 지나 `kernel32!GetFileAttributesA`에서 정지
    - [x] [작업 402 — DrawTextA 글자를 Unifont로](work-logs/20260927-402-drawtext-unifont-glyphs.md). GNU Unifont 15.1.05 ASCII 8×16 글리프(OFL 1.1)로 측정한 셀 위치·잘라내기·OPAQUE 배경을 그림. 대체 텍스처의 `temp` 글자가 나옴
    - [x] [작업 403 — GetFileAttributesA](work-logs/20260927-403-get-file-attributes.md). 측정, 공용 규칙(`DescribeGuestFileAttributes`), Linux `GuestFiles::Attributes`와 kernel32 export, Windows VFS hook(전에는 host 현재 디렉터리 기준으로 조회됨). Windows 60초 기록은 resolver 경로 말고 같음. Linux 두 폭이 입력 없이 attract의 DEMO PLAY까지 돌고 창을 닫을 때까지 멈추지 않음
    - [x] [작업 404 — 게스트 예외 디스패치와 Linux x86 호스트 GS 복원](work-logs/20260927-404-guest-exception-dispatch.md). guest 예외를 guest SEH 체인으로 넘기는 일반 전달과 kernel32 `RtlUnwind` 구현, Linux x86에서 게스트의 `%gs` 수정으로 인한 시그널/브릿지 coredump 원인 규명 및 호스트 GS 트램펄린 복원. 두 폭 모두 `ez2dj1st`가 첫 예외를 통과하고 정상 동작 확인
    - [x] [작업 405 — GetPrivateProfileIntA와 디렉터리 덤프 파일](work-logs/20260927-405-private-profile-int.md). 측정, 공용 INI core와 `DemoVolume` 정책(Windows 제품도 사용), Linux kernel32 export, `GuestFiles` 디렉터리 원본(대소문자 무시), 1st import 이름 33개 resolve-only. 실제 `bookkeeping.ini` 값이 Windows 기록과 같음. 1st는 두 폭 모두 CRT 시작의 `kernel32!InitializeCriticalSection`에서 정지
    - [x] [작업 406 — 1st CRT 시작의 kernel32 함수](work-logs/20260927-406-crt-startup-kernel32.md). critical section, TLS(TEB 슬롯), Interlocked, `GetCurrentThread`, `IsBadReadPtr`/`IsBadWritePtr`(측정). 1st는 두 폭 모두 CRT 시작을 지나 `user32!ShowWindow`에서 정지
    - [x] [작업 407 — ShowWindow](work-logs/20260927-407-show-window.md). 숨긴 창의 `SW_SHOW`는 `WS_VISIBLE` 생성과 같은 메시지 순서(측정), 공용 `ShowHiddenWindow`. 1st는 메시지 루프를 돌고 `user32!EnumDisplaySettingsA`에서 정지
    - [x] [작업 408 — EnumDisplaySettingsA와 ChangeDisplaySettingsExA](work-logs/20260927-408-display-settings.md). 현재 모드는 host 데스크톱 모드(`HostPresentation::DesktopDisplayMode`, SDL), DEVMODEA는 측정한 바이트만. 모드 변경은 Windows 제품처럼 흡수. 1st는 `kernel32!Sleep`에서 정지
    - [x] [작업 409 — Sleep](work-logs/20260927-409-sleep.md). host 대기 서비스(`WaitMilliseconds`), `Sleep(INFINITE)`은 정지. 1st는 `GetPrivateProfileIntA`를 실제로 불러 Windows와 같은 값을 읽고 `kernel32!GetPrivateProfileStringA`에서 정지
    - [x] [작업 410 — GetPrivateProfileStringA](work-logs/20260927-410-private-profile-string.md). 따옴표·버퍼 자름·`ERROR_MORE_DATA`·기본값·키/섹션 목록(측정), 공용 INI core. 1st는 `ez2dj.ini`를 읽고 `kernel32!GetPrivateProfileSectionNamesA`에서 정지
    - [x] [작업 411 — GetPrivateProfileSectionNamesA](work-logs/20260927-411-private-profile-section-names.md). 측정, 공용 INI core 사용. 1st는 `Songs\music.ini` 섹션 목록(167)과 `[STATISTICS]`를 Windows와 같게 읽고 `ddraw!DirectDrawEnumerateA`에서 정지
    - [x] [작업 412 — DirectDrawEnumerateA](work-logs/20260927-412-directdraw-enumerate.md). 주 드라이버 하나(측정), Ex 판과 콜백 루프 공유. 1st는 `ddraw!DirectDrawCreate`에서 정지
    - [x] [작업 413 — DirectDrawCreate와 DirectX 6 객체](work-logs/20260928-413-directdraw-create-dx6.md). `IDirectDraw4`·`IDirect3D3` 객체(DX7과 다른 종류), facade와 같은 `QueryInterface`. 1st는 `IDirectDraw4::SetCooperativeLevel`에서 정지
    - [x] [작업 414 — IDirectDraw4 협력 수준·화면 모드](work-logs/20260928-414-dx6-cooperative-level.md). DX7과 같은 공용 core 본문 공유. 1st는 host 창이 열리고 `IDirect3D3::FindDevice`에서 정지
    - [x] [작업 415 — IDirect3D3::FindDevice 공용 core](work-logs/20260928-415-dx6-find-device.md). DX6 구조체(SDK 배치 검사)와 `directx::FindDevice`를 Windows facade·Linux가 공유. 1st는 `IDirect3D3::EnumZBufferFormats`에서 정지
    - [x] [작업 416 — DX6 장치·표면·viewport 묶음](work-logs/20260928-416-dx6-device-and-viewport.md). `EnumZBufferFormats`, `IDirectDrawSurface4`(DX7 표면 공유), `IDirect3DDevice3`(DX7 장치 상태 공유), `IDirect3DViewport3`와 장치의 viewport, `GetCaps`. Z 형식·caps·`D3DVIEWPORT2` 변환을 공용 core로(Windows facade도 사용). 1st는 두 폭 모두 DX6 초기화와 DirectSound를 지나 `kernel32!CreateThread`에서 정지
    - [x] [작업 417 — Linux 게스트 스레드](work-logs/20260928-417-guest-threads.md). 게스트 스레드마다 host 스레드, 게스트 잠금 하나로 실행(import 안에서만 인계), 스레드별 스택·TEB·FS·실행 상태, x64 transition 상태 인계, 다른 스레드의 fault·정지·종료는 프로세스 종료. `CreateThread`·`SetThreadPriority`·`GetThreadPriority`(측정), 스레드 핸들 대기, 스레드별 ID·last error, critical section 경합 대기. 1st는 두 폭 모두 소리 스레드와 번갈아 돌고 메인 스레드의 `kernel32!HeapValidate`에서 정지
    - [x] [작업 418 — HeapValidate](work-logs/20260928-418-heap-validate.md). 측정대로 힙 전체·블록 시작만 유효. 1st는 두 폭 모두 `user32!wsprintfA`에서 정지
    - [x] [작업 419 — wsprintfA](work-logs/20260928-419-wsprintf.md). 측정한 서식 규칙의 공용 core, `ImportCall::arguments_address`로 가변 인자. 1st는 두 폭 모두 `user32!LoadImageA`에서 정지
    - [x] [작업 420 — Linux에서 1st SE CHD](work-logs/20260928-420-1stse-chd-linux.md). 환경에 1st가 없어 1st SE CHD로 이어 감. 로컬 `cfg/`에 1st SE Hardlock 자료 복원, 보호 계층 Hardlock 39건 통과, `SetBkColor`(측정), legacy I/O RVA 실행 확인. 작업 421·422 뒤 두 폭 모두 창을 닫을 때까지 돎
    - [x] [작업 421 — 파일에서 읽는 비트맵](work-logs/20260928-421-bitmap-files.md). 공용 BMP 해석, `LoadImageA`(LR_LOADFROMFILE·DIB section), `GetObjectA`·`CreateCompatibleDC`·`SelectObject`·1:1 `StretchBlt`·`DeleteDC`, 선택된 비트맵의 `DeleteObject` 미룸(측정). 1st SE(x64)는 `IDirectDrawSurface4::QueryInterface(IID_IDirect3DTexture2)`에서 정지
    - [x] [작업 422 — Linux DX6 texture](work-logs/20260928-422-dx6-textures.md). `IDirect3DTexture2`(표면 소유, 참조 수 공유)와 `Load`, `IDirect3DDevice3::SetTexture`, 색 채우기·복사 `Blt`, `BltFast`, `RestoreAllSurfaces`, light state, DX6 vertex buffer와 두 VB draw(Windows DX6 facade 규칙). 1st SE는 두 폭 모두 창을 닫을 때까지 돎
    - [x] 1st SE의 Linux 화면 확인: 타이틀·데모 플레이 화면 정상(2026-09-29 사용자 확인, WSLg 창 캡처)
    - [x] 1st SE의 Linux 코인·시작 입력과 소리: 정상(2026-09-29 사용자 확인)
    - [x] [작업 423 — 소프트웨어 페이싱](work-logs/20260929-423-software-present-pacing.md). swap이 막지 않는 host(WSLg)에서 공용 backend가 표시 주기로 present를 맞춤(`PresentPacer`). WSLg 1st SE 112 → 60 FPS, Windows에서는 켜지지 않음
    - [ ] `StretchBlt` 확대·축소 대응(설계 421)
    - [x] [작업 424 — 표면 Lock/Unlock](work-logs/20260929-424-surface-lock.md). Windows facade의 Lock 규칙을 공용 core(`PlanLock`)로, Linux Lock/Unlock. 4th의 F1 뒤 Linux가 멈추지 않고 Windows와 같게 검은 화면
    - [x] [작업 425 — 렌더 타깃 Lock](work-logs/20260929-425-render-target-lock.md). 화면에 내보내는 표면의 Lock은 GL 렌더 타깃에서 되읽고 Unlock은 다시 올림(공용 backend, 두 host). 4th의 F1(TEST) 테스트 모드 메뉴가 두 host에서 보임
    - [x] [작업 426 — 8비트 팔레트 DIB](work-logs/20260929-426-palettized-dib.md). `StretchDIBits`가 8비트 `BI_RGB` DIB를 받음(측정: 팔레트는 24비트와 같은 변환, 팔레트 밖 인덱스는 검정). Linux 5th가 두 폭 모두 창을 닫을 때까지 돎
    - [ ] 게스트 스레드의 남은 것: `TerminateThread`(1st의 스레드 종료 시간 초과 경로), `CREATE_SUSPENDED`/`ResumeThread`, `ExitThread`, `GetExitCodeThread`, 스레드별 메시지 큐. import 없이 도는 게스트 코드는 다른 스레드를 막음(설계 417)
    - [ ] Linux `DrawTextA`의 한글(CP949 2바이트)과 가변 폭: `System` 글꼴은 가변 폭("iW" 18px)이라 Unifont 고정 8px와 글자 배치가 다름. 한글이 필요해지면 Unifont 한글 글리프(16×16)를 더 가져옴
    - [x] [작업 399 — FindFirstFileA와 곡·스타일 목록](work-logs/20260927-399-find-files.md). 측정, 제품 VFS 목록 규칙을 core로(Windows VFS·그래픽 60초 기록 전후 같음), Linux `GuestFiles` 검색과 kernel32 세 export, `--api-log-calls`. 코인·시작 뒤 스타일 검색(10개)을 지나 게임 화면 준비 중 `CreateVertexBuffer`에서 정지
    - [x] [작업 427 — Linux ez2d2m](work-logs/20260929-427-ez2d2m-linux.md). 시리얼 함수 resolve-only, `WS_BORDER` 창(측정), 진단용 `UnhandledExceptionFilter`, Linux EZ2Dancer word 보드(공용 키 배치 `ez2dancer_keyboard_map`), 입력 helper 두 곳(`0xb169`·`0xb4cb`)은 opcode 매칭. 두 폭 모두 타이틀 화면까지 돌고 창을 닫을 때까지 실행
    - [x] [작업 428 — ez2d2m 시리얼 무효 핸들](work-logs/20260929-428-ez2d2m-serial.md). 실패한 COM1 핸들에 대한 overlapped `ReadFile`·`WriteFile`, 시리얼 함수 8개, `GetOverlappedResult`, `CloseHandle(-1)`(측정). 코인이 크레딧을 올린 뒤 멈추던 문제를 고침. 작업 427의 "코인이 오르지 않음"은 키 전달 실패로 인한 오판이었고, 추정 코인 매핑(`0x304` bit 0)은 두 host 모두 동작함
    - [ ] Linux `--io-config`(공용 INI 읽기), 키·마우스 창 메시지와 마우스 이동량
    - [x] [작업 398 — Linux 소리 출력](work-logs/20260927-398-linux-sound-output.md). `hle::HostAudio`, Linux `dsound.dll` 버퍼의 voice·host 사본(Windows facade 호출 순서), Linux SDL 오디오·SDL3_mixer와 `LinuxHostAudio`, `--audio-gain-db`. 두 폭이 재생 장치를 열고 커서가 장치를 따름(Windows 그래픽·오디오 기록 전후 같음, 스트리밍 lock 횟수만 시간 의존)
    - [ ] 남은 표면 메서드(`Lock`, `GetDC`와 Linux GDI DC, `Blt`, `Flip`), 게임이 도달하는 대로
    - [ ] DirectX 6 facade(`IDirect3D3`)의 텍스처 형식 열거(`FillRgb565Format`)도 공용 core `Rgb565Format`으로. `FindDevice`·Z 형식·`GetCaps`·viewport 변환은 작업 415·416에서 완료
    - [ ] (Linux 4th 플레이 가능 이후 검토) Windows도 HLE in-process runner로 통일해 네이티브 DirectX facade 제거 여부 설계. 걸리는 점: ddraw HLE가 `GuestGdi`·`GuestUser`·guest 메모리 모델에 기대므로 gdi32·user32 창/메시지·메모리까지 함께 옮겨야 하고, Windows 실행이 진짜 OS 동작 기준점 역할을 잃음
    - [ ] 그다음 message loop와 DirectInput·DirectSound
    - [ ] message queue(`PeekMessageA`/`DispatchMessageA`), window timer, `ShowCursor`/`SetCursor`
    - [ ] 게스트 호출 중 게스트 SEH가 host frame을 건너 unwind하는 경우
    - [ ] 게스트 루트 밖 경로(`windows` 지원 디렉터리)와 디렉터리 dump 실행의 게스트 파일
    - [x] guest thread와 막히는 대기(`CreateThread`, `WaitForSingleObject` 무한 대기): 작업 417. 다른 스레드가 없을 때의 `INFINITE` 대기는 여전히 정지
    - [ ] 일광 절약 규칙과 한국어 zone 이름(`GetTimeZoneInformation`)
    - [ ] CP949 두 byte 표(현재는 단일 byte만, 두 byte는 정지)
    - [ ] timer 전달과 message loop(`SetTimer` 기록은 작업 367)
  - [ ] [작업 353 — Linux x64 compatibility-mode adapter](design/20260924-353-linux-x64-compat-mode-adapter.md)
    - [x] [1단계 — 전환 runtime과 합성 probe](work-logs/20260924-353-linux-x64-compat-mode-adapter.md). `fsgsbase`·`arch_prctl` 두 경로에서 진입·복귀, TEB FS, import gate, SIGILL/SIGTRAP 포착, host TLS·callee-saved 보존 확인
    - [x] [2단계 — x64 `NativePeSession`·import thunk·in-process runner](work-logs/20260924-354-linux-x64-pe-session.md) (작업 354). 공용 코드를 루트로 옮기고 bootstrap·bridge·저주소 할당을 폭별 구현으로 분리. 합성 PE32가 x64에서 exit 51, SIGILL 포착 뒤 재실행까지 통과
    - [x] [3단계 — facade·kernel32 진단 연결, `original_runner.cpp` `#if` 분리](work-logs/20260924-355-linux-x64-facade-diagnostics.md) (작업 355). 실제 4th CHD가 x64에서 API 13개 뒤 게스트 `INT3`(`0x00af1136`)까지 x86과 같은 경로. GetVersion 진단은 trace 부재로 명시적 거절
    - [x] [4단계 — x64 게스트 SEH 디스패치(중첩 전환)와 instruction trace](work-logs/20260924-356-linux-x64-guest-seh-trace.md) (작업 356). 실제 4th CHD가 x64에서 x86과 같은 `#0016`에 도달, GetVersion trace 43 frame 일치
    - [x] [작업 357 — Linux 기본 `--run`을 in-process continuation으로 전환](work-logs/20260924-357-linux-default-in-process-run.md). helper는 `--linux-helper` 진단 fallback
    - [ ] 후속 — 게스트 entry 레지스터(EBX 등) 정의. 두 host 모두 host 잔여값을 넘김(작업 356 분석)
  - [ ] 후속 — guest handle/VFS, DLL별 `gdi32`·DirectX 확장, Windows adapter

  *Task 340 — Plan the guest PE compatibility modules. Follow with the platform-neutral module descriptors and registry (341), descriptor-driven PE32 facade builder (342), the four-export `kernel32` module and Linux i386 mapping (343), and pseudo-handle removal plus static/dynamic identity and real-4th regression (344), with the two earlier resolver diagnostics converged on the same path and a real `GetVersion` call confirmed (348), a continuation diagnostic that reaches 13 APIs and stops at the guest's own `INT3` (349), guest SEH chain observation confirming a registered handler at `0x00af159b` that loads `ContextRecord` (350), and guest SEH dispatch delivering `EXCEPTION_BREAKPOINT` and resuming at `0x00af11af` passing `#0014 CreateFileA("\\.\FEnteDev")` to `#0015 GetProcAddress(0, "GetActiveWindow")` (351), then a `user32` facade exporting only `GetActiveWindow`, after which `#0002` receives `0x6eff0000`, `#0015` receives thunk `0x6eff2000`, and the guest stops, without calling it, at `#0016 GetProcAddress(kernel32, "ExitProcess")` (352). Task 358 adds `ExitProcess` to the `kernel32` facade, expressing guest termination through `ImportReturn.exit_process`; on both widths `#0016` now receives a facade address and the run stops at `#0019`, the static `user32!MessageBoxA`, after `GetModuleHandleA("DDRAW.DLL")` returns 0. Task 359 adds `user32!MessageBoxA`, and on both widths the real 4th CHD then shows `"Error 1009 : Cannot open Hardlock driver."` (caption `Hardlock`) and exits normally through `ExitProcess(9)`, the first use of Task 358's exit path on the original. Toward a Linux Hardlock device HLE, Task 360 moves the Hardlock material assembly, `DeviceIoControl` completion rules, and device-path match into the shared core with an identical Windows Hardlock record, and corrects WTS class 4 to `WTSSessionId`; Task 361 adds guest device handles and the `kernel32` device calls, so on both Linux widths the real 4th opens `\\.\FEnteDev`, has its initialize answered, and stops at `GetProcAddress(user32, "CreateCursor")`; Task 363 adds the Hardlock API startup environment (guest process state, declared absent names, seven `kernel32` and three `user32` exports, and the `advapi32` and `wtsapi32` facades), so both widths receive WTS session 0, complete two handshakes and one descriptor, and stop at `GetProcAddress(kernel32, "OpenProcess")`; Task 364 adds own-process memory (a shared handle space, recorded page protections for image and `VirtualAlloc` regions, `OpenProcess`, `Virtual*`, and `LocalAlloc/Free`), so both widths finish the decryption loop with Windows' 37 descriptors and 36 transforms and stop at `GetProcAddress(kernel32, "GetCurrentThreadId")`; Task 367 finishes the envelope (the original import table rebuilt across ten DLLs with resolve-only exports, facades as RWX-code image regions for the `ExitProcess` hook, `Read/WriteProcessMemory`, and thread timers), so both widths reach the original CRT's `GetVersion` at `0x004c4424` and stop at `HeapCreate`; Task 368 completes the original CRT start-up (heaps, start-up information, command line and environment, measured code page 949, module paths, and the stop stub kept from guest SEH), stopping at the game code's first call, `CreateEventA`; Task 369 adds unnamed events, a host clock service with the time exports, and `GetTimeZoneInformation`, passing C++ static initialization and the game's own Hardlock login into WinMain, which stops at `timeBeginPeriod`. Task 353 starts the Linux x64 compatibility-mode adapter: stage 1, the transition runtime and synthetic probe, passes on both the `fsgsbase` and `arch_prctl` paths, and stage 2 (Task 354) runs the synthetic PE32 through the shared `NativePeSession` and runner on x64; stage 3 (Task 355) wires facades and the kernel32 diagnostic so the real 4th CHD follows the x86 path on x64 up to the guest `INT3` after 13 APIs, and stage 4 (Task 356) adds guest SEH dispatch and the instruction trace, so the x64 host reaches the same `#0016` and 43-frame trace as x86; defining guest entry registers such as EBX, which both hosts currently leave as host leftovers, is follow-up work. Guest handles/VFS, per-DLL expansion, and the Windows adapter remain later work.*

- [x] [작업 294 — 작업 293 후속](work-orders/20260917-294-port-helper-dump-crosscheck.md) — 보호 빌드 `resumed` 덤프로 프로파일 helper RVA 대조. 바이트 폭 다섯 빌드 모두 일치
- [ ] 3rd present 비용 변화 원인 — 2026-09-17 01:25~01:49 사이 실행 환경 변화 뒤 present가 약 15 ms 블록되고 60 fps로 바뀜. OSD 코드와 무관함은 A/B로 확인([작업 297 로그](work-logs/20260917-297-imgui-osd-autoplay.md))
- [ ] `ez2dj3rd` 입력 슬롯 `0x1b`의 물리 바인딩 — 게임 내장 autoplay 토글([분석](analysis/ez2dj3rd-demo-play.md))
- [ ] 나머지 타깃의 `game_controls` — `game-state-hunt` 스킬로 빌드별 autoplay 플래그 확인. 완료: `ez2dj3rd`(작업 297), `ez2dj4th`(작업 300), `ez2dj5th`(작업 301), `ez2dj1stse`(작업 302). `ez2d2m`(작업 305). 변수 없음: `ez2dj1st`(작업 304, 데모 전용 플레이어 장면). 남은 후보: `ez2dj2nd`(덤프 미수집), `ez2dj6th`(자식 프로세스 구조로 현재 덤프 불가)
- [x] [작업 303](work-logs/20260918-303-directsound-static-oneshot.md) — `ez2dj1stse`·`ez2dj1st` 효과음 무한 반복. `STATIC` 효과음을 스트리밍에서 제외하고 스트리밍 경로가 `DSBPLAY_LOOPING`을 따르게 함. 사용자 청취 확인
- [ ] OSD 입력 공급의 Linux 경로
- [x] [작업 365 — host 실행 출력의 spdlog 전환](work-logs/20260925-365-host-run-output-spdlog.md). CLI 실행 출력과 Windows launcher의 JSONL·오류·요약이 `re2dj_logging`을 거침. stdout에는 help·version·사용법만 남음
- [x] [작업 366 — injected runtime 기록의 spdlog 전환](work-logs/20260925-366-injected-runtime-spdlog.md). `WriteRuntimeLog` 네 channel(runtime·VFS·graphics·audio), ODS 문구 유지, `*.runtime.log` 추가, last error 보존
- [x] [작업 362](work-logs/20260924-362-vfs-runtime-probe-repair.md) — `re2dj_windows_vfs_runtime_probe` 실패 원인. hang이 아니라 probe 결함 세 개(작업 303 뒤 `STATIC` 스트리밍 fixture, 옛 `dirty-*` trace 이름, 열린 stream과 mixer trace 때문에 정리 단계 `remove_all` 예외로 abort). Windows CTest 6/6
- [ ] 작업 119 — Windows x86 4th dynamic <code>GetProcAddress</code> VFS HLE
- [ ] Windows x86 INI API HLE (`GetPrivateProfile*`, `WritePrivateProfileStringA`)
- [ ] Windows x86 directory enumeration (`FindFirstFileA`, `FindNextFileA`, `FindClose`)
- [ ] GitHub Actions first workflow verification

## 분석 미완료 / Analysis remaining

- [ ] `Songs/` 아래 자산 파일 형식 분석
- [ ] 3rd target guest drive and working-directory evidence
- [ ] 실패 경로 continuation buffer(힙 페이지)의 의미 추적 — entry 직후 XOR 루프 대상 버퍼와 `[0x01ed7074]` 플래그 기입 지점
- [ ] 3rd 보호 빌드(`EZ2DJ.EXE`)의 import 표면과 런타임 흐름

## 보류 / Deferred

- [ ] Windows x64 host expansion
- [ ] Native 32-bit Linux kernel에서의 실행 검증

- [ ] Task 096 visual revalidation: confirm scene-transition flicker/fade-out and z-order behavior with the user's current display setup
- [ ] 작업 097 Music Select 좌표·창 pixel viewport 재검증 (창 크기/DPI 변경 포함)

*Task 097's trace shows internally centered logical coordinates for the Music Select composition, while the SDL/OpenGL backend now refreshes its pixel viewport after native child-window or DPI size changes. The exact user-visible artwork position still needs a reproducible Music Select capture.*

- [ ] Task 097 coordinate revalidation: confirm Music Select artwork placement after window resize or DPI changes
