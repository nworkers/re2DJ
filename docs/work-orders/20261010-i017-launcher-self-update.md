# #17 작업 지시서 — 런처 자동 업데이트와 산출물 이름 re2DJ-v* / #17 work order — launcher self-update and re2DJ-v* artifact names

이슈: [#17](https://github.com/reexec/re2DJ/issues/17) · 설계: [20261010-i017-launcher-self-update.md](../design/20261010-i017-launcher-self-update.md)

## 절차 / Steps

1. rePIU `src/update/`·`include/repiu/update/`를 `re2dj/update/`로 옮긴다(이름 공간, 빌드 버전, 자산 이름, `.re2dj-*` 접미사, 대소문자 무시). miniz의 ZIP 읽기를 켠다.
   *Carry rePIU's `src/update/` and `include/repiu/update/` over to `re2dj/update/` (namespace, build version, asset names, `.re2dj-*` suffixes, case-insensitive lookup), and switch miniz's ZIP reading on.*
2. `platform/https_download.h`와 Linux(`curl`)·Windows(WinHTTP) 구현, `self_process.h`에 실행 파일 경로·`execv`·경로 지정 실행.
   *`platform/https_download.h` with Linux (`curl`) and Windows (WinHTTP) implementations, and the executable path, `execv` and running a given path in `self_process.h`.*
3. 런처 설정 `[Launcher] check_updates`, 화면의 알림 줄·Update 버튼·진행 막대, 세션의 업데이터 생성·설치·다시 시작, 남은 `.re2dj-old` 정리.
   *The launcher setting `[Launcher] check_updates`, the screen's notice, Update button and progress bar, and the session creating the updater, installing, restarting and removing leftover `.re2dj-old` files.*
4. 단위 테스트(rePIU probe `launcher_update`를 옮김).
   *Unit tests (rePIU's `launcher_update` probe carried over).*
5. 산출물 이름 `re2DJ-v*`: `package_release.{sh,ps1}`, `release.yml`, 사이트 문구, 주석.
   *Artifact names `re2DJ-v*`: `package_release.{sh,ps1}`, `release.yml`, the site text and comments.*
6. 문서: 가이드 `docs/guides/launcher-update.md`, README, ARCHITECTURE, IMPLEMENTED, THIRD_PARTY_NOTICES(miniz ZIP 사용).
   *Documents: the guide `docs/guides/launcher-update.md`, README, ARCHITECTURE, IMPLEMENTED and THIRD_PARTY_NOTICES (miniz's ZIP use).*
7. 검증: Linux 빌드·테스트, v0.0.70 릴리스로 실제 업데이트, 패키지 이름 확인, push 뒤 CI.
   *Verification: Linux builds and tests, a real update from the v0.0.70 release, the package name, and CI after the push.*

## 완료 조건 / Done when

릴리스 설치에서 런처가 새 버전을 알리고 Update로 교체·다시 시작하며, 빌드 트리는 교체하지 않고, 산출물 이름이 `re2DJ-v*`이며, 모든 타깃의 빌드와 테스트가 통과한다.

*In a release install the launcher announces a new version and Update replaces and restarts, a build tree is never replaced, artifacts are named `re2DJ-v*`, and every target builds and passes its tests.*
