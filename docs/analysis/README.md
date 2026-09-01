# 분석 색인 / Analysis Index

이 디렉터리는 원본 EZ2DJ 바이너리와 HDD 자산에서 **직접 확인한** 프로젝트 고유 사실을 주제별로 누적한다. 일반 기술 배경은 [`docs/kb/`](../kb/README.md)에 둔다.

*This directory accumulates project-specific facts **verified directly** against the original EZ2DJ binaries and HDD assets, organized by topic. General background knowledge lives in [`docs/kb/`](../kb/README.md).*

## 표기 규칙 / Notation

모든 서술은 **확인됨 / 추정 / 미확정** 중 하나로 표기한다. 확인됨에는 검증 방법을, 추정에는 근거를, 미확정에는 확인 방법을 함께 적는다.

*Every statement is marked **confirmed**, **inferred**, or **unresolved**, alongside the verification method, the evidence, or the way to find out.*

## 문서 / Documents

| 문서 | 내용 | 현재 상태 |
| --- | --- | --- |
| [ez2dj-hdd-layout.md](ez2dj-hdd-layout.md) | HDD 덤프의 디렉터리 구조와 실행 파일 식별 | 1st SE / 3rd 덤프로 확인됨 |
| [ez2dj-exe-structures.md](ez2dj-exe-structures.md) | 실행 파일별 PE 구조, 보호 계층 해부, 데이터 인벤토리, 런타임 흐름 | 1st SE 두 파일·3rd 헤더로 확인됨, 런타임은 `ez2dj.exe`만 |
| [ez2dj-import-surface.md](ez2dj-import-surface.md) | 원본이 실제로 호출하는 Win32 API 집합과 HLE 우선순위 | `ez2dj1.exe`·보호 빌드 `.gidata`로 확인됨 |
| [ez2dj-asset-loading-path.md](ez2dj-asset-loading-path.md) | 자산 검색 경로 테이블, BMP `LoadImageA` 경계, `.str` 스크립트 참조, 이중 IAT 구조 | 1st SE `ez2dj.exe` 정적 분석과 detached 실행 로그로 확인됨 |
| [windows-original-process-loader.md](windows-original-process-loader.md) | Windows loader가 원본 EXE를 주 이미지로 배치한 관찰 | `ez2dj1.exe` suspended process로 확인됨 |
| [ez2dj-io-map.md](ez2dj-io-map.md) | legacy I/O port 범위와 공개 구현 교차 확인 의미 | 원본 확인/외부 추정/미확정 분리 |
| [ez2dj-demo-volume.md](ez2dj-demo-volume.md) | `DemoVolume` INI 경로, 원본 DirectSound profile table과 실제 실행 귀속 | 1st SE 대응 unprotected binary와 실제 실행으로 확인됨 |
| [win32-caption-dpi.md](win32-caption-dpi.md) | DWM caption 결손과 DPI frame 계산 순서 | 1st SE 실제 제품 실행으로 확인됨 |

새 분석 문서를 추가하거나 이름을 바꾸면 같은 작업에서 이 표를 갱신한다.

| [graphics-transition-depth.md](graphics-transition-depth.md) | 장면 전환 fade 후보와 Direct3D/OpenGL 깊이 상태 경계 | 작업 096 ddraw trace와 Win32 실행으로 확인 |

| [ez2dj3rd-hardlock-function-0e.md](ez2dj3rd-hardlock-function-0e.md) | 3rd Hardlock device name, API descriptor, and Function 0x0e boundary | Device/API boundary confirmed; valid 0x0e response unresolved |
| [ez2dj4th-chd-filesystem.md](ez2dj4th-chd-filesystem.md) | 4th Trax CHD v5, FAT32 geometry, directory and executable layout | Real `4thTrax.chd` read confirmed; cabinet boot sequence unresolved |
| [ez2dj4th-hardlock-runtime.md](ez2dj4th-hardlock-runtime.md) | 4th Hardlock config, device/IOCTL sequence and validation boundary | Vendor driver framing confirmed; valid response and transform unresolved |

*Update this table in the same task whenever an analysis document is added or renamed.*
