# #17 설계: 런처 자동 업데이트와 산출물 이름 `re2DJ-v*`

이슈: [#17](https://github.com/reexec/re2DJ/issues/17) · 참고: rePIU v0.0.215(#48, `docs/design/20261010-i048-launcher-self-update.md`) · 선행: [#12 런처](20261010-i012-launcher.md)

## 목표

1. 스팀덱처럼 릴리스를 손으로 받아 풀기 번거로운 환경에서, 런처가 새 버전을 알리고 버튼 하나로 설치한 뒤 다시 시작합니다(Linux·Windows).
2. 릴리스 산출물 이름을 `re2DJ-v<ver>-<platform>.{tar.gz,zip}`로 바꿉니다. 압축 안의 실행 파일 이름은 `re2dj`/`re2dj.exe` 그대로입니다.

## 확인한 사실

* `reexec/re2DJ`는 공개 저장소라 `GET https://api.github.com/repos/reexec/re2DJ/releases/latest`를 토큰 없이 부를 수 있습니다. v0.0.70 응답의 `assets[]`마다 `name`, `size`, `browser_download_url`, `digest`(`sha256:<hex>`)가 있습니다.
* 산출물: Linux는 `…-linux-x64.tar.gz`, `…-linux-x86.tar.gz`이고 최상위 폴더(`<이름>/`) 아래에 `re2dj`(755), `VERSION`, 문서, `config/`가 있습니다. Windows는 `…-windows-x86.zip`이고 루트에 `re2dj.exe`, `VERSION`, 문서, `config/`가 있습니다(`package_release.{sh,ps1}`).
* `roms/`, `cfg/`, `overlays/`, `logs/`는 현재 디렉터리 기준이고 아카이브에 없습니다. 아카이브의 파일만 바꾸면 사용자 데이터는 남습니다.
* libchdr에 들어 있는 miniz 3.1.2가 이미 링크돼 있지만 `MINIZ_NO_ARCHIVE_APIS`로 ZIP 읽기가 꺼져 있습니다. inflate(`tinfl`)는 켜져 있습니다.
* 빌드 라벨은 `RE2DJ_HOST_OS_LABEL`(`Win`/`Linux`)과 `RE2DJ_HOST_ARCH_LABEL`(`x86`/`x64`)입니다(`version.h`).
* Windows x86 제품은 시작할 때 자기 자신을 다시 띄워 게스트 주소 범위를 예약하고(바깥 프로세스는 기다림), 런처는 그 안쪽 프로세스에서 돕니다.

## 결정

rePIU의 구조와 동작을 그대로 옮기고, 다음만 re2DJ에 맞춥니다.

### 1. 구성

| 구성 | 위치 | 비고 |
|---|---|---|
| `SemanticVersion` | `re2dj/update/semantic_version` | 빌드 버전은 `re2dj::VersionString()` |
| `ReleaseInfo` | `re2dj/update/release_info` | 작은 JSON 파서, 이 빌드용 자산 고르기 |
| `Sha256` | `re2dj/update/sha256` | |
| `ReleaseArchive` | `re2dj/update/release_archive` | tar.gz(`tinfl`), zip(miniz 메모리 리더), 위험한 경로 거부, 최상위 폴더 하나 벗기기 |
| `UpdateInstall` | `re2dj/update/update_install` | 설치 조건, 교체·되돌림, `.re2dj-old` 정리 |
| `LauncherUpdater` | `re2dj/update/launcher_updater` | 백그라운드 스레드 상태 기계, UI용 스냅샷 |
| HTTPS 받기 | `re2dj/platform/https_download.h`; `linux/https_download.cpp`(시스템 `curl`을 `posix_spawnp`), `windows/https_download.cpp`(WinHTTP) | |
| 실행 파일 경로·다시 시작 | `re2dj/platform/self_process.h`에 `SelfExecutablePath`, `ReplaceSelfProcess`(Linux `execv`), `RunExecutableAndWait` | |

업데이트 계층은 공용 코어에 두고 OS 헤더를 쓰지 않습니다. Linux는 TLS 라이브러리를 링크하지 않습니다(릴리스의 NEEDED와 glibc 상한을 그대로 두기 위해). `curl`에는 셸 없이 인자 배열로 `--proto =https --proto-redir =https`를 넘깁니다.

### 2. 자산 이름

이 빌드가 받는 자산은 `re2DJ-v<ver>-windows-x86.zip`(Win/x86), `-linux-x64.tar.gz`(Linux/x64), `-linux-x86.tar.gz`(Linux/x86)입니다. 이름은 **대소문자를 무시하고** 찾습니다. 이 작업 전 릴리스(v0.0.70까지)의 `re2dj-v…` 이름도 같은 자산으로 보므로, 이 버전이 나오기 전에도 실제 릴리스로 전체 경로를 시험할 수 있습니다.

### 3. 흐름

rePIU와 같습니다. 런처 세션을 시작할 때 한 번 백그라운드에서 확인하고, 오프라인·오류는 로그 한 줄만 남깁니다. 더 높은 버전이고 이 빌드용 자산과 SHA-256 `digest`가 있으면 런처 맨 위에 "re2DJ v<ver> is available."과 **Update** 버튼(키보드·게임패드로도 닿음)을 보여 줍니다. 받는 동안 진행 막대를 그리고, 검증·풀기가 끝나면(staged) 런처 창이 닫히고 세션이 설치합니다.

* 받는 곳: 설치 폴더의 `.re2dj-update/`(같은 파일 시스템이라 교체가 rename 하나).
* 설치: 파일마다 기존 파일을 `<이름>.re2dj-old`로 옮긴 뒤 새 파일을 넣습니다. 하나라도 실패하면 모두 되돌리고 런처를 다시 엽니다. `.re2dj-old`는 다음 시작 때 지웁니다(목록 `.re2dj-old-files`).
* 다시 시작: Linux는 `execv`로 같은 PID를 유지합니다(Steam 게임 모드가 게임이 끝났다고 보지 않게). Windows는 새 실행 파일을 자식으로 실행해 기다린 뒤 그 종료 코드로 끝납니다. 시험용 `RE2DJ_UPDATE_CURRENT_VERSION`은 다시 시작하기 전에 지웁니다.

### 4. 설치할 수 있는 곳

실행 파일 옆 `VERSION`의 내용이 빌드 버전과 같을 때만 Update 버튼을 보입니다. 릴리스 아카이브는 늘 이 조건을 만족하고 빌드 트리(`build/<preset>/bin/`)에는 `VERSION`이 없으므로, 개발 트리는 덮어쓰지 않습니다. 설치 폴더에 쓸 수 없으면 받기 전에 알립니다.

### 5. 끄기와 시험

| 설정 | 뜻 |
|---|---|
| `cfg/re2dj.ini` `[Launcher] check_updates = 0` | 확인하지 않음(기본 1) |
| `RE2DJ_UPDATE_CHECK=0` | 그 실행에서 확인하지 않음(파일보다 우선) |
| `RE2DJ_UPDATE_CURRENT_VERSION=<버전>` | 시험용: 비교와 설치 조건에 빌드 버전 대신 이 값을 씀 |

인자 실행(`re2dj ez2dj6th`)은 런처를 거치지 않으므로 확인하지 않습니다.

### 6. 산출물 이름

`package_release.sh`·`package_release.ps1`의 이름과 tar 최상위 폴더, `release.yml`의 workflow artifact 이름을 `re2DJ-v<ver>-<platform>`로 바꿉니다. 실행 파일 이름과 사이트의 자산 분류(접미사 기준)는 그대로이고, 사이트 안내 문구의 예시 이름을 고칩니다.

## 바꾸지 않는 것

* 게임 실행 경로, 인자 실행, 릴리스 아카이브의 구성 파일, CI 대상.
* `roms/`, `cfg/`, `overlays/`, `logs/`와 아카이브에 없는 파일.

## 검증

* 단위 테스트(rePIU probe `launcher_update`를 옮김): 버전 비교, 저장한 릴리스 JSON 파싱과 자산 선택(세 빌드, 대소문자 무시), SHA-256 시험 벡터, 손으로 만든 tar.gz·zip 풀기, 위험한 경로 거부, 교체와 되돌림, 설치 조건, `.re2dj-old` 정리, 가짜 fetch로 상태 기계.
* 빌드: Linux x64·x86 Debug(경고를 오류로)·Release, clang. Windows는 CI.
* 실제(Linux): v0.0.70 릴리스를 대상으로, 시험 폴더에 새 빌드와 `VERSION 0.0.1`을 두고 `RE2DJ_UPDATE_CURRENT_VERSION=0.0.1`로 런처를 열어 알림 → Update → 교체 → 같은 PID로 다시 시작 → v0.0.70 런처가 열림을 확인합니다. 빌드 트리에서는 버튼 없이 알림만 나오는지 봅니다. 산출물 이름은 `package_release.sh`로 만든 tar의 이름과 내부 구성으로 확인합니다. Windows의 실제 교체는 사용자 확인으로 남깁니다.

---

# #17 Design: launcher self-update and `re2DJ-v*` artifact names

Issue: [#17](https://github.com/reexec/re2DJ/issues/17) · Reference: rePIU v0.0.215 (#48, `docs/design/20261010-i048-launcher-self-update.md`) · Builds on: [#12 launcher](20261010-i012-launcher.md)

## Goals

1. Where unpacking a release by hand is awkward, such as a Steam Deck, the launcher announces a new version, installs it with one button and restarts (Linux and Windows).
2. Release artifacts are named `re2DJ-v<ver>-<platform>.{tar.gz,zip}`; the executable inside stays `re2dj`/`re2dj.exe`.

## Facts checked

* `reexec/re2DJ` is public, so `GET https://api.github.com/repos/reexec/re2DJ/releases/latest` needs no token; in v0.0.70's response each of `assets[]` has `name`, `size`, `browser_download_url` and `digest` (`sha256:<hex>`).
* Artifacts: Linux `…-linux-x64.tar.gz` and `…-linux-x86.tar.gz`, with `re2dj` (755), `VERSION`, the documents and `config/` under one top folder (`<name>/`); Windows `…-windows-x86.zip`, with `re2dj.exe`, `VERSION`, the documents and `config/` at its root (`package_release.{sh,ps1}`).
* `roms/`, `cfg/`, `overlays/` and `logs/` are relative to the current directory and in no archive, so replacing the archive's files leaves the user's data alone.
* The miniz 3.1.2 inside libchdr is already linked, with ZIP reading switched off by `MINIZ_NO_ARCHIVE_APIS`; inflate (`tinfl`) is on.
* The build labels are `RE2DJ_HOST_OS_LABEL` (`Win`/`Linux`) and `RE2DJ_HOST_ARCH_LABEL` (`x86`/`x64`) (`version.h`).
* The Windows x86 product restarts itself at start to reserve the guest's address range (the outer process waits), and the launcher runs in that inner process.

## Decisions

rePIU's structure and behaviour carry over as they are, with these re2DJ fittings.

### 1. Parts

| Part | Where | Note |
|---|---|---|
| `SemanticVersion` | `re2dj/update/semantic_version` | the build's version is `re2dj::VersionString()` |
| `ReleaseInfo` | `re2dj/update/release_info` | a small JSON parser and picking this build's asset |
| `Sha256` | `re2dj/update/sha256` | |
| `ReleaseArchive` | `re2dj/update/release_archive` | tar.gz (`tinfl`), zip (miniz's memory reader), dangerous paths refused, one top folder stripped |
| `UpdateInstall` | `re2dj/update/update_install` | where it may install, replacing and rolling back, removing `.re2dj-old` |
| `LauncherUpdater` | `re2dj/update/launcher_updater` | a background-thread state machine and a snapshot for the UI |
| HTTPS fetch | `re2dj/platform/https_download.h`; `linux/https_download.cpp` (the system `curl` through `posix_spawnp`), `windows/https_download.cpp` (WinHTTP) | |
| Executable path and restart | `SelfExecutablePath`, `ReplaceSelfProcess` (Linux `execv`) and `RunExecutableAndWait` in `re2dj/platform/self_process.h` | |

The update layer sits in the shared core with no OS header. Linux links no TLS library, keeping the release's NEEDED list and glibc floor; `curl` gets `--proto =https --proto-redir =https` in an argument array, with no shell.

### 2. Asset names

This build installs from `re2DJ-v<ver>-windows-x86.zip` (Win/x86), `-linux-x64.tar.gz` (Linux/x64) or `-linux-x86.tar.gz` (Linux/x86), looked up **ignoring case**: releases before this task (up to v0.0.70), named `re2dj-v…`, count as the same assets, so the whole path can be tried against a real release before this version ships.

### 3. Flow

As in rePIU. The check runs once in the background when a launcher session starts, and being offline or an error leaves only a log line. For a higher version with this build's asset and a SHA-256 `digest`, the launcher's top shows "re2DJ v<ver> is available." with an **Update** button (reachable by keyboard and pad). A progress bar shows the download; once checked and unpacked (staged), the launcher window closes and the session installs.

* Downloads go to `.re2dj-update/` in the install folder (the same file system, so each replacement is one rename).
* Installing renames each existing file to `<name>.re2dj-old` and moves the new one in; any failure moves everything back and reopens the launcher. `.re2dj-old` files go at the next start (listed in `.re2dj-old-files`).
* Restarting: Linux `execv` keeps the process id (so Steam's game mode does not take the game as ended); Windows runs the new executable as a child, waits, and exits with its code. The test override `RE2DJ_UPDATE_CURRENT_VERSION` is withdrawn before the restart.

### 4. Where it may install

The Update button appears only when a `VERSION` next to the executable names the build's version. Release archives always meet this and a build tree (`build/<preset>/bin/`) has no `VERSION`, so a development tree is never overwritten. An install folder that cannot be written is reported before downloading.

### 5. Turning it off and testing

| Setting | Meaning |
|---|---|
| `cfg/re2dj.ini` `[Launcher] check_updates = 0` | no check (default 1) |
| `RE2DJ_UPDATE_CHECK=0` | no check for that run (wins over the file) |
| `RE2DJ_UPDATE_CURRENT_VERSION=<version>` | testing: used instead of the build's version for the comparison and the install condition |

Runs with arguments (`re2dj ez2dj6th`) never open the launcher and so never check.

### 6. Artifact names

`package_release.sh` and `package_release.ps1` name the archive and the tar's top folder `re2DJ-v<ver>-<platform>`, and `release.yml` its workflow artifacts the same. The executable's name and the site's asset classification (by suffix) stay; the site's example names change.

## Unchanged

* The game run path, runs with arguments, the files in a release archive, the CI targets.
* `roms/`, `cfg/`, `overlays/`, `logs/` and files in no archive.

## Verification

* Unit tests (rePIU's `launcher_update` probe carried over): version comparison, parsing a stored release JSON and picking assets (three builds, ignoring case), SHA-256 test vectors, unpacking hand-made tar.gz and zip, refusing dangerous paths, replacing and rolling back, the install condition, removing `.re2dj-old`, and the state machine over a fake fetch.
* Builds: Linux x64 and x86 Debug (warnings as errors), Release and clang; Windows by CI.
* Real (Linux): against the v0.0.70 release, a test folder holding the new build and `VERSION 0.0.1`, the launcher opened with `RE2DJ_UPDATE_CURRENT_VERSION=0.0.1`: notice, Update, replacement, restart in the same process, and the v0.0.70 launcher opening; in a build tree only the notice, no button. The artifact name by the tar `package_release.sh` makes and its contents. A real replacement on Windows is left to the user.
