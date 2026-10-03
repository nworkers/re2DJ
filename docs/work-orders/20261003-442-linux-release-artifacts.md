# 작업 442 작업 지시서 — Linux 릴리스 산출물 / Task 442 work order — Linux release artifacts

설계: [20261003-442-linux-release-artifacts.md](../design/20261003-442-linux-release-artifacts.md)

## 절차 / Steps

1. Linux release preset에 C++ 런타임 정적 링크를 더한다.
   *Add the static C++ runtime to the Linux release presets.*
2. `scripts/package_release.sh`(폭, NEEDED, GLIBC 검사 포함). Windows 패키지에 고지 문서를 더한다.
   *`scripts/package_release.sh`, with the width, NEEDED and GLIBC checks; add the notices to the Windows package.*
3. `release.yml`을 version, windows-x86, linux matrix, publish job으로 나눈다.
   *Split `release.yml` into version, windows-x86, a linux matrix and publish jobs.*
4. `ci.yml`: linux-x64 오디오 헤더, linux-x86 job.
   *`ci.yml`: audio headers for linux-x64 and a linux-x86 job.*
5. 사이트: 플랫폼별 산출물 분류, 다운로드 페이지, 문구.
   *The site: per-platform asset classification, the download page and its text.*
6. 문서: `scripts/README.md`, README 다운로드 안내, `ARCHITECTURE.md`, 사이트 README, 작업 로그.
   *Documents: `scripts/README.md`, the README's download notes, `ARCHITECTURE.md`, the site README and the work log.*
7. 로컬 검증(설계의 검증 절).
   *Local verification as in the design.*

## 완료 조건 / Done when

- 로컬 x64 Release 빌드·CTest·패키징·사이트 빌드가 통과한다.
  *The local x64 Release build, CTest, packaging and site build pass.*
- workflow 실행과 i386 컨테이너 빌드는 할 수 없는 이유와 함께 사용자 확인으로 남긴다.
  *The workflow run and the i386 container build are left to the user, with the reason recorded.*
