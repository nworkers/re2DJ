# 작업 366 작업 로그 — injected runtime 기록의 spdlog 전환 / Task 366 work log — Injected runtime records through spdlog

설계: [20260925-366-injected-runtime-spdlog.md](../design/20260925-366-injected-runtime-spdlog.md)
작업 지시서: [20260925-366-injected-runtime-spdlog.md](../work-orders/20260925-366-injected-runtime-spdlog.md)

## 변경 / Changes

- **`runtime_log.h/.cpp`**(새 파일): `WriteRuntimeLog(RuntimeLogChannel, const char*)`. channel별로 spdlog `basic_file_sink_mt` logger를 둔다. 이어쓰기이고 매 줄 flush하며, pattern은 `[%H:%M:%S.%e] [%t] %v`다. export 경로는 호출마다 확인해 비면 닫고 바뀌면 다시 연다. 열기에 실패한 경로는 다시 시도하지 않는다. `kRuntime`은 문구를 `OutputDebugStringA`로 그대로 먼저 보낸다. last error는 보존한다. export `g_re2dj_runtime_log_path`와 `Re2djCloseRuntimeLogs`를 추가했다.
  ***`runtime_log.h/.cpp`** (new): `WriteRuntimeLog(RuntimeLogChannel, const char*)` with one spdlog `basic_file_sink_mt` logger per channel (appending, flushing every line, pattern `[%H:%M:%S.%e] [%t] %v`). Export paths are checked per call, closing on empty and reopening on change, and a failed path is not retried; `kRuntime` first passes the text unchanged to `OutputDebugStringA`; the last error is preserved. Adds the exports `g_re2dj_runtime_log_path` and `Re2djCloseRuntimeLogs`.*
- **호출 지점.**
  ***Call sites.***
  - VFS의 `AppendVfsTraceMessage`, graphics의 `WriteGraphicsTraceLine`(자체 handle과 lock 제거), audio의 `Re2djAudioTrace`(줄 상한 유지)가 각 channel로 쓴다.
    *VFS's `AppendVfsTraceMessage`, graphics' `WriteGraphicsTraceLine` (its own handle and lock removed), and audio's `Re2djAudioTrace` (line limit kept) write through their channels.*
  - 다섯 파일의 `OutputDebugStringA` 58곳은 `kRuntime`으로 옮겼다.
    *The 58 `OutputDebugStringA` calls in five files moved to `kRuntime`.*
  - Hardlock transform 입력 dump는 형식이 고정된 데이터 파일이다. 그래서 `AppendDiagnosticFile`에 남기고 그 뜻을 주석에 적었다.
    *The Hardlock transform input dump is a fixed-format data file, so it stays on `AppendDiagnosticFile`, whose comment now says so.*
- **launcher.** 주입 직후 `*.runtime.log`를 쓰고 JSONL에 `runtime_log` 사건을 남긴다. 자식 process를 따라갈 때는 `*.child.runtime.log`를 쓴다.
  ***Launcher.** Writes `*.runtime.log` right after injection with a `runtime_log` JSONL event, and `*.child.runtime.log` when following a child process.*
- **CMake.** DLL에 `runtime_log.cpp`를 넣고 `spdlog::spdlog`에 link했다.
  ***CMake.** The DLL gains `runtime_log.cpp` and links `spdlog::spdlog`.*
- **VFS probe.** spdlog sink가 파일을 연 채로 두고 없는 디렉터리도 만든다. 그래서 첫 build에서는 매 실행마다 임시 디렉터리가 남았다. 남은 파일은 DLL이 unload될 때 쓴 `vfs.log`의 exit 기록이었다. 이제 probe는 정리 전에 audio와 VFS 경로를 비우고 `Re2djCloseRuntimeLogs`를 부른다.
  ***VFS probe.** Because the spdlog sink keeps its file open and creates missing directories, the first build left a temporary directory after every run, holding the exit records the DLL wrote to `vfs.log` while unloading. The probe now clears the audio and VFS paths and calls `Re2djCloseRuntimeLogs` before cleanup.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Windows x86 build, CTest | 오류·경고 없음, 6/6 / no errors or warnings, 6/6 |
| Linux x64·x86 CTest | 각각 3/3 / 3/3 each |
| VFS probe 6회 / 6 runs | 모두 종료 코드 0, 남은 임시 디렉터리 0 / all exit 0, no temporary directory left |
| Windows 실제 4th, 20초 뒤 종료 / real 4th, stopped after 20 s | `*.runtime.log`(149 KB), `*.vfs.log`(144 KB), `*.ddraw.log`(186 KB)가 `[00:23:55.915] [12044] re2dj:vfs:CreateFileA` 형식으로 남음. JSONL의 `runtime_log` 사건은 `written:true`. 게임 화면까지 진행 / `*.runtime.log` (149 KB), `*.vfs.log` (144 KB), and `*.ddraw.log` (186 KB) in the form `[00:23:55.915] [12044] re2dj:vfs:CreateFileA`; the JSONL `runtime_log` event reports `written:true`; the game reached its screen |

이 세션의 probe 실행이 남긴 임시 디렉터리는 지웠다. 그 전부터 있던 40개는 건드리지 않았다.

*Temporary directories left by this session's probe runs were removed; the 40 that predate it were left alone.*
