# 작업 366 작업 지시서 — injected runtime 기록의 spdlog 전환 / Task 366 work order — Injected runtime records through spdlog

설계: [20260925-366-injected-runtime-spdlog.md](../design/20260925-366-injected-runtime-spdlog.md)

## 절차 / Steps

1. `src/platform/windows/runtime_log.h/.cpp`에 `WriteRuntimeLog`, `g_re2dj_runtime_log_path`, `Re2djCloseRuntimeLogs`를 추가한다.
   *Add `WriteRuntimeLog`, `g_re2dj_runtime_log_path`, and `Re2djCloseRuntimeLogs` in `src/platform/windows/runtime_log.h/.cpp`.*
2. VFS·graphics·audio의 파일 기록과 모든 `OutputDebugStringA` 호출을 channel로 옮긴다. 데이터 dump 파일은 그대로 둔다.
   *Move the VFS, graphics, and audio file writes and every `OutputDebugStringA` call to the channels, leaving data dump files as they are.*
3. launcher가 `*.runtime.log`(자식은 `*.child.runtime.log`) 경로를 쓴다.
   *The launcher writes the `*.runtime.log` path (`*.child.runtime.log` for a child).*
4. DLL을 `spdlog::spdlog`에 link한다. VFS probe는 정리 전에 기록을 멈추고 파일을 닫는다.
   *Link the DLL to `spdlog::spdlog`; the VFS probe stops recording and closes the files before cleanup.*

## 완료 조건 / Done when

- Windows x86 build에 경고가 없고, CTest가 6/6이며, Linux 두 폭 CTest도 통과한다.
  *Windows x86 builds without warnings with CTest 6/6, and both Linux widths pass CTest.*
- VFS probe를 반복 실행해도 임시 디렉터리가 남지 않는다.
  *Repeated VFS probe runs leave no temporary directory.*
- Windows 실제 4th 실행에서 `*.runtime.log`, `*.vfs.log`, `*.ddraw.log`가 접두사를 단 줄로 남는다. launcher의 handoff도 그대로 진행한다.
  *A Windows real-4th run leaves `*.runtime.log`, `*.vfs.log`, and `*.ddraw.log` with prefixed lines, and the launcher's handoff proceeds as before.*
