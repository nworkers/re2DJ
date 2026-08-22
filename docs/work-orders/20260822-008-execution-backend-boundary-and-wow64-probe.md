# 작업 지시: 실행 backend 경계와 WOW64 probe

## 목표

`ExecutionBackend` 공용 event/reply 인터페이스를 추가하고 Windows x64에서 별도 x86 helper의 네이티브 x86→HLE gate 호출 가능성을 검증합니다.

## Goal

Add the shared `ExecutionBackend` event/reply interface and validate native x86-to-HLE gate calls from a separate x86 helper under Windows x64.

## 범위

* platform-neutral backend interface header
* Win32 x86 전용 native helper probe
* CMake Win32 probe preset과 CTest 등록
* 아키텍처, README, 스크립트 문서 갱신
* Windows x64 기존 build/test 회귀 검증

## 제외 범위

* 원본 `ez2dj1.exe` 실행
* synthetic PE32 전체 mapping과 IAT binding
* 32/64비트 IPC 구현
* Linux native helper와 Web 실행 엔진 구현

## Verification

Run the Windows x64 warnings-as-errors build and tests, then configure, build, and run the Win32 native-helper probe under WOW64.
