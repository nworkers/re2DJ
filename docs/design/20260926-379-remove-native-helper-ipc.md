# 작업 379 설계 — native helper IPC 제거 / Task 379 design — remove the native helper IPC

선행: [작업 353 설계](20260924-353-linux-x64-compat-mode-adapter.md), [작업 357 로그](../work-logs/20260924-357-linux-default-in-process-run.md), [작업 378 설계](20260926-378-linux-no-windows-sources.md)

## 배경 / Background

원본 PE32를 별도 32비트 helper process에서 실행하고 host와 IPC로 주고받는 경로가 두 host에 있었다.
- Linux: i386 `re2dj_linux_native_ipc_helper`, 이를 부르는 `NativeHelperBackend`, CLI `--linux-helper`.
- Windows: WOW64 `re2dj_native_ipc_helper`와 `NativeHelperBackend`(선택 빌드 `RE2DJ_BUILD_NATIVE_HELPER_PROBE`).

Linux 제품은 작업 357부터 두 폭 모두 원본을 같은 프로세스에서 실행한다. helper는 진단 fallback으로만 남아 있었다. Windows 제품은 원본을 WOW64 process로 실행하고 runtime을 주입하므로 helper를 쓰지 않는다. 작업 378 뒤 사용자가 Linux IPC도 없어야 하지 않느냐고 물었고, 두 host 모두에서 제거하기로 정했다(2026-09-26).

*Both hosts had a path that ran the original PE32 in a separate 32-bit helper process talking to the host over IPC: on Linux the i386 `re2dj_linux_native_ipc_helper`, the `NativeHelperBackend` that drives it, and the CLI's `--linux-helper`; on Windows the WOW64 `re2dj_native_ipc_helper` and its `NativeHelperBackend` (the optional `RE2DJ_BUILD_NATIVE_HELPER_PROBE` build). Since Task 357 the Linux product runs the original in its own process on both widths, leaving the helper as a diagnostic fallback only, and the Windows product runs the original as a WOW64 process with an injected runtime and never uses the helper. After Task 378 the user asked whether the Linux IPC should also go, and chose (2026-09-26) to remove it on both hosts.*

## 결정 / Decisions

1. **Linux에서 지우는 것.** `native_helper_backend`(header 포함), `x86/native_ipc_helper*`, capability rejection helper, IPC host probe, `RunOriginalUntilBoundary`, CLI `--linux-helper`, preset `linux-x86-helper`, `scripts/test_linux_native_helper_probe.sh`, CMake 옵션 `RE2DJ_BUILD_LINUX_NATIVE_HELPER`와 그 조건문이다. 옵션 없이 늘 해 오던 SDL 설정과 Linux target 정의는 조건 없이 남긴다.
   ***Removed on Linux:** `native_helper_backend` (with its header), `x86/native_ipc_helper*`, the capability-rejection helper, the IPC host probe, `RunOriginalUntilBoundary`, the CLI's `--linux-helper`, the `linux-x86-helper` preset, `scripts/test_linux_native_helper_probe.sh`, and the `RE2DJ_BUILD_LINUX_NATIVE_HELPER` CMake option with its conditions; the SDL settings and Linux targets it guarded stay, unconditionally.*
2. **Windows에서 지우는 것.** `native_helper_backend`(header 포함), `native_helper_probe`, `native_ipc_helper`, 빌드되지 않던 `native_ipc_host_probe`와 `src/tools/windows_import_observer`, 그리고 helper만 쓰던 Windows `native_import_thunks`·`native_pe_image`, preset `windows-x86-native-probe`, CMake 옵션 `RE2DJ_BUILD_NATIVE_HELPER_PROBE`다.
   ***Removed on Windows:** `native_helper_backend` (with its header), `native_helper_probe`, `native_ipc_helper`, the unbuilt `native_ipc_host_probe` and `src/tools/windows_import_observer`, the Windows `native_import_thunks` and `native_pe_image` only the helper used, the `windows-x86-native-probe` preset, and the `RE2DJ_BUILD_NATIVE_HELPER_PROBE` CMake option.*
3. **공용 protocol.** `src/platform/native_helper_protocol.h`를 지운다. Linux import thunk가 쓰던 크기 상한 두 개(`kMaximumImportStringSize`, `kMaximumImportCount`)는 `native_import_thunks.h`로 옮긴다. `src/platform/native_probe_fixture`는 Linux in-process probe가 계속 쓰므로 남긴다.
   ***Shared protocol:** `src/platform/native_helper_protocol.h` goes; the two size limits the Linux import thunks used (`kMaximumImportStringSize`, `kMaximumImportCount`) move to `native_import_thunks.h`. `src/platform/native_probe_fixture` stays, as the Linux in-process probes still use it.*
4. **문서.** 현재 상태를 말하는 README·ARCHITECTURE·KB·가이드는 고친다. 작업별 design·work-log와 ARCHITECTURE의 날짜별 절은 당시 기록으로 둔다.
   ***Documentation:** the README, ARCHITECTURE, KB, and guides that state the current design are updated; per-task designs and work logs and ARCHITECTURE's dated sections stay as the record of their time.*
