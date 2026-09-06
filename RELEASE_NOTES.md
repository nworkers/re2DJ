# 릴리즈 노트 / Release Notes

## v0.0.30 (2026-09-06)

### 한국어

EZ2DJ 3rd Trax (MAME CHD) 완전 실행(그래픽 60 FPS, 사운드, 키보드 조작) 지원 및 DirectDraw 7 HLE 계층 구현

#### 1. EZ2DJ 3rd Trax 완전 실행 지원
- **MAME CHD 기반 VFS 마운트**: `roms/ez2dj3rd/ez2dj3rd.chd` 이미지를 직접 인식하여 FAT32 파일시스템 상의 `EZ2DJ/EZ2DJ.EXE` 및 리소스(BG, Sound, System)를 동적으로 읽어 실행하도록 지원.
- **오디오 출력**: DirectSound HLE 스트리밍 링 버퍼를 통해 BGM 및 키음의 정상 출력을 확인.

#### 2. DirectDraw 7 HLE 및 윈도우 모드 프레젠테이션
- **DirectDraw 7 계층 구현**: `DirectDrawCreateEx` thunk 및 `IDirectDraw7` 인터페이스 구현.
- **클리퍼 및 프라이머리 서피스 분리**: `CreateClipper` 및 `SetHWnd` 지원, 백버퍼 없는 단독 `PrimarySurface` 생성 지원.
- **윈도우 모드 60 FPS 프레임 표시**: 윈도우 모드 데스크톱 화면 좌표계를 수용하고, `PrimarySurface->Blt` 호출 시 OpenGL FBO 버퍼를 호스트 SDL 창으로 스왑(`Present`)하도록 연동하여 안정적인 60 FPS 화면 갱신을 달성.

#### 3. I/O 포트 에뮬레이션 및 키보드 입력 지원
- **3rd 전용 Legacy I/O 헬퍼 RVA 확정**: 메모리 역어셈블 분석을 통해 `in al, dx`(`0x000a9887`) 및 `out dx, al`(`0x000a98bb`) 헬퍼 주소를 확정하고 포트(`0x101`~`0x106`)를 에뮬레이션 버스에 연결.
- **키보드 파싱 확장**: `config/ez2dj-io.example.ini`의 1P 턴테이블 키(`TAB`)를 비롯하여 `ESC`, `SHIFT`, `CTRL`, `ALT`, `BACKSPACE`, `CAPS`, `INSERT`, `DELETE`, `HOME`, `END`, `PAGEUP`, `PAGEDOWN` 등의 파싱 지원 추가.
- **설정 파일 경로 정규화**: `--io-config` 인자를 절대 경로로 자동 변환하여 게스트 작업 디렉터리 경로 불일치 문제 해결. 코인 투입(F5), 스타트(1/2), 건반, 턴테이블, 페달 조작 지원.

#### 4. 릴리즈 빌드 스크립트 추가
- 최적화된 바이너리를 빌드하고 검증하기 위한 `scripts/build_release.bat` 및 `scripts/build_release.ps1` 추가.

---

### English

Full execution support for EZ2DJ 3rd Trax (60 FPS graphics, audio, and keyboard controls) and DirectDraw 7 HLE implementation.

#### 1. Full EZ2DJ 3rd Trax Execution Support
- **MAME CHD-Backed VFS Mount**: Directly mounts `roms/ez2dj3rd/ez2dj3rd.chd`, dynamically reading `EZ2DJ/EZ2DJ.EXE` and game assets (BG, Sound, System) from the FAT32 volume.
- **Audio Playback**: Confirmed pristine BGM and key sound playback through the DirectSound HLE streaming ring buffer.

#### 2. DirectDraw 7 HLE & Windowed Presentation
- **DirectDraw 7 Layer Implementation**: Added `DirectDrawCreateEx` export thunk and `IDirectDraw7` interface wrapper.
- **Clipper & Standalone Primary Surface**: Added support for `CreateClipper`, `SetHWnd`, and standalone `PrimarySurface` creation without backbuffers.
- **Windowed 60 FPS Frame Presentation**: Accepts screen-space desktop coordinates and triggers host SDL window buffer swaps (`Present`) inside `PrimarySurface->Blt`, achieving smooth and stable 60 FPS rendering.

#### 3. I/O Port Emulation & Keyboard Input Support
- **Confirmed 3rd Legacy I/O Helper RVAs**: Identified `in al, dx` (`0x000a9887`) and `out dx, al` (`0x000a98bb`) helper instruction RVAs through memory disassembly, mapping ports `0x101` through `0x106` to the I/O bus.
- **Extended Keyboard Key Parsing**: Added support in `ParseKey` for `TAB` (`p1_positive=TAB`), `ESC`, `SHIFT`, `CTRL`, `ALT`, `BACKSPACE`, `CAPS`, `INSERT`, `DELETE`, `HOME`, `END`, `PAGEUP`, and `PAGEDOWN`.
- **Configuration Path Canonicalization**: Canonicalized `--io-config` paths to absolute paths, resolving guest working directory path mismatches and enabling full control for Coin (F5), Start (1/2), Keys, Turntables, and Pedal.

#### 4. Release Build Scripts
- Added `scripts/build_release.bat` and `scripts/build_release.ps1` for building and verifying optimized Release binaries.
