# 작업 362 설계 — `re2dj_windows_vfs_runtime_probe` 복구 / Task 362 design — Repair `re2dj_windows_vfs_runtime_probe`

## 배경

Windows CTest의 `re2dj_windows_vfs_runtime_probe`는 작업 303 이후 계속 실패했다. TODO에는 "hang"으로 적혀 있었다. 조사 결과 원인은 제품 코드가 아니라 probe 자체에 있었다. 결함은 세 개이고, 앞의 것이 뒤의 것을 가리고 있었다.

1. **스트리밍 fixture 분류.** probe의 32 byte 스트리밍 ring은 `DSBCAPS_STATIC|LOCHARDWARE|GETCURRENTPOSITION2`로 만든다. 작업 303은 `IsStreamingBufferDescription`에서 `STATIC` buffer를 스트리밍에서 제외했다. 그래서 이 buffer는 일반 buffer가 되었고, `directsound:streaming-start`와 `streaming=1`이 기록되지 않았다.
2. **trace 필드 이름.** wrap 검사는 `dirty-offset=24 dirty-bytes=16`을 찾는다. 그러나 커밋 9452bd2 이후 backend는 unlock마다 ring 전체를 shadow로 복사하고, `shadow-offset=0 shadow-bytes=32`를 기록한다.
3. **정리 단계 abort.** 모든 검사가 통과해도 마지막 `std::filesystem::remove_all(root)`가 예외를 던지고, 처리되지 않은 예외가 `abort()`(종료 코드 3)가 되었다. 이유는 두 가지다.
   - `ABSOLUTE.TXT`를 읽은 `std::ifstream` 두 개가 `main`이 끝날 때까지 열려 있다(오류 32).
   - 검사 뒤에도 mixer thread가 `Re2djAudioTrace`를 호출해 `audio.log`를 다시 만든다(`OPEN_ALWAYS`, "directory is not empty"). 두 번째 이유는 실행마다 결과가 달라지는 경쟁이다.

## 결정

- 스트리밍 fixture에서 `DSBCAPS_STATIC`만 뺀다. 나머지 flag는 유지한다. 분류 규칙(작업 303)은 바꾸지 않는다.
- wrap 검사는 현재 backend 계약인 `shadow-offset=0 shadow-bytes=32`와 `lock-offset=24 first=8 second=8`, `backend-refresh=1`을 확인한다.
- `ABSOLUTE.TXT` stream은 읽은 직후 닫는다.
- trace를 읽은 뒤에는 `g_re2dj_audio_trace_path`를 비워 파일 trace를 멈춘다. 마지막 정리는 `error_code`판 `remove_all`을 써서, 정리 경쟁 때문에 probe가 abort되지 않게 한다.

제품 코드는 바꾸지 않는다.

---

## Background

Windows CTest's `re2dj_windows_vfs_runtime_probe` has failed since Task 303; TODO recorded it as a "hang". The cause lies in the probe itself, not in product code. There were three defects, each hiding the next:

1. **Streaming fixture classification.** The probe's 32-byte streaming ring used `DSBCAPS_STATIC|LOCHARDWARE|GETCURRENTPOSITION2`. Task 303 excluded `STATIC` buffers in `IsStreamingBufferDescription`, so the buffer became an ordinary buffer and `directsound:streaming-start`/`streaming=1` never appeared.
2. **Trace field names.** The wrap check looked for `dirty-offset=24 dirty-bytes=16`. Since 9452bd2, however, the backend copies the whole ring as a shadow on every unlock and records `shadow-offset=0 shadow-bytes=32`.
3. **Cleanup abort.** Even with every check passing, the final `std::filesystem::remove_all(root)` threw, and the unhandled exception became `abort()` (exit code 3), for two reasons:
   - two `std::ifstream`s that read `ABSOLUTE.TXT` stayed open until `main` returned (error 32);
   - after the checks, the mixer thread still called `Re2djAudioTrace` and recreated `audio.log` (`OPEN_ALWAYS`, "directory is not empty"). This second reason is a race, so the result varied between runs.

## Decisions

- Drop only `DSBCAPS_STATIC` from the streaming fixture and keep the other flags; the Task 303 classification rule is unchanged.
- The wrap check verifies the current backend contract: `shadow-offset=0 shadow-bytes=32` together with `lock-offset=24 first=8 second=8` and `backend-refresh=1`.
- Close the `ABSOLUTE.TXT` streams right after reading them.
- After reading the trace, clear `g_re2dj_audio_trace_path` to stop file tracing, and use the `error_code` overload of `remove_all` for the final cleanup so a cleanup race cannot abort the probe.

No product code changes.
