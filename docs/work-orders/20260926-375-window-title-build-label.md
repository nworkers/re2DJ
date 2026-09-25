# 작업 375 작업 지시서 — 창 제목의 빌드 표시 / Task 375 work order — build label in the window title

설계: [20260926-375-window-title-build-label.md](../design/20260926-375-window-title-build-label.md)

## 절차 / Steps

1. CMake에 `RE2DJ_BUILD_CONFIG="$<CONFIG>"`를 전역 정의로 추가한다.
   *Add `RE2DJ_BUILD_CONFIG="$<CONFIG>"` as a global CMake definition.*
2. `re2dj/version.h`에 OS·아키텍처 macro와 `BuildLabel()`을 둔다.
   *Put the OS and architecture macros and `BuildLabel()` in `re2dj/version.h`.*
3. Windows `window_mode.cpp`의 제목 형식에 넣고, VFS runtime probe의 제목 검사에 더한다.
   *Use it in Windows `window_mode.cpp`'s title format and in the VFS runtime probe's title check.*
4. 단위 테스트, 문서.
   *Unit test and documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭의 build·CTest가 통과하고, probe가 실제 창 제목에서 `(Win/x86 Debug)`를 확인한다.
  *Windows x86 and both Linux widths build and pass CTest, with the probe finding `(Win/x86 Debug)` in the real window title.*
