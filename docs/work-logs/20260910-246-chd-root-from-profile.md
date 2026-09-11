# 작업 로그: CHD 내부 디렉터리를 프로파일에서 받기, 그리고 종료 원인 정정

## 한국어

### 관련 문서

- 작업 지시: [CHD 내부 디렉터리를 프로파일에서 받기](../work-orders/20260910-246-chd-root-from-profile.md)
- 분석: [ez2d2m CHD 파일시스템과 실행 파일 관찰](../analysis/ez2d2m-chd-filesystem.md)의 종료 원인 정정 절
- 정정 대상: [ez2d2m 종료 원인 규명](20260910-245-ez2d2m-exit-cause.md)

### 요구사항

작업 245의 종료 원인 결론을 외부 지적을 참고해 다시 검토합니다.

### 정정

작업 245의 결론은 **틀렸습니다.**

| 항목 | 작업 245의 서술 | 실제 |
| --- | --- | --- |
| `ExitProcess(0)` | 보호 계층의 실패 종료 | **명시적 정상 종료** |
| Function `0x0001` | 종료를 결정하는 미구현 데이터 조회 | **API teardown**(`API_DOWN`), 종료 경로에 있을 뿐 |
| echo 응답 | 내용이 거절됨 | 근거 없음. 거부 실험은 "성공 여부를 신경 쓴다"만 보임 |

거부 실험이 재시도를 유발한 것은 사실이지만, 그것은 요청 실패 처리의 확인일 뿐 응답 내용의 거절을 뜻하지 않습니다. **"순서상 앞에 있다"와 "원인이다"를 충분히 갈라내지 못한 것이 오류의 원인입니다.** 시험을 설계할 때 한쪽 가설만 반증하고 다른 가설을 세우지 않았습니다.

### 관측을 가로막던 결함

원인을 다시 추적하면서 두 가지를 발견했습니다.

**첫째, 제 실행 방식이 프로파일과 달랐습니다.** probe 실행에 `--hle-d3d3`와 `--hle-directsound`를 주지 않아 프로파일이 켜는 두 경계가 꺼진 채였습니다. 다만 두 플래그를 켜고 재실행해도 결과는 256줄로 동일했으므로, 이 누락이 결론을 바꾸지는 않았습니다.

**둘째, 진짜 결함이 있었습니다.** 게스트가 자기 `EZ2Dancer.ini`를 열 때 `error=2`로 실패하고 있었습니다. runtime의 `ChdRelativePath`가 이미지 내부 디렉터리를 `"EZ2DJ/"`로 하드코딩하는데, `ez2d2m`은 `ez2dancer` 아래에 있어 CHD fallback이 언제나 빗나갔습니다. 이 저장소 첫 비-EZ2DJ 제품이 드러낸 가정입니다.

### 실제 종료 원인 — 확인됨

INI가 열리자 게스트가 한 걸음 더 갑니다. transform 요청이 11건에서 12건으로 늘고 **12번째가 `function=0x0011`, `block_count=7`** 입니다.

| 요청 | function | block_count | 매핑 |
| --- | --- | --- | --- |
| 1–11 | `0x000e` | 1 | 전부 매핑 |
| 12 | `0x0011` | 7 | **0 매핑 / 7 unmapped** |

**종료 원인은 응답할 수 없는 `function=0x0011` 7블록 transform입니다.** 뒤따르는 `0x0001` 두 건은 teardown이고 `ExitProcess(0)`은 그 결과입니다.

[6th 분석](../analysis/ez2dj6th-hardlock.md)은 6th의 미해결 항목을 같은 Function `0x0011` 응답으로, 그 입력을 7개로 기록합니다. 두 제품이 같은 미해결 요청에서 멈춥니다.

7블록 입력은 `cfg/ez2d2m/runtime-challenges-full.txt`에 받아 뒀습니다.

### 코드 변경

runtime에 CHD 내부 루트 전역을 추가하고 `ChdRelativePath`가 그것을 쓰게 했습니다. 기본값은 `EZ2DJ`라 기존 동작이 유지됩니다. launcher는 프로파일의 `executable_relative_path`에서 디렉터리 부분을 뽑아 전달합니다. `ez2dj3rd`는 `EZ2DJ/EZ2DJ.EXE`라 종전과 같은 값을 받습니다.

### 검증

| 항목 | 결과 |
| --- | --- |
| `ez2d2m` INI open | `stage=chd success=1` (이전 `stage=native success=0 error=2`) |
| `ez2d2m` 실행 | 261줄, transform 12건, 12번째가 7블록 unmapped |
| 추출본 실행과 CHD 실행 | 동일한 결과 |
| `ez2dj3rd` | 313줄·자산 0·crash 1로 수정 전과 동일 |
| Windows x86 Debug 전체 build | 통과, 경고 0건 |
| 단위 시험 | checks 1550, failures 0 |

### 남은 것

- `function=0x0011`의 유효한 7블록 응답. 이 저장소는 보호 응답을 추측해 만들지 않으므로 외부 도구나 실제 동글에서 와야 합니다.
- `0x0001` 응답이 teardown으로 충분한지. `0x0011`을 넘긴 뒤에 확인됩니다.
- `candidate-70`의 최종 검증. 여전히 `asset-open` 0건입니다.

## English

### Related documents

- Work order: [taking the image-internal directory from the profile](../work-orders/20260910-246-chd-root-from-profile.md)
- Analysis: the corrected exit-cause section of [ez2d2m CHD filesystem and executable observations](../analysis/ez2d2m-chd-filesystem.md)
- Corrects: [identifying the ez2d2m exit cause](20260910-245-ez2d2m-exit-cause.md)

### Requirement

Re-examine task 245's exit-cause conclusion against outside comment.

### The correction

Task 245's conclusion was **wrong**. `ExitProcess(0)` is a deliberate normal exit, not a protection failure exit. Function `0x0001` is the legacy Hardlock API's `API_DOWN` teardown, a step on the way out rather than the step that decides to leave. And there was no basis for saying the echo's content was rejected.

The refusal experiment did cause retries, but that only confirms the protection handles a failed request; it does not show its content was rejected. **The error was not separating "precedes the exit" from "causes it" firmly enough** — the test disproved one reading without proposing the other.

### What was hiding the cause

Two things surfaced while re-tracing. First, my runs did not match the profile: the probe was invoked without `--hle-d3d3` and `--hle-directsound`, so two boundaries the profile enables were off. Re-running with both changed nothing — still 256 lines — so this did not alter the conclusion.

Second, there was a real defect. The guest's `EZ2Dancer.ini` open was failing with `error=2`, because the runtime's `ChdRelativePath` hard-coded the image-internal directory as `"EZ2DJ/"` while `ez2d2m` lives under `ez2dancer`, so every CHD fallback missed — an assumption exposed by the first non-EZ2DJ product here.

### The real exit cause — confirmed

With the INI readable the guest goes one step further: transform requests rise from eleven to twelve, and **the twelfth is `function=0x0011` with `block_count=7`**. The first eleven are single-block `function=0x000e` and all map; all seven blocks of the twelfth are unmapped.

**The exit cause is that unanswerable seven-block `function=0x0011` transform.** The two `0x0001` requests after it are teardown, and `ExitProcess(0)` is the consequence.

[The 6th analysis](../analysis/ez2dj6th-hardlock.md) records 6th's unresolved item as the same Function `0x0011` response with seven inputs, so two products stop at the same request. The seven input blocks are captured in `cfg/ez2d2m/runtime-challenges-full.txt`.

### Code change

The runtime gained a global for the image-internal root, used by `ChdRelativePath` and defaulting to `EZ2DJ` so existing behaviour is unchanged, and the launcher derives it from the profile's `executable_relative_path`. `ez2dj3rd` is `EZ2DJ/EZ2DJ.EXE` and so receives the same value as before.

### Verification

`ez2d2m`'s INI open now reports `stage=chd success=1` where it previously reported `stage=native success=0 error=2`; the run reaches 261 lines with twelve transforms, the twelfth a seven-block unmapped request; running from the CHD and from the extracted tree give the same result; `ez2dj3rd` is unchanged at 313 lines, zero assets and one crash; the full Windows x86 Debug build passes with zero warnings; and unit tests report 1550 checks with 0 failures.

### What remains

A valid seven-block answer for `function=0x0011`, which must come from an external tool or a real dongle since this repository does not invent protection responses. Whether the `0x0001` answer suffices as teardown, checkable only past `0x0011`. And the final validation of `candidate-70`, since `asset-open` is still zero.
