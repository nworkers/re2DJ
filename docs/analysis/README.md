# 분석 색인 / Analysis Index

이 디렉터리는 원본 EZ2DJ 바이너리와 HDD 자산에서 **직접 확인한** 프로젝트 고유 사실을 주제별로 누적한다. 일반 기술 배경은 [`docs/kb/`](../kb/README.md)에 둔다.

*This directory accumulates project-specific facts **verified directly** against the original EZ2DJ binaries and HDD assets, organized by topic. General background knowledge lives in [`docs/kb/`](../kb/README.md).*

## 표기 규칙 / Notation

모든 서술은 **확인됨 / 추정 / 미확정** 중 하나로 표기한다. 확인됨에는 검증 방법을, 추정에는 근거를, 미확정에는 확인 방법을 함께 적는다.

*Every statement is marked **confirmed**, **inferred**, or **unresolved**, alongside the verification method, the evidence, or the way to find out.*

## 문서 / Documents

- [Linux 실행 기준선 / Linux runtime baseline](linux-runtime-baseline.md): WSL x86·x64 합성 실행·빌드 확인과 원본 실행 미확정 범위 / Verified WSL x86/x64 synthetic execution/builds and unresolved original execution.

| 문서 | 내용 | 현재 상태 |
| --- | --- | --- |
| [ez2dj-hdd-layout.md](ez2dj-hdd-layout.md) | HDD 덤프의 디렉터리 구조와 실행 파일 식별 | 1st Tracks·1st SE·2nd·3rd·5th·6th 덤프와 보조 도구로 확인됨 |
| [ez2dj-exe-structures.md](ez2dj-exe-structures.md) | 실행 파일별 PE 구조, 보호 계층 해부, 데이터 인벤토리, 런타임 흐름, 파일 해시 | 1st Tracks·1st SE 두 빌드·2nd·3rd·4th·5th·6th 세 실행 파일·`ez2d2m` 헤더로 확인됨 |
| [ez2dj-import-surface.md](ez2dj-import-surface.md) | 원본이 실제로 호출하는 Win32 API 집합과 HLE 우선순위 | 정식 빌드의 원본 `.idata`와 packed table로 확인됨 (1st Tracks·1st SE·3rd·4th·5th·6th·`ez2d2m`) |
| [ez2dj-asset-loading-path.md](ez2dj-asset-loading-path.md) | 자산 검색 경로 테이블, BMP `LoadImageA` 경계, `.str` 스크립트 참조, 이중 IAT 구조 | 1st SE `ez2dj.exe` 정적 분석과 detached 실행 로그로 확인됨 |
| [ez2dj-io-map.md](ez2dj-io-map.md) | legacy I/O port 범위, 공개 구현 교차 확인 의미, helper 시그니처와 프로파일 RVA 확인 상태 | 원본 확인/외부 추정/미확정 분리. 보호 빌드의 helper RVA는 덤프 대조 미완료 |
| [ez2dj-demo-volume.md](ez2dj-demo-volume.md) | `DemoVolume` INI 경로, 원본 DirectSound profile table과 실제 실행 귀속 | 1st SE 정식 빌드 `.text` 정적 분석과 실제 실행으로 확인됨 |
| [win32-caption-dpi.md](win32-caption-dpi.md) | DWM caption 결손과 DPI frame 계산 순서 | 1st SE 실제 제품 실행으로 확인됨 |
| [ez2dj3rd-frame-pacing.md](ez2dj3rd-frame-pacing.md) | 3rd의 sleep 기반 프레임 리미터, 목표 주기 피드백, SDL3 타이머 해상도 의존 | 3rd 실제 실행 계정과 원본 `.idata`로 확인됨. 원본 목표 주기 상수는 미확정 |
| [protected-build-runtime-decryption.md](protected-build-runtime-decryption.md) | `.protect` 빌드의 런타임 복호화 시점, 실행 중 이미지 덤프, 자체 INI 파서 | 3rd 두 시점 덤프 비교로 확인됨. 단계적 복호화 여부와 타 빌드는 미확정 |
| [ez2d2m-demo-play.md](ez2d2m-demo-play.md) | EZ2Dancer 2nd MOVE의 클래스 쌍 구조, 채널 모드 설정 함수, autoplay 플래그와 내장 토글 | 덤프 정적 해석과 OSD 동작으로 확인됨. 입력 슬롯 `0xc`의 바인딩과 채널 값의 의미는 미확정 |
| [ez2dj1st-demo-play.md](ez2dj1st-demo-play.md) | 1st Tracks 장면 엔진, 데모 전용 플레이어 장면, 곡 재생기의 상수 자동 인자와 autoplay 변수 부재 | 덤프 정적 해석, 런타임 읽기, 쓰기 시험으로 확인됨. 자동 연주가 데모 장면 코드에 있다는 결론은 추정 |
| [ez2dj1stse-demo-play.md](ez2dj1stse-demo-play.md) | 1st SE 장면 엔진, 자동 플레이 장면(`DemoGame`·`ClubMixDemoGame`·`HowToPlayGame`)과 autoplay 플래그, 효과음 스트리밍 분류 문제 | 덤프 정적 해석, 런타임 읽기, OSD 동작으로 확인됨. 효과음 반복은 작업 303에서 해결 |
| [ez2dj5th-demo-play.md](ez2dj5th-demo-play.md) | 5th 설정 레지스트리(`AutoScratch`·`AutoPedal` 포함), 데모 플래그, autoplay 플래그와 4th 구조 대조 | 덤프 정적 해석, 런타임 읽기, OSD 동작으로 확인됨. 보조 자동 설정의 동작은 미확정 |
| [ez2dj4th-demo-play.md](ez2dj4th-demo-play.md) | 4th 설정 레지스트리, 데모 플래그, autoplay 플래그와 3rd 구조 대조 | 덤프 정적 해석, 런타임 읽기, OSD 동작으로 확인됨. 곡 시작 고정 여부와 판정 계열 두 벌의 의미는 미확정 |
| [ez2dj3rd-demo-play.md](ez2dj3rd-demo-play.md) | 3rd 설정 키-주소 레지스트리, 데모 플레이 플래그와 시작 루틴, 입력 매니저 바인딩 | 덤프 정적 해석과 런타임 읽기로 확인됨. 노트 자동 판정 지점은 미확정 |

새 분석 문서를 추가하거나 이름을 바꾸면 같은 작업에서 이 표를 갱신한다.

| [graphics-transition-depth.md](graphics-transition-depth.md) | 장면 전환 fade 후보와 Direct3D/OpenGL 깊이 상태 경계 | 작업 096 ddraw trace와 Win32 실행으로 확인 |

| [ez2dj1stse-chd-filesystem.md](ez2dj1stse-chd-filesystem.md) | 1st SE CHD v5, FAT32 geometry, StartUp guest boot path, the `.protect` vs `.gtide` executable difference, the FEnteDev Hardlock IOCTL sequence, the resolved transform response, and the Linux in-process run | Hardlock transform response resolved; Windows reaches gameplay; Linux passes the Hardlock and bitmap loading, with legacy-I/O RVAs confirmed |
| [ez2dj3rd-hardlock-function-0e.md](ez2dj3rd-hardlock-function-0e.md) | 3rd Hardlock device name, API descriptor, and Function 0x0e boundary | Device/API boundary confirmed; valid 0x0e response unresolved |
| [ez2dj3rd-chd-filesystem.md](ez2dj3rd-chd-filesystem.md) | 3rd Trax CHD v5, FAT32 geometry, and internal executable path | User-supplied CHD read confirmed; full protection-contract identity remains unresolved |
| [ez2dj4th-chd-filesystem.md](ez2dj4th-chd-filesystem.md) | 4th Trax CHD v5, FAT32 geometry, directory and executable layout, codec set | Real 4th CHD read confirmed under two file names; cabinet boot sequence unresolved |
| [ez2dj5th-6th-chd-filesystem.md](ez2dj5th-6th-chd-filesystem.md) | 5th/6th CHD geometry and executable/filesystem observations | 6th FAT32 path, its three executables and the bundled 1st Tracks layout confirmed; 5th filesystem unresolved |
| [ez2dj4th-hardlock-runtime.md](ez2dj4th-hardlock-runtime.md) | 4th Hardlock config, device/IOCTL sequence, DirectDraw/D3D7 startup, and validation boundary | Vendor driver, raw I/O, DDraw/D3D7 entry confirmed; D3D7 adapter pending |
| [ez2dj6th-hardlock.md](ez2dj6th-hardlock.md) | 6th Hardlock transform input boundary and unresolved Function `0x0011` response | Runtime 7-input maps fully match, but no candidate reaches accepted post-transform execution |
| [ez2dj4th-music-select-disc-state.md](ez2dj4th-music-select-disc-state.md) | Music Select 원판 draw의 cull, blend, UV, texture-transform 진단 | cull 직접 원인 제외; focused runtime trace 대기 |
| [ez2dj4th-graphics-path.md](ez2dj4th-graphics-path.md) | 4th가 DirectX 7 경계에 요구하는 인터페이스·표면·디바이스와 현재 차단 지점 | 표면 형식과 디바이스 요구는 실행으로 확인됨, present와 종료 원인은 미확정 |
| [ez2d2m-chd-filesystem.md](ez2d2m-chd-filesystem.md) | EZ2Dancer 2nd MOVE CHD의 볼륨 geometry, Windows 98 SE 배치, 게임 디렉터리, `EZ2Dancer.exe` PE 구조와 packed import directory | 정적 관찰로 확인됨, Hardlock 응답과 실행 성공은 미확정 |
| [ez2dancer-io-map.md](ez2dancer-io-map.md) | EZ2Dancer의 16비트 폭 `0x300`~`0x30c` 포트 맵과 EZ2DJ byte 폭 보드와의 차이 | 전부 공개 구현 기반 추정, 원본 helper RVA는 미확정 |
| [ez2d2m-ez2dj4th-graphics-clear.md](ez2d2m-ez2dj4th-graphics-clear.md) | `ez2d2m`/`ez2dj4th` retained-frame presentation and Direct3D clear forwarding | Both `DDSCAPS_FLIP` and `LegacyDeviceClear` traces confirmed; HLE omission fixed, new-screen verification pending |

| [ez2d2m-jam-audio-runtime.md](ez2d2m-jam-audio-runtime.md) | EZ2Dancer JAM VFS/DirectSound/SDL runtime audio evidence and streaming Play semantics | 2026-09-14 VFS, PCM delivery, and repeated-Play HLE divergence confirmed |

*Update this table in the same task whenever an analysis document is added or renamed.*

- [EZ2DJ 4th Linux in-process first import](ez2dj4th-linux-inprocess-first-import.md): Confirmed first `GetModuleHandleA("kernel32")` caller return under Linux x86 in-process runner.
