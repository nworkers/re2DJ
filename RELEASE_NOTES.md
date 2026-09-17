# 릴리즈 노트 / Release Notes

## v0.0.48 (2026-09-18)

### 한국어

- **문서 보완**: 작업 291~297에서 확인한 원본 분석 결과(보호 빌드 복호화 시점, helper RVA 대조, 3rd 설정 레지스트리·데모·autoplay 플래그, 프레임 pacing)를 `EXE_DESIGN`에 누적하고, Win32 커서·자식 창 입력 배경 문서와 후속 TODO를 추가했습니다. 코드 변경은 없습니다.

---

### English

- **Documentation**: Accumulated the original-analysis findings of tasks 291-297 (protected-build decryption timing, helper RVA cross-check, 3rd settings registry, demo and autoplay flags, frame pacing) into `EXE_DESIGN`, and added a background topic on Win32 cursor and child-window input plus follow-up TODO entries. No code change.

---

## v0.0.47 (2026-09-17)

### 한국어

- **Dear ImGui 기반 On-Screen Display(OSD) 추가**: 실행 중 백틱(`` ` ``) 키로 토글할 수 있는 가벼운 OSD를 도입했습니다. 숨김 상태에서는 ImGui 프레임을 구성하지 않아 렌더링 비용이 발생하지 않습니다. 화면 상단에 버전, 빌드 일시, 타깃 프로파일, 실행 파일 이름을 표시합니다.
- **EZ2DJ 3rd Trax 자율 연주(Autoplay) 토글 지원**: 복호화 덤프 분석으로 확인된 내부 autoplay 플래그 주소(`0x00629508`)를 타깃 프로파일의 `game_controls`에 등록했습니다. 런처는 실행 파일의 빌드 timestamp(`0x3bca98a3`)가 일치할 때만 주소를 주입 런타임에 전달해 안전하게 무장하며, 곡 시작 전에 OSD에서 체크하면 해당 곡이 자동으로 연주됩니다. 데모 오버레이나 음소거 등 데모 플레이 부작용이 없습니다.
- **마우스 커서 표시 복구**: 3rd가 커서를 숨기더라도 클라이언트 영역 위에서 마우스 커서가 유지되어 OSD를 조작할 수 있도록 했습니다.

---

### English

- **Dear ImGui On-Screen Display (OSD)**: Introduced a lightweight OSD toggled with backtick (`` ` ``). No ImGui frame is built while hidden, incurring zero rendering overhead. Displays version, build timestamp, target profile, and executable name across the top of the window.
- **Autoplay Toggle for EZ2DJ 3rd Trax**: Registered the internal autoplay flag address (`0x00629508`) in target profile `game_controls`. Armed only when the executable build timestamp (`0x3bca98a3`) matches. Ticking Autoplay before song start plays the song autonomously without demo-play side effects.
- **Mouse Cursor Restoration**: Restored the arrow cursor over the client area to ensure easy interaction with the OSD despite 3rd hiding the cursor.

---

## v0.0.31 (2026-09-07)

### 한국어

런타임 핫 경로 성능 개선: CHD/FAT32 판독 캐시, OpenGL draw 경계 고정 비용 제거, draw 경로 진단 게이트

게스트가 관찰하는 바이트, 픽셀, 상태는 바뀌지 않습니다. 성능 특성만 바뀝니다.

#### 1. CHD/FAT32 판독 캐시 (작업 219)
- **CHD hunk 캐시 추가**: 압축 해제된 hunk의 LRU 캐시 `re2dj::storage::ChdHunkCache`를 저장소 계층의 독립 구성요소로 추가했습니다. `libchdr`의 `chd_read`는 호출마다 다시 압축을 풀기 때문에, 512바이트 sector 하나를 읽을 때마다 4,096바이트 hunk 전체를 LZMA 해제하던 비용을 이 계층이 흡수합니다.
- **FAT32 조회 캐시 추가**: `Fat32Volume`에 디렉터리 항목, 해석된 경로, 파일 클러스터 체인 캐시를 넣었습니다. `ReadFileRange`가 호출마다 경로를 다시 해석하고 FAT 체인을 첫 클러스터부터 다시 걷던 제곱 동작을 제거했고, 중간 클러스터 버퍼 없이 목적지 버퍼로 직접 판독합니다.
- **판독 경로 직렬화**: 게스트가 여러 스레드에서 파일 API를 호출하므로 공개 판독 API를 `std::mutex`로 직렬화했습니다. 이전에는 잠금이 없었고 `libchdr`의 내부 버퍼가 공유 상태였습니다.
- 실제 4th CHD에 대한 `re2dj_chd_probe` 출력이 변경 전후 바이트 단위로 동일함을 확인했습니다.

#### 2. OpenGL draw 경계 고정 비용 제거 (작업 220)
- draw 1회마다 반복되던 `SDL_GL_MakeCurrent` 1회, `glGetUniformLocation` 5회, 정점 속성 배열 활성/비활성 6회, `glTexParameteri` 4회, `std::vector` 힙 할당 1회를 제거했습니다.
- uniform location은 프로그램 링크 직후 한 번만 조회하고, 정점 변환 버퍼는 재사용하며, 텍스처 샘플러 상태는 값이 실제로 바뀔 때만 설정합니다.
- draw별 `glGetError`는 초기 256 draw와 진단 실행으로 한정합니다. `Present`의 프레임 단위 검사는 그대로 유지하므로 지속적인 GL 실패는 계속 검출됩니다.

#### 3. draw 경로 진단 게이트 (작업 221)
- **`--graphics-draw-diagnostics` 옵션 추가**: draw 경로 안에서 실행되던 `ReportDrawDiagnostic`, `ReportLateDrawDiagnostic`, `ReportTransformDiagnostic`을 새 스위치 뒤로 옮겼습니다. 기본값은 꺼짐이며 제품 실행 경로는 이 비용을 지불하지 않습니다.
- 실제 4th CHD 실행에서 `.ddraw.log`가 21,117줄에서 631줄로 줄었고, 텍스처 전 픽셀 스캔과 긴 레코드 포맷팅을 유발하던 draw 단위 항목 20,480건이 사라졌습니다. 초기화 진단은 그대로 기록됩니다.
- draw 단위 증거가 필요한 조사에서는 이 옵션을 명시적으로 켭니다. 관련 가이드와 분석 문서를 함께 갱신했습니다.

#### 4. 문서
- [런타임 핫 경로 성능 설계](docs/design/20260907-219-runtime-performance-hot-paths.md), 작업 지시 3건, 작업 로그 3건을 추가했습니다.
- `libchdr`에 hunk 캐시가 없다는 일반 기술 배경을 [docs/kb/mame-chd-hunk-decompression.md](docs/kb/mame-chd-hunk-decompression.md)에 정리했습니다.
- 4th CHD의 codec 목록과 hunk 수를 분석 문서에 반영하고, CHD 파일 이름이 인식 조건이 아니라는 점을 명시했습니다.

---

### English

Runtime hot-path performance: CHD/FAT32 read caches, per-draw fixed cost removal in the OpenGL boundary, and a gate for the draw-path diagnostics.

The bytes, pixels, and state the guest observes are unchanged; only performance characteristics change.

#### 1. CHD/FAT32 Read Caches (Task 219)
- **CHD hunk cache**: Added `re2dj::storage::ChdHunkCache`, an LRU of decompressed hunks, as its own component in the storage layer. `libchdr`'s `chd_read` decompresses on every call, so reading one 512-byte sector fully decompressed a 4,096-byte LZMA hunk; that cost is now absorbed here.
- **FAT32 lookup caches**: Added directory-entry, resolved-path, and cluster-chain caches to `Fat32Volume`. This removes the quadratic behavior where `ReadFileRange` re-resolved the path and re-walked the FAT chain from the first cluster on every call, and reads now go straight into the caller's destination without an intermediate cluster buffer.
- **Serialized read path**: The public read API is now guarded by a `std::mutex`, since the guest calls the file APIs from several threads; previously there was no lock and `libchdr`'s internal buffers were shared state.
- Verified that `re2dj_chd_probe` output on the real 4th CHD is byte-identical before and after.

#### 2. OpenGL Draw Boundary Fixed Cost (Task 220)
- Removed the per-draw `SDL_GL_MakeCurrent`, five `glGetUniformLocation` lookups, six vertex-attribute-array toggles, four `glTexParameteri` calls, and one vector allocation.
- Uniform locations are resolved once after link, the vertex conversion buffer is reused, and texture sampler state is applied only when a value actually changes.
- The per-draw `glGetError` now runs for the first 256 draws and during diagnostic runs; `Present` keeps its unconditional per-frame check, so a persistently broken GL state is still detected.

#### 3. Draw-Path Diagnostic Gate (Task 221)
- **Added `--graphics-draw-diagnostics`**: `ReportDrawDiagnostic`, `ReportLateDrawDiagnostic`, and `ReportTransformDiagnostic` now sit behind a switch that defaults to off, so the product execution path does not pay for them.
- On the real 4th CHD the `.ddraw.log` dropped from 21,117 lines to 631, removing the 20,480 per-draw entries that drove whole-surface texel scans and long record formatting. Initialization diagnostics still record.
- Investigations that need draw-level evidence turn the option on explicitly; the related guides and analysis documents were updated accordingly.

#### 4. Documentation
- Added the [runtime hot-path performance design](docs/design/20260907-219-runtime-performance-hot-paths.md), three work orders, and three work logs.
- Recorded the absence of a hunk cache in `libchdr` as general background in [docs/kb/mame-chd-hunk-decompression.md](docs/kb/mame-chd-hunk-decompression.md).
- Recorded the 4th CHD codec set and hunk count in the analysis document, and noted that the CHD file name is not part of recognition.

---

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
