# 작업 325: 단일 프로세스 x86 실행 backend 설계 / Task 325: Single-process x86 execution-backend design

## 목표 / Goal

Linux x86/x64와 Windows x64에 대한 단일 프로세스 원본 x86 실행 방향을 확정하고, Windows의 지원되는 WoW64 경계와 Linux x64 compatibility-mode boundary를 구분한다.

*Establish the single-process original-x86 execution direction for Linux x86/x64 and Windows x64, distinguishing the supported Windows WoW64 boundary from the Linux x64 compatibility-mode boundary.*

## 완료 기준 / Completion criteria

- Wine 코드와 runtime을 사용하지 않는다는 점을 명시한다.
- Windows x64에서는 64비트 process에 32비트 runtime을 직접 load하는 방식을 채택하지 않는다.
- Linux x86, Linux x64, Windows x64의 backend 형태와 도입 순서를 문서화한다.

*Explicitly exclude Wine code and runtime. Do not adopt direct loading of 32-bit runtime into a Windows x64 process. Document backend form and adoption order for Linux x86, Linux x64, and Windows x64.*
