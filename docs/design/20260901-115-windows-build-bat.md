# Windows x86 빌드 BAT 진입점 설계

## 한국어

### 목적

Windows 사용자가 저장소 루트에서 한 번의 명령으로 기존 `scripts/build.ps1` 기반 Win32 x86 빌드를 실행할 수 있도록 `scripts/build_win32.bat` 진입점을 추가한다.

### 설계 결정

1. BAT는 빌드 로직을 복제하지 않고 기존 `scripts/build.ps1`을 호출한다. CMake preset, 기본 preset 이름, 구성 이름의 단일 출처를 유지한다. PowerShell 스크립트에는 configure/build 실패 코드를 반환하는 최소 검사만 둔다.
2. BAT의 경로는 `%~dp0`로 계산하여 현재 작업 디렉터리가 저장소 루트가 아니어도 스크립트를 찾는다.
3. 전달된 인자는 PowerShell 스크립트에 그대로 전달한다. 따라서 `-Preset`과 `-Configuration`을 기존 방식대로 재정의할 수 있다.
4. PowerShell의 종료 코드를 BAT의 종료 코드로 보존하여 CI와 사용자가 빌드 실패를 감지할 수 있게 한다.
5. CMake, Visual Studio, SDL3 의존성 탐색과 네트워크 정책은 기존 PowerShell/CMake 계층의 책임으로 남긴다.

### 실행 흐름

```mermaid
flowchart LR
    U[사용자] --> B[scripts/build_win32.bat]
    B --> P[scripts/build.ps1]
    P --> C[cmake --preset windows-x86-debug]
    C --> G[cmake --build --preset windows-x86-debug --config Debug]
    G --> O[build/windows-x86/bin/Debug]
```

## English

### Purpose

Add `scripts/build_win32.bat` so a Windows user can start the existing Win32 x86 build with one command from the repository root.

### Design decisions

1. The BAT delegates to `scripts/build.ps1` instead of duplicating build logic. The CMake preset, default preset name, and configuration remain single-sourced; the PowerShell script only adds minimal configure/build failure-code checks.
2. Resolve the script through `%~dp0`, so the entry point works even when invoked outside the repository root.
3. Pass all supplied arguments through to PowerShell, preserving the existing `-Preset` and `-Configuration` overrides.
4. Preserve PowerShell's exit code as the BAT exit code so CI and users can detect failures.
5. Leave CMake, Visual Studio, SDL3 dependency discovery, and network policy to the existing PowerShell/CMake layers.

### Execution flow

The BAT calls `scripts/build.ps1`, which configures and builds the `windows-x86-debug` preset and places outputs under `build/windows-x86/bin/Debug`.
