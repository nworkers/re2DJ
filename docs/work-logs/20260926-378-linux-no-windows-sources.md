# 작업 378 작업 로그 — Linux가 Windows 소스를 참조하지 않게 / Task 378 work log — Linux references no Windows sources

설계: [20260926-378-linux-no-windows-sources.md](../design/20260926-378-linux-no-windows-sources.md)
작업 지시서: [20260926-378-linux-no-windows-sources.md](../work-orders/20260926-378-linux-no-windows-sources.md)

## 진행 / Progress

점검에서 Linux probe 3개가 `src/platform/windows/native_ipc_host_probe.cpp`를 포함하고 있음을 찾았다. fixture를 공용 위치로 옮긴 뒤, Linux IPC host probe에 자기 실행 흐름을 넣었다. 첫 build는 새로 쓴 문자열의 `\n`이 shell heredoc에서 실제 줄바꿈으로 바뀌어 실패했다. Edit 도구로 고쳤다.

*The audit found three Linux probes including `src/platform/windows/native_ipc_host_probe.cpp`. After the fixture moved to the shared place and the Linux IPC host probe got its own run, the first build failed because a new string's `\n` became a real newline through a shell heredoc; the Edit tool fixed it.*

## 변경 / Changes

- **`src/platform/native_probe_fixture.h/.cpp`**(새 파일): 합성 PE32, 상수, `ProbeImportHandler`.
  ***`src/platform/native_probe_fixture.h/.cpp`** (new): the synthetic PE32, its constants, and `ProbeImportHandler`.*
- **`src/platform/linux/native_ipc_host_probe.cpp`**: `RunBaselineProbe`, `GuestMemoryChecks`, `char**` 인자. Windows include, namespace 별칭, `wmain` define을 없앴다.
  ***`src/platform/linux/native_ipc_host_probe.cpp`:** `RunBaselineProbe`, `GuestMemoryChecks`, and `char**` arguments, without the Windows include, namespace alias, or `wmain` define.*
- **`src/platform/linux/{x86,x64}/native_in_process_probe.cpp`**: 공용 fixture만 include한다. / *include only the shared fixture.*
- **`src/platform/windows/native_ipc_host_probe.cpp`**: 공용 fixture를 쓰고 Linux 분기를 지웠다. 빌드하는 target은 없다. / *uses the shared fixture without the Linux branches; no target builds it.*
- **CMake**: Linux probe target 3개에 `src/platform/native_probe_fixture.cpp`를 더했다. / *the three Linux probe targets gain `src/platform/native_probe_fixture.cpp`.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| `src/platform/windows` 참조 / references | Linux 코드·target에 없음. 남은 것은 CLI의 `_WIN32` 분기와 Windows 전용 키보드 테스트 2개 / none in Linux code or targets; what remains is the CLI's `_WIN32` branch and two Windows-only keyboard tests |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 / no warnings or errors, 3/3 each |
| helper script, 두 폭 / both widths | 기본 `result=51`, fault `signal=4 eip=0x1000100f`, stop, capability 거부(`rejected before LoadImage`) 모두 전과 같음 / baseline `result=51`, fault `signal=4 eip=0x1000100f`, stop, and capability rejection (`rejected before LoadImage`) all as before |
| in-process probe, 두 폭 / both widths | `exit=51`, x64 `fault=SIGILL@0x11001008 rerun=ok`, x86 `dynamic=2 signal=4 process-exit=7`, 전과 같음 / as before |
| 실제 4th, 두 폭 / real 4th, both widths | 호출 1,824번, `CreateSurface`에서 정지, 두 폭 같음 / 1,824 calls, stopping at `CreateSurface`, identical on both widths |
| Windows x86 build, CTest | exit 0, 우리 코드 경고·오류 없음, 6/6 / exit 0, no warnings or errors from this project, 6/6 |
