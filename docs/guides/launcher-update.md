# 런처 업데이트와 스팀덱 설치

설계: [#17](../design/20261010-i017-launcher-self-update.md) · 작업 로그: [#17](../work-logs/20261010-i017-launcher-self-update.md) · 런처: [#12](../design/20261010-i012-launcher.md). 이 문서는 **반복 가능한 절차**만 둡니다.

## 1. 무엇을 하는가

런처(인자 없이 실행한 `re2dj`)가 열리면 GitHub의 최신 릴리스를 확인합니다. 더 새 버전이 있으면 런처 맨 위에 "re2DJ v<버전> is available."과 **Update** 버튼이 나옵니다. 누르면 이 빌드에 맞는 아카이브를 받아 GitHub가 알려 준 SHA-256과 비교하고, 같을 때만 풀어 **아카이브에 든 파일만** 바꾼 뒤 런처를 다시 시작합니다. `roms/`, `cfg/`, `overlays/`, `logs/`는 손대지 않습니다.

| 빌드 | 받는 파일 |
|---|---|
| Windows x86 | `re2DJ-v<버전>-windows-x86.zip` |
| Linux x64 (스팀덱) | `re2DJ-v<버전>-linux-x64.tar.gz` |
| Linux x86 | `re2DJ-v<버전>-linux-x86.tar.gz` |

v0.0.70까지의 릴리스는 이름이 `re2dj-v…`(소문자)이며, 대소문자를 무시하고 같은 파일로 찾습니다. 압축 안의 실행 파일은 늘 `re2dj`(`re2dj.exe`)입니다.

Update 버튼은 **릴리스 아카이브를 푼 폴더**(실행 파일 옆 `VERSION`이 실행 중인 버전과 같음)에서만 나옵니다. 빌드 트리에서는 알림 줄과 릴리스 페이지 주소만 보입니다. Linux는 시스템 `curl`로 받습니다(스팀덱과 일반 데스크톱에 있음). Windows는 WinHTTP를 씁니다.

## 2. 스팀덱에 처음 설치하기

1. 데스크톱 모드에서 릴리스 페이지의 `re2DJ-v<버전>-linux-x64.tar.gz`를 받아 풉니다. 예:
   ```bash
   mkdir -p ~/re2DJ && cd ~/re2DJ
   tar -xzf ~/Downloads/re2DJ-v*-linux-x64.tar.gz --strip-components=1
   ```
2. 원본 HDD 이미지는 `~/re2DJ/roms/<프로필>/`에 둡니다(README의 원본 자산 준비와 같음). 설정은 `~/re2DJ/cfg/`에 생깁니다.
3. Steam에서 "비 스팀 게임 추가"로 `~/re2DJ/re2dj`를 등록하고, 속성의 **시작 위치를 `~/re2DJ`**로 둡니다. `roms/`와 `cfg/`는 시작 위치 기준으로 찾습니다.
4. 게임 모드에서 실행하면 런처가 열립니다. 게임패드로 프로필을 고르고 `A`로 시작합니다.

이후로는 런처에 알림이 뜨면 Update를 누르기만 하면 됩니다(D-pad로 버튼까지 올라가 `A`). Linux에서는 같은 프로세스가 새 실행 파일로 바뀌므로 Steam은 게임이 계속 실행 중인 것으로 봅니다.

게임을 끝내려면 `L2`(LT)+`R2`(RT)+왼쪽 스틱 클릭+오른쪽 스틱 클릭을 1초 동안 함께 누르고 있습니다. 게임이 끝나 런처로 돌아오고, 런처에서 같은 조합을 누르면 re2DJ가 끝납니다(#20).

## 3. 끄기

| 방법 | 범위 |
|---|---|
| `cfg/re2dj.ini`의 `[Launcher]`에 `check_updates = 0` | 계속 |
| `RE2DJ_UPDATE_CHECK=0` | 그 실행(파일보다 우선) |

인자 실행(`re2dj ez2dj6th`)은 런처를 거치지 않으므로 확인하지 않습니다.

## 4. 문제가 생기면

로그(콘솔 또는 `logs/re2dj-*.log`)의 `update:` 줄이 단계와 이유를 말합니다.

| 로그 | 뜻 / 조치 |
|---|---|
| `check failed: cannot run curl` | Linux에 `curl`이 없음. 배포판 패키지로 설치 |
| `check failed: curl exit 6/7/28` | 네트워크 없음·연결 실패·시간 초과. 다음 실행 때 다시 확인 |
| `not a release install (...)` | 빌드 트리이거나 `VERSION`이 다름. 릴리스 아카이브로 설치 |
| `the install folder cannot be written` | 폴더 권한. 쓸 수 있는 곳에 설치 |
| `does not match the release's SHA-256` | 받은 파일이 손상됨. Retry |
| `install failed: ...` | 교체 중 실패. 기존 파일로 되돌린 상태이며 Retry 가능 |

교체 직후 옛 파일은 `<이름>.re2dj-old`로 남았다가 다음 시작 때 지워집니다(목록: `.re2dj-old-files`). 되돌리고 싶으면 다음 시작 전에 `.re2dj-old`를 원래 이름으로 바꾸면 됩니다. 받는 중 임시 파일은 설치 폴더의 `.re2dj-update/`에 있고 다음 업데이트 때 지워집니다.

## 5. 업데이트 경로 시험하기

새 릴리스를 만들지 않고 실제 GitHub 릴리스로 시험합니다. 저장소 루트에서 실행해 `roms/`와 `cfg/`를 그대로 씁니다.

```bash
t=build/update-test; rm -rf "$t"; mkdir -p "$t"
cp build/linux-x64-release/bin/re2dj "$t/"          # 시험할 빌드
printf '0.0.1\n' > "$t/VERSION"                      # 옛 버전인 척
RE2DJ_UPDATE_CURRENT_VERSION=0.0.1 "$t/re2dj"        # 런처
```

런처에 최신 릴리스 알림이 뜨고 Update로 `$t`의 파일이 바뀌며, 다시 열린 런처는 그 릴리스입니다. `RE2DJ_UPDATE_CURRENT_VERSION`은 다시 시작할 때 지워집니다. 네트워크 없는 확인은 단위 테스트의 `RunLauncherUpdateTests`입니다.

---

# Launcher updates and a Steam Deck install

Design: [#17](../design/20261010-i017-launcher-self-update.md) · work log: [#17](../work-logs/20261010-i017-launcher-self-update.md) · launcher: [#12](../design/20261010-i012-launcher.md). This document holds **repeatable procedures** only.

## 1. What it does

When the launcher (`re2dj` run without arguments) opens, it checks GitHub's latest release. For a newer version the top of the launcher shows "re2DJ v<version> is available." with an **Update** button, which downloads this build's archive, compares it with the SHA-256 GitHub reports, and only then unpacks it, replaces **only the files in the archive**, and restarts the launcher. `roms/`, `cfg/`, `overlays/` and `logs/` are left alone.

| Build | Downloads |
|---|---|
| Windows x86 | `re2DJ-v<version>-windows-x86.zip` |
| Linux x64 (Steam Deck) | `re2DJ-v<version>-linux-x64.tar.gz` |
| Linux x86 | `re2DJ-v<version>-linux-x86.tar.gz` |

Releases up to v0.0.70 are named `re2dj-v…` (lower case) and are found as the same files, ignoring case. The executable inside is always `re2dj` (`re2dj.exe`).

The Update button appears only in **a folder a release archive was unpacked into** (a `VERSION` next to the executable naming the running version); a build tree shows only the notice and the releases page. Linux downloads with the system `curl` (present on a Steam Deck and common desktops), Windows with WinHTTP.

## 2. A first install on a Steam Deck

1. In desktop mode, download `re2DJ-v<version>-linux-x64.tar.gz` from the releases page and unpack it, for example:
   ```bash
   mkdir -p ~/re2DJ && cd ~/re2DJ
   tar -xzf ~/Downloads/re2DJ-v*-linux-x64.tar.gz --strip-components=1
   ```
2. Put the original HDD images under `~/re2DJ/roms/<profile>/` (as the README's asset section says); settings appear in `~/re2DJ/cfg/`.
3. In Steam, "Add a Non-Steam Game" with `~/re2DJ/re2dj`, and set its **Start In to `~/re2DJ`**: `roms/` and `cfg/` are found from there.
4. Run it in game mode and the launcher opens; pick a profile with the pad and start it with `A`.

From then on, press Update when the launcher shows the notice (up to the button with the d-pad, then `A`). On Linux the same process becomes the new executable, so Steam sees the game as still running.

To end a game, hold `L2` (LT) + `R2` (RT) + left stick click + right stick click together for a second: the game ends back to the launcher, and the same chord in the launcher ends re2DJ (#20).

## 3. Turning it off

| How | Scope |
|---|---|
| `check_updates = 0` under `[Launcher]` in `cfg/re2dj.ini` | lasting |
| `RE2DJ_UPDATE_CHECK=0` | that run (wins over the file) |

Runs with arguments (`re2dj ez2dj6th`) never open the launcher and never check.

## 4. When something goes wrong

The log's `update:` lines (the console or `logs/re2dj-*.log`) give the step and the reason: `check failed: cannot run curl` (install `curl` from the distribution), `check failed: curl exit 6/7/28` (no network, no connection, a timeout; checked again next run), `not a release install (...)` (a build tree or a different `VERSION`; install from a release archive), `the install folder cannot be written` (folder permissions), `does not match the release's SHA-256` (a damaged download; Retry), and `install failed: ...` (put back to the old files; Retry).

Right after a replacement the old files stay as `<name>.re2dj-old` and go at the next start (listed in `.re2dj-old-files`); to go back, rename them before that start. Temporary download files live in the install folder's `.re2dj-update/` and go with the next update.

## 5. Trying the update path

Against a real GitHub release without making a new one, run from the repository root so `roms/` and `cfg/` are used as they are:

```bash
t=build/update-test; rm -rf "$t"; mkdir -p "$t"
cp build/linux-x64-release/bin/re2dj "$t/"          # the build to try
printf '0.0.1\n' > "$t/VERSION"                      # pretend to be old
RE2DJ_UPDATE_CURRENT_VERSION=0.0.1 "$t/re2dj"        # the launcher
```

The launcher shows the latest release, Update replaces `$t`'s files, and the reopened launcher is that release. `RE2DJ_UPDATE_CURRENT_VERSION` is withdrawn at the restart. The offline check is the unit test `RunLauncherUpdateTests`.
