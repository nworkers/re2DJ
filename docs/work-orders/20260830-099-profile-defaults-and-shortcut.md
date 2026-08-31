# 타깃 프로파일 실행 기본값과 shortcut 작업 지시

## 목적

`ez2dj3rd`를 단순 감지 결과가 아니라 실행 가능한 built-in profile로 확장한다. 프로파일에 기본 실행 설정을 넣고 명령행 override를 적용하며, `re2dj ez2dj3rd`만으로 `roms/ez2dj3rd`의 원본 실행 파일을 실행한다.

*Extend `ez2dj3rd` from a detection-only result into an executable built-in profile. Store baseline execution settings in the profile, apply command-line overrides, and make `re2dj ez2dj3rd` run the original executable from `roms/ez2dj3rd`.*

## 작업 범위

1. `TargetProfile`에 profile-derived run defaults와 shortcut용 HDD 경로를 추가한다.
2. 1st SE의 기존 Windows product policy를 profile defaults로 옮겨 회귀를 막고, 3rd profile에 확인된 기본값을 추가한다.
3. CLI가 positional profile ID를 받고, profile의 기본 HDD 경로를 선택하며, positional invocation에서 자동 실행한다.
4. `--hdd`, `--target`, `--fullscreen`, `--windowed`, `--audio-gain-db`, `--demo-volume`, `--io-config`의 override 우선순위를 구현한다.
5. Windows original-process backend와 shared launcher가 profile별 정책을 사용하게 하고, 3rd에 불필요한 1st 전용 I/O·진단 제약을 적용하지 않는다.
6. synthetic tests, Windows build/CTest, 실제 3rd shortcut 실행을 검증한다.
7. README, ARCHITECTURE, Windows 실행 가이드, 관련 analysis, 설계·작업 로그를 갱신하고 커밋한다.

*Scope: add profile-derived run defaults and shortcut HDD path; move the existing 1st SE Windows product policy into profile defaults and add confirmed 3rd defaults; accept positional profile IDs and auto-run; implement override precedence; make the backend and shared launcher consume profile policy without imposing 1st-only I/O/diagnostic constraints on 3rd; verify synthetic tests, build/CTest, and the real shortcut; update the relevant documentation and commit.*

## 기본값과 override

- `ez2dj1stse`: 기존 canonical HLE/detached policy, `roms/ez2dj1stse`, windowed, audio gain 0 dB, demo profile 3, confirmed LPTDI target state.
- `ez2dj3rd`: `roms/ez2dj3rd`, 정적 import로 확인된 DirectSound ordinal `#1`·파일 I/O 기반 VFS·detached policy. `EZ2DJ.INI`의 `FullScreen=1`은 기록하되, 현재 launcher가 제공하는 `DirectDrawCreate`/display hook과 3rd의 `DirectDrawCreateEx`가 다르므로 DirectDraw/display HLE를 기본 활성화하지 않는다. guest drive/directory와 LPTDI state는 추측하지 않는다.
- positional profile ID는 실행을 implied하지만 `--hdd`가 있으면 HDD 경로를 교체하고, 지원되는 fullscreen/audio/demo/I-O command-line options는 profile defaults를 교체한다. 3rd에서 확인되지 않은 hook에 해당하는 옵션은 거절한다.

*Defaults: 1st keeps the existing canonical HLE/detached policy, `roms/ez2dj1stse`, windowed mode, 0 dB gain, demo profile 3, and the confirmed LPTDI state; 3rd uses `roms/ez2dj3rd` with the applicable DirectSound ordinal, file-I/O VFS, and detached policy. Its `FullScreen=1` is recorded, but the current launcher hooks `DirectDrawCreate`/display APIs that do not appear in this 3rd IAT, so graphics/display HLE is not enabled by default. The guest path and LPTDI state remain unguessed; positional selection implies execution, while supported explicit HDD/audio options override profile defaults.*

## 제외 범위

- 게임 로직·보호 로직을 C++로 재구현
- 원본 HDD/실행 파일/게임 자산의 저장소 추가·수정
- 미확정 3rd guest drive, LPTDI response, DirectInput/AVI 동작을 확인된 사실로 선언
- 3rd 전용 HLE subsystem을 근거 없이 새로 구현

*Excluded: reimplementing gameplay or protection, adding or modifying original assets, presenting unresolved 3rd guest-drive/LPTDI/DirectInput/AVI behavior as confirmed, or inventing a 3rd-only HLE subsystem without evidence.*

## 완료 조건

- `re2dj ez2dj3rd`가 `roms/ez2dj3rd`를 열고 `ez2dj/EZ2DJ.EXE`를 선택해 runtime detachment까지 진입한다.
- profile defaults와 CLI overrides가 테스트로 고정된다.
- 1st SE 기존 canonical command가 회귀하지 않는다.
- Windows x86 Debug build와 CTest가 통과한다.
- 실제 3rd 실행 결과는 runtime detachment 후 검증을 위해 수동 종료했다는 점과 남은 미확정 항목이 작업 로그·analysis에 기록된다.

*Completion requires the shortcut to open `roms/ez2dj3rd`, select `ez2dj/EZ2DJ.EXE`, and reach runtime detachment; profile defaults and CLI overrides to be pinned by tests; the existing 1st SE canonical command to remain regression-free; Windows x86 Debug build and CTest success; and the real 3rd result plus unresolved items to be recorded in the work log and analysis.*
