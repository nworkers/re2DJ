# Windows x86 빌드 BAT 진입점 작업 지시

## 한국어

관련 설계: [Windows x86 빌드 BAT 진입점 설계](../design/20260901-115-windows-build-bat.md)

### 목표

기존 PowerShell 빌드 진입점을 재사용하고, 실패 종료 코드만 보존하도록 최소 보완한 뒤 `scripts/build_win32.bat`로 감싸 Windows 사용자가 동일한 preset 빌드를 쉽게 실행하도록 한다.

### 작업 범위

1. `%~dp0` 기준으로 `scripts/build.ps1`을 호출하는 BAT 추가.
2. `build.ps1`이 configure/build의 `$LASTEXITCODE`를 반환하도록 최소 보완.
3. 인자 전달과 종료 코드 보존 검증.
4. `scripts/README.md`에 BAT 사용법 추가.
5. 작업 로그에 검증 결과 기록.

### 제외 범위

Visual Studio 설치 탐색, CMake preset 변경, SDL3 의존성 관리, 빌드 산출물 커밋은 포함하지 않는다.

## English

Related design: [Windows x86 build BAT entry-point design](../design/20260901-115-windows-build-bat.md)

### Goal

Reuse the existing PowerShell build entry point, minimally preserve configure/build failure codes, and wrap it in `scripts/build_win32.bat` so Windows users can run the same preset build conveniently.

### Scope

Add the BAT, make `build.ps1` return configure/build failure codes, pass arguments and exit codes through, document its usage in `scripts/README.md`, and record verification in a work log.

### Out of scope

Do not change Visual Studio discovery, CMake presets, SDL3 dependency management, or commit build artifacts.
