# Linux 원본 entry bring-up 작업 로그

## 결과

Linux i386 helper를 probe 전용 이름에서 `re2dj_linux_native_ipc_helper` production target으로 승격했다. 기존 x86-64 synthetic host probe는 같은 helper를 계속 사용하므로 PE32 requested-base mapping, relocation, TLS callback, named/ordinal import gate와 bounded stack-memory IPC 회귀를 유지한다.

Linux x86-64 host에는 `original_runner` orchestration을 추가했다. 사용자가 지정한 HDD target executable을 읽어 `NativeHelperBackend::PrepareImage`와 `Start`를 호출하고, 첫 import gate·process exit·fault·stop event를 구조화한 결과로 반환한다. import gate는 loader의 `LoadedPeImage.imports` metadata와 대조하며, 알려지지 않은 gate address는 빈 이름으로 진행하지 않고 통제된 오류로 종료한다. 결과 구조체는 호출마다 초기화해 재사용 시 이전 경계 정보가 남지 않게 했다.

CLI의 Linux `--run`은 명시적인 `--linux-helper <path>`를 받아 선택 target의 실제 host path와 PE metadata를 runner에 전달한다. 첫 미구현 import는 module과 name 또는 ordinal, gate, EIP와 ESP를 출력하고 별도 종료 코드로 끝난다. process exit와 guest fault도 구분한다. 다른 host의 기존 미구현 실행 동작은 유지했다.

## 검증

- WSL Ubuntu 24.04에서 `RE2DJ_WARNINGS_AS_ERRORS=ON` i386 helper build가 성공했다.
- `file` 결과는 helper가 `ELF 32-bit LSB pie executable, Intel 80386`임을 확인했다.
- WSL에 desktop X11·Wayland 개발 package가 없어 일반 `linux-x64-debug` configure는 SDL3 prerequisite 검사에서 중단됐다. `sudo` 설치는 인증 입력이 필요해 수행하지 않았다.
- 같은 source를 SDL의 공식 `SDL_UNIX_CONSOLE_BUILD=ON`으로 별도 Linux x64 warnings-as-errors build해 새 `original_runner.cpp`와 CLI를 포함한 전체 target compile을 확인했다.
- Linux CTest 1/1이 통과했다.
- x86-64 host/i386 production helper integration은 `load=0x11000000`, `entry=0x11001000`, imports 2개, arguments 41·42, result 51, child 0으로 성공했다.
- 원본 HDD 자산은 읽거나 수정하지 않았다. 실제 원본의 첫 경계 관찰은 사용자가 제공한 HDD path에서 수행할 검증으로 남긴다.

## 회고

이번 단계는 기존 synthetic feasibility 경계를 제품 CLI에 연결했지만 Win32 API를 아직 구현하지 않는다. 따라서 첫 import에서 의도적으로 멈추는 것이 정상이며, 다음 작업은 protocol을 성급히 확장하기보다 guest stack·TEB/PEB·FS와 fault context부터 안정화해야 한다. Linux desktop package 부재와 helper/runtime 코드의 컴파일 정확성을 분리해 검증함으로써 환경 prerequisite를 구현 실패로 오인하지 않았다.

---

# Linux Original Entry Bring-up Work Log

## Result

Promoted the Linux i386 helper from probe-only naming to the production `re2dj_linux_native_ipc_helper` target. The existing x86-64 synthetic host probe continues to use the same helper, retaining regression coverage for requested-base PE32 mapping, relocations, TLS callbacks, named and ordinal import gates, and bounded stack-memory IPC.

Added Linux x86-64 `original_runner` orchestration. It reads the selected executable from the user-supplied HDD, calls `NativeHelperBackend::PrepareImage` and `Start`, and returns the first import-gate, process-exit, fault, or stopped event as a structured result. Import gates are resolved against `LoadedPeImage.imports`; an unknown gate address now produces a controlled error rather than continuing with an empty name. The result is reset on every call so reused result storage cannot retain stale boundary details.

Linux `--run` now accepts an explicit `--linux-helper <path>` and passes the selected target's resolved host path and PE metadata to the runner. The first unimplemented import reports its module and name or ordinal plus gate, EIP, and ESP before returning a distinct exit code. Process exits and guest faults are reported separately. Existing unimplemented execution behavior on other hosts remains unchanged.

## Verification

- The warnings-as-errors i386 helper build passed under WSL Ubuntu 24.04.
- `file` identified the helper as an `ELF 32-bit LSB pie executable, Intel 80386`.
- The normal `linux-x64-debug` configure stopped at SDL3's prerequisite check because the WSL instance lacks desktop X11 and Wayland development packages. Installation was not performed because `sudo` required interactive authentication.
- A separate warnings-as-errors Linux x64 build using SDL's supported `SDL_UNIX_CONSOLE_BUILD=ON` compiled all targets, including the new `original_runner.cpp` and CLI, from the same source.
- Linux CTest passed 1/1.
- The x86-64 host/i386 production-helper integration passed with `load=0x11000000`, `entry=0x11001000`, two imports, arguments 41 and 42, result 51, and child exit 0.
- No original HDD assets were read or modified. Observing the first boundary of an original executable remains a user-supplied-HDD validation step.

## Retrospective

This phase connects the existing synthetic feasibility boundary to the product CLI but does not yet implement Win32 APIs. Stopping deliberately at the first import is therefore expected. The next task should stabilize the guest stack, TEB/PEB, FS state, and fault context before broad protocol expansion. Separating missing Linux desktop prerequisites from compilation of the helper and runtime code avoided treating an environment limitation as an implementation failure.
