# TODO

현재 진행 중인 작업과 아직 결정되지 않은 항목만 기록합니다. 완료 항목은 [구현 완료 항목](IMPLEMENTED.md)으로 이동합니다.

*This file contains only active work and unresolved items. Completed items are moved to [Implemented](IMPLEMENTED.md).*

작업 086의 `DemoVolume` HLE로 확인된 title 음량 저하 원인은 제거됐다. 실제 전체 곡·효과음 청취 정확성은 작업 072의 사용자 재검증 항목으로 유지하며 Linux 작업 077은 사용자 결정에 따라 잠시 보류한다.

*Task 086's `DemoVolume` HLE removes the confirmed title-volume attenuation. Audible accuracy across complete songs and effects remains a user-revalidation item under Task 072; Linux Task 077 remains temporarily paused by user decision.*

## 현재 진행 / In progress

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
