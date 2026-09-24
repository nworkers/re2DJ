# 작업 347: 플랫폼 비트 폭 디렉터리 규칙 / Task 347: platform bit-width directory policy

## 목표 / Objective

`windows/`와 `linux/` 플랫폼 디렉터리의 루트에는 32비트·64비트에서 공통으로 컴파일되는 코드 또는 비트 폭과 무관한 조율 코드만 둡니다. 호스트 비트 폭에 종속된 구현은 `x86/` 또는 `x64/` 하위 디렉터리로 분리합니다.

*Keep only code shared by 32-bit and 64-bit builds, or orchestration independent of host bit width, at the roots of the `windows/` and `linux/` platform directories. Put host-bit-width-specific implementations in `x86/` or `x64/` subdirectories.*

## 분류 규칙 / Classification rules

| 위치 | 허용 코드 |
| --- | --- |
| `src/platform/<os>/` | 해당 OS 전용이지만 host pointer width, register ABI와 명령 집합에 중립인 코드 또는 x86/x64 공용 코드 |
| `src/platform/<os>/x86/` | 32비트 host process에서만 컴파일되는 코드, i386 ABI·register·instruction·pointer-width 가정이 있는 코드 |
| `src/platform/<os>/x64/` | 64비트 host process에서만 컴파일되는 코드, x86-64 ABI·register·instruction·pointer-width 가정이 있는 코드 |

*The platform root holds OS-specific but host-width-neutral or shared code. `x86/` holds code compiled only into 32-bit host processes or dependent on i386 ABI, registers, instructions, or pointer width. `x64/` holds the corresponding 64-bit-only code.*

공개 플랫폼 구현 헤더가 비트 폭 전용이면 `include/re2dj/platform/<os>/x86/` 또는 `x64/`에 같은 구조로 둡니다. 비트 폭 공용 인터페이스와 adapter 선언은 상위 `<os>/`에 둡니다.

*Mirror the same structure under `include/re2dj/platform/<os>/x86/` or `x64/` for public platform-implementation headers. Keep bit-width-neutral interfaces and adapter declarations in the parent `<os>/` directory.*

게스트가 32비트 주소나 PE32 구조를 다룬다는 사실만으로 host 구현을 `x86/`에 넣지는 않습니다. `GuestAddress`, 고정 폭 정수와 직렬화처럼 x86·x64 host에서 같은 코드가 빌드된다면 상위 디렉터리에 둡니다. 반대로 CMake가 한 비트 폭에서만 source를 선택하거나 native register/ABI에 의존하면 하위 디렉터리 대상입니다.

*Guest-facing 32-bit addresses or PE32 structures alone do not make host code x86-only. Code that builds identically on x86 and x64 hosts through `GuestAddress`, fixed-width integers, or serialization stays in the parent directory. Code selected by CMake for one host width or dependent on native registers or ABI belongs in the corresponding subdirectory.*

## 책임 분리 / Responsibility split

같은 기능에 x86/x64 구현이 모두 있으면 공용 interface·state·policy는 상위 디렉터리에 두고, 하위 디렉터리에는 architecture adapter와 native entry만 둡니다. 한 파일 안의 대규모 `#if`로 두 구현을 섞지 않습니다.

*When a feature has both x86 and x64 implementations, keep the shared interface, state, and policy in the parent directory, leaving architecture adapters and native entries in the subdirectories. Do not combine substantial implementations behind large `#if` blocks in one file.*

## 현재 적용 범위 / Current scope

이번 작업은 규칙과 디렉터리 책임을 문서화합니다. 현재 Windows x86 injected runtime과 Linux i386 helper 등 기존 비트 폭 전용 파일의 대량 이동은 CMake source list, include path와 런타임 산출물 계약을 함께 바꾸므로 별도 구조 정리 작업으로 수행합니다. 새 파일과 기존 파일을 실질적으로 분리·이동하는 작업부터 이 규칙을 적용합니다.

*This task documents the rule and directory responsibilities. Bulk movement of existing width-specific files such as the Windows x86 injected runtime and Linux i386 helper changes CMake source lists, include paths, and runtime artifact contracts, so it belongs to a separate structural task. Apply this rule to new files and whenever existing files are materially split or moved.*
