# 작업 366 설계 — injected runtime 기록의 spdlog 전환 / Task 366 design — Injected runtime records through spdlog

선행: [작업 365 설계](20260925-365-host-run-output-spdlog.md)

## 배경 / Background

Windows에서 게스트 process 안에 주입되는 `re2dj_windows_injected_runtime.dll`은 기록을 네 갈래로 따로 남겼다.

*The `re2dj_windows_injected_runtime.dll` injected into the Windows guest process kept its records along four separate paths:*

| 갈래 / Path | 방식 / Mechanism | 파일 / File |
| --- | --- | --- |
| 일반 진단 / general diagnostics | `OutputDebugStringA` 약 60곳. launcher가 debugger로 붙어 있을 때만 JSONL에 남는다 / about 60 `OutputDebugStringA` calls, reaching the JSONL only while the launcher is attached as debugger | 없음 / none |
| VFS | `AppendDiagnosticFile`: 줄마다 `CreateFileA`/`WriteFile`/`CloseHandle` / `CreateFileA`/`WriteFile`/`CloseHandle` per line | `*.vfs.log` |
| graphics | `WriteGraphicsTraceLine`: 공유 handle과 `SRWLOCK` / a shared handle and an `SRWLOCK` | `*.ddraw.log` |
| audio | `Re2djAudioTrace`: 줄마다 파일을 열고 닫는다 / opens and closes the file per line | `*.audio.log` |

launcher는 일부 `OutputDebugStringA` 문구(handoff 등)를 그대로 기다린다. 따라서 그 문구는 바뀌면 안 된다.

*The launcher waits for some `OutputDebugStringA` texts (such as the handoff) verbatim, so those texts must not change.*

## 결정 / Decisions

1. **공용 진입점.** DLL 안에 `runtime_log.h/.cpp`를 두고 `WriteRuntimeLog(RuntimeLogChannel, const char*)`를 제공한다. channel은 `kRuntime`, `kVfs`, `kGraphics`, `kAudio`다.
   ***Single entry point:** `runtime_log.h/.cpp` in the DLL provides `WriteRuntimeLog(RuntimeLogChannel, const char*)` with channels `kRuntime`, `kVfs`, `kGraphics`, and `kAudio`.*
2. **파일 기록은 spdlog.** 각 channel은 launcher가 export에 써 준 경로로 spdlog `basic_file_sink_mt`를 연다. 이어쓰기이고, 매 줄 flush한다. pattern은 `[%H:%M:%S.%e] [%t] %v`이고 줄 끝은 spdlog가 붙인다.
   ***File records through spdlog:** each channel opens an spdlog `basic_file_sink_mt` at the path the launcher wrote into its export, appending and flushing every line, with pattern `[%H:%M:%S.%e] [%t] %v` and spdlog ending the line.*
   - 경로는 호출마다 확인한다. 비면 logger를 닫고, 바뀌면 새로 연다. 그래서 경로를 비워 기록을 멈추는 probe도 파일 handle을 붙잡지 않는다.
     *The path is checked on every call: an empty path closes the logger and a changed one reopens it, so a probe that clears the path to stop recording holds no file handle.*
   - 열기에 실패한 경로는 다시 시도하지 않는다.
     *A path that fails to open is not retried.*
   - logger는 spdlog 전역 registry에 넣지 않는다. 게스트 process 안의 다른 이름과 겹치지 않게 하기 위해서다.
     *Loggers stay out of spdlog's global registry, so nothing clashes with other names in the guest process.*
3. **`kRuntime`.** 받은 문구를 `OutputDebugStringA`로 **바꾸지 않고** 먼저 보낸다. launcher의 handoff 대기와 JSONL 기록은 그대로다. 새 export `g_re2dj_runtime_log_path`에 경로가 있으면 같은 줄을 파일에도 남긴다. launcher는 이 경로를 JSONL의 형제 `*.runtime.log`로 준다. 6th처럼 따라가는 자식 process에는 `*.child.runtime.log`를 준다. 기존 `OutputDebugStringA` 호출은 모두 이 channel로 바꾼다. graphics와 audio는 지금처럼 자기 ODS를 유지하고, 파일만 자기 channel로 쓴다.
   ***`kRuntime`** first passes the text **unchanged** to `OutputDebugStringA`, keeping the launcher's handoff wait and JSONL record as they are, then writes the same line to the file at the new export `g_re2dj_runtime_log_path` when set. The launcher sets it to the JSONL's sibling `*.runtime.log`, and to `*.child.runtime.log` for a followed child process such as 6th's. Every existing `OutputDebugStringA` call moves to this channel; graphics and audio keep their own ODS and write only their file through their channel.*
4. **게스트 last error 보존.** 파일 기록의 I/O가 게스트가 읽을 `GetLastError`를 덮지 않도록 `WriteRuntimeLog`가 last error를 저장했다가 되돌린다.
   ***Guest last error preserved:** `WriteRuntimeLog` saves and restores the last error, so recording I/O never overwrites what the guest reads from `GetLastError`.*
5. **상한은 그대로.** 각 호출 지점의 기록 상한(예: audio 줄 수 상한, VFS budget)은 바꾸지 않는다.
   ***Bounds unchanged:** each call site's recording bound (for example the audio line limit and the VFS budgets) stays as it is.*
6. **link.** `re2dj_windows_injected_runtime`은 `spdlog::spdlog`(정적)에 link한다.
   ***Linking:** `re2dj_windows_injected_runtime` links `spdlog::spdlog` (static).*

## 영향 / Impact

- 파일 줄에 시각과 thread 접두사가 붙는다. 이 파일을 읽는 곳은 `re2dj_windows_vfs_runtime_probe`의 부분 문자열 검사뿐이다. 이 검사는 접두사에 영향을 받지 않는다.
  *File lines gain a time and thread prefix; the only reader is `re2dj_windows_vfs_runtime_probe`'s substring checks, which the prefix does not affect.*
- 게스트 process 안의 hardlock descriptor 출력 파일(`g_re2dj_hardlock_descriptor_output`)은 기록이 아니라 산출물이므로 그대로 둔다.
  *The hardlock descriptor output file (`g_re2dj_hardlock_descriptor_output`) is an artifact, not a record, and stays as it is.*
