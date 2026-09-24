# 작업 353 작업 지시서 — Linux x64 compatibility-mode 전환 runtime / Task 353 work order — Linux x64 compatibility-mode transition runtime

설계: [20260924-353-linux-x64-compat-mode-adapter.md](../design/20260924-353-linux-x64-compat-mode-adapter.md)
선행: [작업 340 계획](20260921-340-guest-pe-compatibility-modules.md) 7단계

[작업 353 설계](../design/20260924-353-linux-x64-compat-mode-adapter.md)의 1단계를 구현한다. x86-64 `re2dj` 프로세스 안에서 32비트 게스트 코드로 들어갔다가 돌아오는 전환 runtime을 만들고, 합성 기계어 probe로 검증한다.

*Implement stage 1 of the [Task 353 design](../design/20260924-353-linux-x64-compat-mode-adapter.md): a transition runtime that enters 32-bit guest code and returns inside the x86-64 `re2dj` process, validated with a synthetic machine-code probe.*

## 단계 / Steps

1. 폭 중립 타입을 추출한다. `NativeImportGateEvent`/`Result`/`Handler`는 `src/platform/linux/native_import_gate.h`로, `NativeGuestFault`는 `src/platform/linux/native_guest_fault.h`로 옮긴다. x86 헤더는 새 헤더를 include한다.
2. `src/platform/linux/x64/native_compat_mode_transition.h/.cpp`에 전환 blob과 host asm routine을 작성한다.
3. `src/platform/linux/x64/native_compat_mode.h/.cpp`에 `NativeCompatModeRuntime`(초기화, 시험 전환, `Run`, signal 처리)을 작성한다.
4. `src/platform/linux/x64/native_compat_mode_probe.cpp`를 작성한다. CMake의 Linux 64비트 분기에 runtime source와 probe를 추가하고 `add_test`로 등록한다.
5. Linux x64 debug를 빌드하고 CTest와 probe를 실행한다. Linux x86 debug·helper를 빌드하고 기존 probe를 실행한다.
6. KB, `ARCHITECTURE.md`, `src/platform/linux/README.md`, 설계 307·340, `docs/TODO.md`, 작업 로그를 갱신하고 커밋한다.

*Steps: (1) extract the width-neutral types, moving `NativeImportGateEvent`/`Result`/`Handler` to `src/platform/linux/native_import_gate.h` and `NativeGuestFault` to `src/platform/linux/native_guest_fault.h`, with the x86 headers including them; (2) write the transition blob and host asm routines in `src/platform/linux/x64/native_compat_mode_transition.h/.cpp`; (3) write `NativeCompatModeRuntime` (initialization, trial transition, `Run`, signal handling) in `src/platform/linux/x64/native_compat_mode.h/.cpp`; (4) write `src/platform/linux/x64/native_compat_mode_probe.cpp`, adding the runtime sources and probe to the Linux 64-bit CMake branch and registering it with `add_test`; (5) build Linux x64 debug and run CTest and the probe, then build Linux x86 debug and helper and run the existing probes; (6) update the KB, `ARCHITECTURE.md`, `src/platform/linux/README.md`, designs 307 and 340, `docs/TODO.md`, and the work log, then commit.*

## 완료 조건 / Completion criteria

* `re2dj_linux_compat_mode_probe`가 `fsgsbase` 경로와 `arch_prctl` 경로 모두에서 설계의 검증 항목 1~6을 통과한다.
* Linux x64 CTest 전체와 Linux x86 기존 probe·단위 테스트가 통과한다.
* 확인된 사실과 미확정 사항(미지원 커널 동작 등)이 작업 로그와 KB에 구분되어 기록된다.

*Completion: `re2dj_linux_compat_mode_probe` passes design checks 1–6 on both the `fsgsbase` and `arch_prctl` paths; the full Linux x64 CTest run and the existing Linux x86 probes and unit tests pass; and confirmed facts are recorded separately from unresolved items (such as unsupported-kernel behavior) in the work log and KB.*
