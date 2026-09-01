# 작업 118 — ez2dj4th 보호 stub bounded API trace

## 한국어

관련 설계: [ez2dj4th 보호 stub bounded API trace 설계](../design/20260901-118-ez2dj4th-bounded-api-trace.md)

### 작업 목표

정적 `ExitProcess` import가 없는 4th 보호 실행 파일에서도 기존 Windows x86
`--api-trace`가 entry 이후 Win32 API 호출을 제한된 event 범위에서 기록하도록
수정합니다. 목적은 첫 `CreateFileA` 이전의 실제 API 순서를 확인하는 것이며,
보호 응답을 추측하거나 게임 로직을 재구현하는 것이 아닙니다.

### 구현 범위

1. launcher의 API trace 대기 루프에 bounded completion 경계를 추가합니다.
2. `--api-trace`에서 `ExitProcess` import가 없을 때 exit breakpoint를 armed 상태로
   가장하지 않고 bounded mode를 선택합니다.
3. bounded event cap 또는 child 종료를 JSONL 진단 event로 기록합니다.
4. 기존 ExitProcess import가 있는 target의 exit-bound trace 동작을 유지합니다.
5. 4th static import surface와 실제 bounded trace 결과를 분석 문서에 기록합니다.
6. Debug x86 build, product-loader probe, 실제 CHD staging 실행 및 diff 검사를
   작업 로그에 남깁니다.

### 제외 범위

* Hardlock IOCTL 응답, seed solver, 장치 mock, 4th HLE profile 변경
* `Fat32Volume`, libchdr, CHD staging/VFS 파일 의미 변경
* 원본 실행 파일 또는 원본 자산의 저장소 추가
* 기존 1st/3rd HLE 동작 변경

### 완료 조건

* 4th에서 `--api-trace`가 준비 단계 오류 없이 API breakpoint를 설치합니다.
* JSONL에 watched API와 caller/인자 기록, bounded boundary가 남습니다.
* exit import가 있는 기존 target의 product-loader 및 관련 build가 통과합니다.
* 확인됨/미확정 상태가 `docs/analysis/`에 유지되고 작업 로그가 추가됩니다.

## English

Related design: [ez2dj4th bounded API-trace design](../design/20260901-118-ez2dj4th-bounded-api-trace.md)

### Goal

Make the existing Windows x86 `--api-trace` record post-entry Win32 calls for
the 4th protected executable even though its static imports have no
`ExitProcess`. The goal is to identify the real API order before the first
`CreateFileA`, not to guess a protection response or rewrite game logic.

### Scope

1. Add a bounded completion boundary to the launcher's API-trace wait loop.
2. Select bounded mode when `--api-trace` finds no `ExitProcess` import instead
   of pretending that an exit breakpoint is armed.
3. Record the event cap or child exit as a JSONL diagnostic event.
4. Preserve exit-bound tracing for targets with a static `ExitProcess` import.
5. Record the 4th static import surface and real bounded-trace result in analysis.
6. Record Debug x86 build, product-loader probe, real CHD-staging execution, and
   diff checks in the work log.

### Out of scope

Do not change Hardlock IOCTL responses, seed solving, device mocks, the 4th HLE
profile, `Fat32Volume`, libchdr, CHD staging/VFS semantics, original assets, or
existing 1st/3rd HLE behavior.

### Completion criteria

* 4th `--api-trace` installs API breakpoints without a preparation error.
* JSONL contains watched APIs with caller/arguments and a bounded boundary.
* Existing targets with an exit import pass the product-loader and related builds.
* Confirmed and unresolved status remains explicit in `docs/analysis/`, with a
  corresponding work log.
