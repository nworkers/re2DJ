# 작업 로그 347: 플랫폼 비트 폭 디렉터리 규칙 / Work log 347: platform bit-width directory policy

## 결과 / Result

`AGENTS.md`, `docs/CODING_STYLE.md`, `ARCHITECTURE.md`와 플랫폼 README에 OS와 host 비트 폭을 분리하는 규칙을 반영했습니다. `src/platform/windows/`·`linux/` 루트는 OS 전용이면서 x86/x64 중립 또는 공용인 코드에 사용하고, host 32비트 전용 구현은 `x86/`, host 64비트 전용 구현은 `x64/`에 둡니다. 비트 폭 전용 공개 구현 header도 `include/re2dj/platform/<os>/x86/`·`x64/`로 같은 구조를 따릅니다.

*Updated `AGENTS.md`, `docs/CODING_STYLE.md`, `ARCHITECTURE.md`, and the platform READMEs with a rule separating OS from host bit width. The `src/platform/windows/` and `linux/` roots are for OS-specific code neutral across or shared by x86 and x64; 32-bit-host-only implementations belong under `x86/`, and 64-bit-host-only implementations under `x64/`. Width-specific public implementation headers mirror the structure under `include/re2dj/platform/<os>/x86/` and `x64/`.*

게스트 PE32·32비트 주소 의미와 host native 비트 폭을 구분했습니다. 고정 폭 정수와 `GuestAddress`를 사용해 x86/x64 host에서 공통으로 빌드되는 코드는 상위 OS 디렉터리에 남고, host pointer width, native ABI·register·instruction 또는 한 비트 폭 전용 CMake 선택에 의존하는 코드만 하위로 분리합니다. 두 구현이 갈리면 공용 policy·state·interface는 상위에 유지합니다.

*Distinguished guest PE32/32-bit-address semantics from native host width. Code shared by x86 and x64 hosts through fixed-width integers and `GuestAddress` stays in the parent OS directory; only code dependent on host pointer width, native ABI, registers, instructions, or one-width-only CMake selection moves below it. When implementations diverge, shared policy, state, and interfaces remain in the parent.*

## 현재 구조 감사 / Current-layout audit

- Windows: 32비트 injected runtime, DirectDraw/Direct3D/DirectSound COM facade, 32비트 native helper 계열은 `windows/x86/` 이동 후보입니다.
- Linux: i386 helper의 PE mapping·bootstrap·import thunk와 native in-process 실행 계열은 `linux/x86/` 이동 후보입니다.
- Linux x86/x64 product host가 공유하는 `NativeHelperBackend`, `original_runner`와 고정 폭 IPC 계약은 상위 또는 공용 위치 후보입니다.
- 이번 작업에서는 파일을 이동하지 않았습니다. 기존 경로 이동은 CMake source list, include 경로, probe와 runtime DLL 계약을 함께 검증해야 하므로 `docs/TODO.md`에 후속 작업으로 남겼습니다.

*Audit: the Windows 32-bit injected runtime, DirectDraw/Direct3D/DirectSound COM facades, and 32-bit native-helper family are candidates for `windows/x86/`; the Linux i386 helper's PE mapping, bootstrap, import thunks, and native in-process execution are candidates for `linux/x86/`; `NativeHelperBackend`, `original_runner`, and fixed-width IPC contracts shared by Linux x86/x64 product hosts are candidates to remain in parent/shared locations. No files moved in this task because path migration must verify CMake source lists, include paths, probes, and runtime-DLL contracts together; that work is recorded in `docs/TODO.md`.*

## 검증 / Validation

- 변경 대상 문서에서 이전의 “Windows 디렉터리는 64비트 전용” 및 “32비트 helper도 루트에 둔다” 규칙이 제거됐습니다.
- 한국어와 영어 규칙이 `x86`=32비트 전용, `x64`=64비트 전용으로 일치합니다.
- `git diff --check`가 통과했습니다.
- 코드와 build graph는 변경하지 않았으므로 빌드 검증은 필요하지 않습니다.

*Validation confirmed removal of the prior “Windows directory is 64-bit-only” and “32-bit helper stays at the root” rules, matching Korean and English definitions of `x86` as 32-bit-only and `x64` as 64-bit-only, and a clean `git diff --check`. No code or build graph changed, so no build verification was required.*
