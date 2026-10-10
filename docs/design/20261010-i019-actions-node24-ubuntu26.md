# #19 설계: GitHub Actions의 Node 24 액션과 Ubuntu 26.04 runner

이슈: [#19](https://github.com/reexec/re2DJ/issues/19) · 참고: rePIU #50(Node 24 액션)

## 배경

* CI(`ci.yml`)와 릴리스(`release.yml`)의 작업마다 Node.js 20 지원 종료 경고가 납니다. `actions/checkout@v4`, `actions/cache@v4`, `actions/upload-artifact@v4`, `actions/download-artifact@v4`가 Node 20용이라 Node 24로 강제 실행되고 있습니다. 사이트(`pages.yml`)는 이미 Node 24 버전(`checkout@v7` 등)을 씁니다.
* `ubuntu-latest`가 2026-10-19부터 Ubuntu 26.04로 바뀐다는 안내가 납니다(runner-images #14748).

## 확인한 사실

* 각 액션의 최신 주 버전은 `runs.using: node24`입니다: checkout v7.0.1, cache v6.1.0, upload-artifact v7.0.2, download-artifact v8.0.2. rePIU #50도 같은 버전을 씁니다.
* 릴리스 노트를 보면 v4 이후의 깨지는 변경은 이 저장소의 사용 방식에 영향이 없습니다. Node 24 실행(runner 2.327.1 이상, GitHub 호스팅 runner는 충족), ESM 전환, checkout의 인증 정보 저장 위치, upload-artifact의 압축하지 않는 단일 파일 업로드(선택 사항), download-artifact의 ID 지정 다운로드 경로와 해시 불일치 시 오류(기본)가 바뀌었습니다. 이 저장소는 이름 패턴과 `merge-multiple: true`로 받으므로 경로가 그대로입니다.
* **스팀덱용 Linux 릴리스 바이너리는 runner의 Ubuntu에서 빌드하지 않습니다.** `release.yml`의 Linux 작업은 `debian:bookworm`(x86은 `i386/debian:bookworm`) 컨테이너 안에서 빌드·시험·패키징하고, `package_release.sh`는 glibc 2.36(`RE2DJ_MAX_GLIBC`)보다 새 심볼이 있으면 패키징을 거부합니다. runner의 Ubuntu 버전은 Docker를 띄우는 바깥 환경일 뿐이라 바이너리의 glibc 요구는 바뀌지 않습니다.
* runner에서 직접 빌드하는 것은 CI의 `linux-x64`(gcc·clang)뿐이고 그 결과물은 배포하지 않습니다. Ubuntu 26.04(개발 PC와 같음)에는 CI가 설치하는 apt 패키지 21개가 모두 있고, GCC 15.2와 clang 21.1.8로 경고를 오류로 한 빌드는 그동안 이 PC에서 검증해 왔습니다(작업 459·460과 이후 작업).

## 결정

1. `ci.yml`·`release.yml`의 액션을 checkout v7, cache v6, upload-artifact v7, download-artifact v8로 올립니다.
2. Linux runner를 `ubuntu-latest`·`ubuntu-24.04`에서 **`ubuntu-26.04`로 고정**합니다(`ci.yml`, `release.yml`, `pages.yml`). 이전 날짜에 끌려가지 않고 지금 검증한 상태로 두며, 다음 LTS로 옮길 때도 명시적으로 바꿉니다. Windows runner는 그대로입니다.
3. 릴리스 빌드 컨테이너(Debian 12)와 glibc 상한 2.36은 바꾸지 않습니다. 스팀덱 호환은 이 둘이 결정합니다.

## 검증

* 브랜치 push로 CI 네 작업을 `ubuntu-26.04`와 새 액션에서 돌리고, 경고가 사라졌는지 annotation으로 확인합니다.
* `release.yml`을 브랜치에서 수동 실행해(태그가 없어 게시하지 않음) 세 패키지가 만들어지고, Linux 패키지의 `GLIBC_` 상한 검사가 통과하는지 로그로 확인합니다. 받은 아티팩트의 이름(`re2DJ-v…`)과 실행 파일의 glibc 요구도 확인합니다.
* PR에서 사이트 빌드가 `ubuntu-26.04`로 통과하는지 확인합니다.

---

# #19 Design: Node 24 actions and Ubuntu 26.04 runners in GitHub Actions

Issue: [#19](https://github.com/reexec/re2DJ/issues/19) · Reference: rePIU #50 (Node 24 actions)

## Background

* Every CI (`ci.yml`) and release (`release.yml`) job warns that Node.js 20 is deprecated: `actions/checkout@v4`, `actions/cache@v4`, `actions/upload-artifact@v4` and `actions/download-artifact@v4` target Node 20 and are forced onto Node 24. The site (`pages.yml`) already uses Node 24 versions (`checkout@v7` and so on).
* A notice says `ubuntu-latest` becomes Ubuntu 26.04 from 2026-10-19 (runner-images #14748).

## Facts checked

* Each action's latest major runs `node24`: checkout v7.0.1, cache v6.1.0, upload-artifact v7.0.2, download-artifact v8.0.2; rePIU #50 uses the same.
* Their release notes since v4 hold no breaking change for this repository's use: Node 24 (runner 2.327.1 or later, which GitHub-hosted runners meet), ESM, where checkout keeps its credentials, upload-artifact's optional unzipped single-file upload, and download-artifact's path for downloads by ID and erroring on a hash mismatch by default. This repository downloads by name pattern with `merge-multiple: true`, so its paths stay.
* **The Linux release binaries for the Steam Deck are not built on the runner's Ubuntu.** `release.yml`'s Linux jobs build, test and package inside `debian:bookworm` (`i386/debian:bookworm` for x86), and `package_release.sh` refuses a binary with a glibc symbol newer than 2.36 (`RE2DJ_MAX_GLIBC`). The runner's Ubuntu only hosts Docker, so it leaves the binaries' glibc requirement unchanged.
* Only CI's `linux-x64` jobs (gcc and clang) build on the runner itself, and nothing they make is shipped. Ubuntu 26.04 (the development PC's own) carries all 21 apt packages CI installs, and builds with GCC 15.2 and clang 21.1.8 and warnings as errors have been verified on that PC all along (tasks 459, 460 and since).

## Decisions

1. Move `ci.yml` and `release.yml` to checkout v7, cache v6, upload-artifact v7 and download-artifact v8.
2. **Pin** the Linux runners to **`ubuntu-26.04`** in place of `ubuntu-latest` and `ubuntu-24.04` (`ci.yml`, `release.yml`, `pages.yml`), so the state verified now is kept rather than moved on someone else's date, and the next LTS is an explicit change too. Windows runners stay.
3. The release build container (Debian 12) and the glibc limit of 2.36 stay; they are what decide Steam Deck compatibility.

## Verification

* A branch push runs CI's four jobs on `ubuntu-26.04` with the new actions; the annotations show whether the warnings are gone.
* A manual run of `release.yml` on the branch (no tag, so nothing is published) makes the three packages, with the logs showing the Linux packages' `GLIBC_` limit check passing; the downloaded artifacts' names (`re2DJ-v…`) and the executables' glibc requirement are checked too.
* The PR shows the site build passing on `ubuntu-26.04`.
