# Task 155: bounded 진단 idle timeout 설정 작업 로그

## 결과 요약

launcher probe의 bounded 진단 loop가 쓰던 고정 5초 idle 경계를 `--diagnostic-idle-timeout <milliseconds>`로 조정할 수 있게 했습니다. 기본값은 `5000`으로 기존 동작과 같습니다.

이 옵션으로 Task 154의 관찰 한계가 해소되었습니다. 경계를 60초로 늘리자 이전 세션이 기록한 실행 순서(object source boundary → target match → field access → `0x00434137 / 0xc0000005` AV)가 이 환경에서도 그대로 재현되었고, trace는 `idle_timeout`이 아니라 `child_exit`으로 끝났습니다. 따라서 Task 154에서 "이 환경에서 AV가 재현되지 않는다"고 기록한 것은 실행 차이가 아니라 **고정 idle 경계가 만든 관찰 한계**였습니다.

같은 확장 경계로 Task 154의 후보 실행 추적을 다시 수행한 결과, AV까지의 전체 구간에서 후보 hit는 2건이었고 `target_matches`는 여전히 0건이었습니다. 네 개의 `+0x11c` write 후보 중 어느 것도 `0x00acd824`를 쓰지 않는다는 판정이 이제 fault 시점까지의 전체 구간에 대해 확인됩니다.

## 변경 사항

- `--diagnostic-idle-timeout <milliseconds>` 옵션과 usage 문자열을 추가했습니다.
- 기본값 `5000`, 허용 범위 `1000`–`600000`을 두고, 파싱 실패나 범위 밖 값은 usage 출력 후 실패합니다.
- 값을 `WaitForExitProcessBreakpoint`로 전달해 bounded 진단 loop의 `WaitForDebugEvent` 대기에만 적용했습니다. 초기 breakpoint 대기와 unload tail 수집 등 다른 대기 경로의 상수는 바꾸지 않았습니다.
- `launch` 진단 event에 `diagnostic_idle_timeout_ms`를 기록해 로그만으로 관찰 경계를 재구성할 수 있게 했습니다.

## 검증 증거

- Windows x86 Debug 전체 빌드: 성공
- `build/windows-x86/bin/Debug/re2dj_unit_tests.exe`: `checks: 1184, failures: 0`
- 범위 검증: `--diagnostic-idle-timeout 100`은 usage를 출력하고 실행하지 않습니다.

### 확장 경계 baseline 실행

`logs/windows_x86_launcher_probe/ez2dj4th/20260903-114428-322.jsonl` (`diagnostic_idle_timeout_ms=60000`, Task 150–152와 같은 옵션 조합)

- `null_context_object_source_trace_boundary`: `reason=child_exit`, `boundary_hits=1`, `hits=2`, `target_matches=1`, `code=0xc0000005`
- `null_context_field_access_trace_boundary`: `reason=child_exit`, `hits=1`, `code=0xc0000005`
- field access hit: `eip_after=0x0041a69f`, `ECX=0x00000000`
- fault: `0x00434137`에서 `0xc0000005` 2건, 참조 주소 `0x00000014`

### 확장 경계 후보 실행 추적

`20260903-114501-799.jsonl`과 재현 실행 `20260903-114540-170.jsonl` (`--null-context-field-reference-execution-trace`, `diagnostic_idle_timeout_ms=60000`)

| 실행 | boundary reason | hits | recorded | target_matches | pending | 관찰된 receiver |
| --- | --- | --- | --- | --- | --- | --- |
| `20260903-114501-799` | `child_exit` | 2 | 2 | 0 | 0 | `0x00946d50`, `0x00947220` |
| `20260903-114540-170` | `child_exit` | 2 | 2 | 0 | 0 | `0x00946d50`, `0x00947220` |

두 실행 모두 `code=0xc0000005`로 종료했습니다.

## 판정

- **확인됨 — 관찰 한계는 idle 경계였습니다.** 같은 바이너리와 같은 옵션에서 경계만 60초로 늘리면 이전 세션의 실행 순서와 fault가 그대로 재현됩니다. 따라서 Task 154의 "AV 미재현" 항목은 실행 환경 차이가 아니라 5초 경계에 의한 관찰 절단으로 정정합니다. child가 정지한 것이 아니라, 이 호스트에서 그래픽·오디오 초기화 구간이 5초보다 오래 debug event 없이 진행됐습니다.
- **확인됨 — 후보는 fault 시점까지 target field를 쓰지 않습니다.** 확장 경계에서 `child_exit`까지 관찰한 두 실행 모두 `target_matches=0`이었습니다. Task 154의 판정이 이제 첫 field access와 AV를 포함한 전체 구간에 대해 확인됩니다.
- **확인됨 — 기본 동작은 유지됩니다.** 기본값 `5000`을 쓰면 경계와 boundary event가 기존과 같으므로, 기존 로그의 해석은 바뀌지 않습니다.
- **미확정 — field 초기화 경로.** 어떤 경로가 `0x00acd708 + 0x11c`를 채워야 하는지는 여전히 미확정입니다. 직접 주입과 Hardlock 응답 변경은 계속 보류합니다.

## 다음 단계

1. target object `0x00acd708`의 다른 field가 초기화되었는지 확인해, 객체 전체가 미초기화인지 이 field만 비어 있는지 구분합니다.
2. field read anchor `0x0041a699`를 포함하는 함수의 호출자 경로를 역추적해, 초기화 분기가 건너뛰어지는 조건을 찾습니다.
3. 이후 모든 `ez2dj4th` 진단 실행은 확장된 idle 경계를 명시적으로 지정합니다.

원본 CHD/HDD/EXE 내용과 Hardlock secret material은 이 문서와 저장소에 기록하지 않았습니다.

---

# Task 155: Bounded Diagnostic Idle-Timeout Configuration Work Log

## Result summary

The launcher probe's fixed five-second idle boundary in the bounded diagnostic loop is now configurable through `--diagnostic-idle-timeout <milliseconds>`. The default remains `5000`, matching previous behavior.

The option removed the observation limit found in Task 154. With the boundary raised to 60 seconds, the execution order recorded by the previous session (object-source boundary → target match → field access → `0x00434137 / 0xc0000005` AV) reproduces in this environment, and the trace ends at `child_exit` rather than `idle_timeout`. Task 154's note that the AV "does not reproduce in this environment" was therefore an **observation limit created by the fixed idle boundary**, not an execution difference.

Re-running Task 154's candidate execution trace with the same extended boundary produced two candidate hits over the complete interval up to the AV, still with zero `target_matches`. The conclusion that none of the four `+0x11c` write candidates writes `0x00acd824` now holds for the whole interval up to the fault.

## Changes

- Added the `--diagnostic-idle-timeout <milliseconds>` option and usage text.
- Default `5000` with an accepted range of `1000`–`600000`; unparsable or out-of-range values print usage and fail.
- The value is passed to `WaitForExitProcessBreakpoint` and applied only to the bounded diagnostic loop's `WaitForDebugEvent` wait. Constants in the initial-breakpoint wait, unload-tail collection, and other wait paths are unchanged.
- The `launch` diagnostic event now records `diagnostic_idle_timeout_ms` so the observation boundary can be reconstructed from the log alone.

## Verification evidence

- Full Windows x86 Debug build: passed.
- `build/windows-x86/bin/Debug/re2dj_unit_tests.exe`: `checks: 1184, failures: 0`
- Range validation: `--diagnostic-idle-timeout 100` prints usage and does not launch.

### Extended-boundary baseline run

`logs/windows_x86_launcher_probe/ez2dj4th/20260903-114428-322.jsonl` (`diagnostic_idle_timeout_ms=60000`, same option set as Tasks 150–152)

- `null_context_object_source_trace_boundary`: `reason=child_exit`, `boundary_hits=1`, `hits=2`, `target_matches=1`, `code=0xc0000005`
- `null_context_field_access_trace_boundary`: `reason=child_exit`, `hits=1`, `code=0xc0000005`
- Field-access hit: `eip_after=0x0041a69f`, `ECX=0x00000000`
- Fault: two `0xc0000005` events at `0x00434137`, reference address `0x00000014`

### Extended-boundary candidate execution trace

`20260903-114501-799.jsonl` and the reproduction run `20260903-114540-170.jsonl` (`--null-context-field-reference-execution-trace`, `diagnostic_idle_timeout_ms=60000`)

| Run | boundary reason | hits | recorded | target_matches | pending | observed receivers |
| --- | --- | --- | --- | --- | --- | --- |
| `20260903-114501-799` | `child_exit` | 2 | 2 | 0 | 0 | `0x00946d50`, `0x00947220` |
| `20260903-114540-170` | `child_exit` | 2 | 2 | 0 | 0 | `0x00946d50`, `0x00947220` |

Both runs ended with `code=0xc0000005`.

## Classification

* **Confirmed — the observation limit was the idle boundary.** With the same binary and options, raising only the boundary to 60 seconds reproduces the previous session's execution order and fault. Task 154's "AV does not reproduce" item is therefore corrected to an observation truncation caused by the five-second boundary. The child had not stopped; on this host the graphics and audio initialization interval simply runs longer than five seconds without producing a debug event.
* **Confirmed — the candidates do not write the target field before the fault.** Both extended-boundary runs observed through `child_exit` reported `target_matches=0`. Task 154's classification now holds for the complete interval including the first field access and the AV.
* **Confirmed — default behavior is preserved.** With the default `5000`, boundaries and boundary events match previous runs, so existing logs keep their interpretation.
* **Unresolved — field-initialization path.** Which path should populate `0x00acd708 + 0x11c` is still unknown. Direct injection and Hardlock-response changes remain deferred.

## Next steps

1. Check whether other fields of target object `0x00acd708` are initialized, to distinguish an entirely uninitialized object from a single empty field.
2. Backtrack the caller path of the function containing field-read anchor `0x0041a699` to find the condition that skips the initialization branch.
3. Specify the extended idle boundary explicitly for all further `ez2dj4th` diagnostic runs.

No original CHD/HDD/EXE content or Hardlock secret material was recorded in this document or the repository.
