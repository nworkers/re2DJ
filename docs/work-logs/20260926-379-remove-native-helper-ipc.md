# 작업 379 작업 로그 — native helper IPC 제거 / Task 379 work log — remove the native helper IPC

설계: [20260926-379-remove-native-helper-ipc.md](../design/20260926-379-remove-native-helper-ipc.md)
작업 지시서: [20260926-379-remove-native-helper-ipc.md](../work-orders/20260926-379-remove-native-helper-ipc.md)

## 진행 / Progress

사용 관계를 먼저 확인했다. Linux helper는 CLI `--linux-helper`와 `original_runner`의 `RunOriginalUntilBoundary`에서만 쓰였다. Windows helper backend·IPC helper·helper probe는 선택 빌드에서만 만들어졌고, IPC host probe와 import observer는 빌드되지도 않았다. Windows `native_import_thunks`·`native_pe_image`는 IPC helper만 썼다. 파일 19개를 지우고(`git rm`), CMake의 두 옵션을 없앴다. 옵션이 감싸던 SDL 설정과 Linux target은 조건 없이 남겼다.

*Usage came first: the Linux helper was used only by the CLI's `--linux-helper` and `original_runner`'s `RunOriginalUntilBoundary`; the Windows helper backend, IPC helper, and helper probe were built only by the optional build, and the IPC host probe and import observer were not built at all; the Windows `native_import_thunks` and `native_pe_image` served only the IPC helper. Nineteen files were removed (`git rm`), and the two CMake options went, leaving the SDL settings and Linux targets they guarded unconditional.*

## 변경 / Changes

- **삭제 / Deleted**: Linux `native_helper_backend.{h,cpp}`, `x86/native_ipc_helper.cpp`, `x86/native_ipc_helper_main.{h,cpp}`, `native_ipc_capability_rejection_helper.cpp`, `native_ipc_host_probe.cpp`; Windows `native_helper_backend.{h,cpp}`, `native_helper_probe.cpp`, `native_ipc_helper.cpp`, `native_ipc_host_probe.cpp`, `native_import_thunks.{h,cpp}`, `native_pe_image.{h,cpp}`; `src/tools/windows_import_observer/`; `src/platform/native_helper_protocol.h`; `scripts/test_linux_native_helper_probe.sh`.
- **Linux**: `kMaximumImportStringSize`/`kMaximumImportCount`는 `native_import_thunks.h`로 옮겼다. `original_runner`에서 `RunOriginalUntilBoundary`와 그것만 쓰던 도우미를 지웠다. / *`kMaximumImportStringSize`/`kMaximumImportCount` moved to `native_import_thunks.h`; `original_runner` lost `RunOriginalUntilBoundary` and the helpers only it used.*
- **CLI**: `--linux-helper`를 지웠다. `--linux-in-process-*` 진단이 없으면 늘 continuation이다. / *`--linux-helper` is gone; without an `--linux-in-process-*` diagnostic a run is always the continuation.*
- **CMake·preset**: `RE2DJ_BUILD_LINUX_NATIVE_HELPER`, `RE2DJ_BUILD_NATIVE_HELPER_PROBE`, helper target 5개, preset `linux-x86-helper`·`windows-x86-native-probe`(configure·build·test). / *`RE2DJ_BUILD_LINUX_NATIVE_HELPER`, `RE2DJ_BUILD_NATIVE_HELPER_PROBE`, five helper targets, and the `linux-x86-helper` and `windows-x86-native-probe` presets (configure, build, and test).*
- **문서 / Docs**: README, ARCHITECTURE(첫 절, mermaid, 현재형 bullet, target 표), KB 두 개, Linux build 가이드, IMPLEMENTED, `src/runtime`·`src/platform/{linux,windows}`·`include/re2dj/platform` README.

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| 남은 참조 / remaining references | code·CMake·preset에 helper·IPC·protocol 참조 없음 / none in code, CMake, or presets |
| Linux x64·x86 configure, build, CTest | 경고·오류 없음, 각각 3/3 / no warnings or errors, 3/3 each |
| Linux in-process probe, 두 폭 / both widths | `exit=51`, x64 `fault=SIGILL@0x11001008 rerun=ok`, x86 `dynamic=2 signal=4 process-exit=7`, 전과 같음 / as before |
| Linux 진단 네 개, 두 폭 / four diagnostics, both widths | first-import·first-resolver·getversion-call·createfile-call 결과 전과 같음 / as before |
| 실제 4th, Linux 두 폭 / real 4th, both widths | 호출 1,824번, `CreateSurface`에서 정지, 두 폭 같음 / 1,824 calls, stopping at `CreateSurface`, identical on both widths |
| Windows x86 configure, build, CTest | exit 0, 우리 코드 경고·오류 없음, 6/6. 제품 코드는 바뀌지 않아 실제 실행 비교는 하지 않음 / exit 0, no warnings or errors from this project, 6/6; the product code is unchanged, so no real-run comparison |
