# 작업 376 작업 지시서 — 모든 버전 표시를 한 머리말로 / Task 376 work order — one banner for every version display

설계: [20260926-376-consistent-version-banner.md](../design/20260926-376-consistent-version-banner.md)

## 절차 / Steps

1. `VersionBanner()`를 추가하고, 창 제목·OSD·CLI·진단 도구 4개가 쓰게 한다.
   *Add `VersionBanner()` and use it for the window title, the OSD, the CLI, and the four diagnostic tools.*
2. VFS runtime probe의 제목 검사를 머리말 전체로 바꾼다. probe target에 `include/` 경로를 준다.
   *Check the whole banner in the VFS runtime probe's title test, and give the probe target the `include/` path.*
3. 단위 테스트, 문서.
   *Unit test and documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build·CTest를 통과하고, 각 출력이 같은 머리말로 시작한다.
  *Windows x86 and both Linux widths build and pass CTest, and every output starts with the same banner.*
