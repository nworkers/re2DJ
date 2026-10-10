# #19 작업 지시서 — Node 24 액션과 Ubuntu 26.04 runner / #19 work order — Node 24 actions and Ubuntu 26.04 runners

이슈: [#19](https://github.com/reexec/re2DJ/issues/19) · 설계: [20261010-i019-actions-node24-ubuntu26.md](../design/20261010-i019-actions-node24-ubuntu26.md)

## 절차 / Steps

1. `ci.yml`·`release.yml`: checkout v7, cache v6, upload-artifact v7, download-artifact v8.
   *`ci.yml` and `release.yml`: checkout v7, cache v6, upload-artifact v7, download-artifact v8.*
2. Linux runner를 `ubuntu-26.04`로 고정(`ci.yml`, `release.yml`, `pages.yml`).
   *Pin the Linux runners to `ubuntu-26.04` (`ci.yml`, `release.yml`, `pages.yml`).*
3. 검증: 브랜치 CI와 annotation, `release.yml` 수동 실행과 아티팩트 확인, PR의 사이트 빌드.
   *Verification: the branch CI and its annotations, a manual `release.yml` run with its artifacts checked, and the site build on the PR.*

## 완료 조건 / Done when

CI·릴리스·사이트가 `ubuntu-26.04`와 Node 24 액션으로 통과하고 Node.js 20 경고가 없으며, Linux 릴리스 패키지가 glibc 2.36 상한을 지킨다.

*CI, release and site pass on `ubuntu-26.04` with Node 24 actions and no Node.js 20 warning, and the Linux release packages keep the glibc 2.36 limit.*
