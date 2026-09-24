# 작업 344 작업 로그 — 게스트 모듈 resolver 합류 / Task 344 work log — Guest module resolver convergence

설계: [20260922-344-guest-module-resolver-convergence.md](../design/20260922-344-guest-module-resolver-convergence.md)
작업 지시: [20260922-344-guest-module-resolver-convergence.md](../work-orders/20260922-344-guest-module-resolver-convergence.md)
선행: [작업 343 kernel32 Linux i386 facade](20260922-343-kernel32-linux-i386-facade.md)
분석: [4th Linux in-process 첫 import](../analysis/ez2dj4th-linux-inprocess-first-import.md)

## 한국어

### 구현

| 계층 | 변경 |
| --- | --- |
| 공용 HLE | `ImportCall`에 선택적 `ImportCallServices` 경계. bounded guest 문자열 읽기와 module/export 조회만 노출 |
| 공용 HLE | `GuestModuleRegistry::FindExport(const ImportGate&)` 추가 |
| `kernel32` module | `GetModuleHandleA`, `GetProcAddress` handler 구현. ordinal 형식(상위 word 0) 포함 |
| Linux import thunk | `NativeImportSlotBinding` 보존과 `RebindNativeGuestModuleImports` |
| Linux runner | `RunConfiguredNativePeInProcess`와 `NativePeSessionSetup` 콜백 |
| 진단 | `native_create_file_observation.cpp`에서 pseudo handle과 API별 분기 제거, services 구현 |
| 배치 | 작업 343의 i386 전용 source·probe를 `src/platform/linux/x86/`으로 이동 |

### 설계에서 바꾼 것 — 재결합 주체

설계는 `NativePeSession`이 facade mapping과 재결합을 직접 하도록 썼다. 구현은 session 준비 직후 호출되는 `NativePeSessionSetup` 콜백을 두고 진단 쪽이 수행하도록 바꿨다. 근거와 대가는 [설계 문서](../design/20260922-344-guest-module-resolver-convergence.md)에 기록했다. 요지는 `NativePeSession`이 facade를 모르는 범용 session으로 남아야 하고, 어떤 module을 등록할지는 진단마다 다르다는 것이다.

### 구현 중 보강한 것 — identity를 관측으로 바꿈

초기 구현은 준비 단계에서 registry 값을 관측 필드에 미리 채웠다. 그 상태에서는 출력된 주소가 registry 값인지 게스트가 실제로 받은 값인지 구분되지 않아, 완료 조건의 "정적 IAT와 동적 resolver가 같은 thunk를 반환한다"가 **구조상 참인 명제**일 뿐 증거가 아니었다.

세 갈래를 분리했다.

* registry 값은 준비 단계에서 `registry_*`에 따로 보관한다.
* 게스트가 받은 값은 resolver service가 실제로 반환할 때만 기록하고 미리 채우지 않는다. 0은 "게스트가 묻지 않음"이며 불일치와 구분된다.
* 정적 IAT 값은 기록한 값을 신뢰하지 않고, image가 mapping된 상태에서 slot을 되읽어 얻는다.

`kCreateFileCallNotReached` 경계 판정은 이전 의미를 유지하기 위해 관측값이 아니라 `registry_create_file`로 옮겼다. facade가 준비되었는지를 묻는 조건이기 때문이다.

### 검증 — 실제 4th CHD

`roms/ez2dj4th/ez2dj4th.chd`를 읽기 전용으로 사용한 `re2dj ez2dj4th --linux-in-process-createfile-call` 실행 결과다.

| 항목 | registry | 정적 IAT slot | 게스트가 받은 값 |
| --- | --- | --- | --- |
| `kernel32` module handle | `0x6f000000` | — | `0x6f000000` |
| `CreateFileA` | `0x6f002039` | `0x6f002039` | `0x6f002039` |
| `GetVersion` | `0x6f002026` | — | `0x6f002026` |

같은 실행이 `CreateFileA("\\\\.\\NTICE")`에 도달했다. 인자는 access `0xc0000000`, share `0x00000003`, security null, disposition `0x00000003`, flags 0, template null의 7개이며, caller 복귀 주소 `0x00aeffbc`의 `INT3`에서 `SIGTRAP` EIP `0x00aeffbd`로 제한됐다. 7개 인자가 모두 읽히고 복귀 지점에 정확히 도달했다는 것이 28바이트 `__stdcall` cleanup이 성립했다는 뜻이다.

`0x7F000001`은 이 진단 경로에 남지 않는다.

### 검증 — 빌드와 테스트

* Windows x86 Debug 빌드와 단위 테스트: `checks: 2107, failures: 0`.
* Linux i386(`linux-x86-debug`) 전체 빌드: 오류 0건. 단위 테스트 `checks: 2107, failures: 0`.
* Linux i386 `re2dj_linux_native_guest_module_probe`: 종료 코드 0. 작업 343의 synthetic identity 검증이 이동 후에도 통과한다.
* Linux x64(`linux-x64-debug`) 빌드: 오류 0건. 단위 테스트 `checks: 2107, failures: 0`. 서드파티 SDL3 wayland protocol XML의 validity 경고는 이 작업과 무관하게 그대로다.
* 실제 4th CHD 회귀는 위에 기록했다.

### 남은 범위

`original_runner.cpp`의 `RunOriginalInProcessFirstResolver`와 `RunOriginalInProcessGetVersionCall`에는 `0x7F000001`이 남아 있다. 이번 작업의 완료 조건은 `CreateFileA` 진단 경로로 한정되어 있고 두 함수는 그보다 앞선 별도 진단이므로 건드리지 않았다. 같은 방식으로 합류시킬 수 있으며 후속 후보다.

`\\.\NTICE` 이후의 guest handle 의미, 다른 DLL facade, Linux x64 compatibility-mode trampoline, Windows WoW64 adapter는 설계대로 범위 밖이다.

## English

Design: [20260922-344-guest-module-resolver-convergence.md](../design/20260922-344-guest-module-resolver-convergence.md)
Work order: [20260922-344-guest-module-resolver-convergence.md](../work-orders/20260922-344-guest-module-resolver-convergence.md)
Prerequisite: [Task 343, kernel32 Linux i386 facade](20260922-343-kernel32-linux-i386-facade.md)
Analysis: [4th Linux in-process first import](../analysis/ez2dj4th-linux-inprocess-first-import.md)

### Implementation

| Layer | Change |
| --- | --- |
| Shared HLE | An optional `ImportCallServices` boundary on `ImportCall`, exposing only bounded guest-string reads and module/export lookup |
| Shared HLE | `GuestModuleRegistry::FindExport(const ImportGate&)` |
| `kernel32` module | `GetModuleHandleA` and `GetProcAddress` handlers, including the ordinal form where the upper word is zero |
| Linux import thunks | Retained `NativeImportSlotBinding` plus `RebindNativeGuestModuleImports` |
| Linux runner | `RunConfiguredNativePeInProcess` and the `NativePeSessionSetup` callback |
| Diagnostic | Pseudo handle and per-API branches removed from `native_create_file_observation.cpp`, which now implements the services |
| Placement | Task 343's i386-only sources and probe moved under `src/platform/linux/x86/` |

### Changed from the design — who rebinds

The design had `NativePeSession` map the facade and rebind directly. The implementation adds a `NativePeSessionSetup` callback invoked right after the session is prepared and lets the diagnostic do it. The rationale and the cost are recorded in the [design](../design/20260922-344-guest-module-resolver-convergence.md): `NativePeSession` should stay a general session that knows nothing about facades, and which modules to register differs per diagnostic.

### Strengthened during implementation — identity became an observation

The first implementation pre-seeded the observed fields with registry values during preparation. In that state a printed address could be either the registry value or what the guest received, so the completion criterion that static and dynamic resolution return the same thunk was **true by construction** rather than evidenced.

Three strands were separated. Registry values are kept in their own `registry_*` fields at preparation. Values the guest received are recorded only when the resolver actually returns them and are never pre-seeded, so zero means "the guest never asked" and is distinct from a mismatch. The static IAT value is read back from the slot while the image is mapped instead of trusting the value written.

The `kCreateFileCallNotReached` decision moved to `registry_create_file` rather than an observed value, preserving its previous meaning: it asks whether the facade was prepared.

### Verification — the real 4th CHD

From `re2dj ez2dj4th --linux-in-process-createfile-call` against a read-only `roms/ez2dj4th/ez2dj4th.chd`, the registry, static IAT slot, and guest-visible addresses agree as tabulated in the Korean section: `0x6f000000` for the module handle, `0x6f002039` for `CreateFileA`, and `0x6f002026` for `GetVersion`.

The same run reached `CreateFileA("\\\\.\\NTICE")` with seven arguments — access `0xc0000000`, share `0x00000003`, null security, disposition `0x00000003`, zero flags, null template — and was bounded at the caller return address `0x00aeffbc` with `SIGTRAP` at EIP `0x00aeffbd`. Reading all seven arguments and landing exactly on the return site is what establishes the 28-byte `__stdcall` cleanup. No `0x7F000001` remains on this diagnostic path.

### Verification — builds and tests

* Windows x86 Debug build and unit tests: `checks: 2107, failures: 0`.
* Linux i386 (`linux-x86-debug`) full build with no errors; unit tests `checks: 2107, failures: 0`.
* Linux i386 `re2dj_linux_native_guest_module_probe` exits zero, so Task 343's synthetic identity check still passes after the move.
* Linux x64 (`linux-x64-debug`) build with no errors; unit tests `checks: 2107, failures: 0`. The vendored SDL3 wayland-protocol XML validity warnings are unchanged and unrelated.
* The real 4th CHD regression is recorded above.

### Remaining scope

`RunOriginalInProcessFirstResolver` and `RunOriginalInProcessGetVersionCall` in `original_runner.cpp` still carry `0x7F000001`. This task's completion criteria are scoped to the `CreateFileA` diagnostic path and those two are earlier, separate diagnostics, so they were left alone; they can converge the same way and are a follow-up candidate.

Guest-handle semantics after `\\.\NTICE`, other DLL facades, the Linux x64 compatibility-mode trampoline, and the Windows WoW64 adapter remain out of scope as designed.
