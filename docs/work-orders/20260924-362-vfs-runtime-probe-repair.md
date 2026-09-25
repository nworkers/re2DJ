# 작업 362 작업 지시서 — `re2dj_windows_vfs_runtime_probe` 복구 / Task 362 work order — Repair `re2dj_windows_vfs_runtime_probe`

설계: [20260924-362-vfs-runtime-probe-repair.md](../design/20260924-362-vfs-runtime-probe-repair.md)

## 절차 / Steps

1. `src/tools/windows_vfs_runtime_probe/main.cpp`의 스트리밍 fixture에서 `DSBCAPS_STATIC`을 뺀다.
   *Drop `DSBCAPS_STATIC` from the streaming fixture in `src/tools/windows_vfs_runtime_probe/main.cpp`.*
2. wrap 검사의 기대값을 `shadow-offset=0 shadow-bytes=32`로 바꾼다.
   *Change the wrap check's expectation to `shadow-offset=0 shadow-bytes=32`.*
3. `ABSOLUTE.TXT` stream 두 개를 읽은 직후 닫는다.
   *Close the two `ABSOLUTE.TXT` streams right after reading.*
4. trace를 읽은 뒤 `g_re2dj_audio_trace_path`를 비우고, 마지막 정리를 `error_code`판 `remove_all`로 바꾼다.
   *After reading the trace, clear `g_re2dj_audio_trace_path` and switch the final cleanup to the `error_code` overload of `remove_all`.*
5. TODO의 "hang 원인" 항목을 닫는다.
   *Close the TODO "hang cause" item.*

## 완료 조건 / Done when

- Windows x86 build에 오류와 경고가 없다.
  *The Windows x86 build has no errors or warnings.*
- probe를 직접 반복 실행하면 모두 종료 코드 0이다.
  *Repeated direct probe runs all exit with code 0.*
- `ctest --preset windows-x86-debug`가 여러 번 연속으로 6/6을 통과한다.
  *`ctest --preset windows-x86-debug` passes 6/6 on several consecutive runs.*
