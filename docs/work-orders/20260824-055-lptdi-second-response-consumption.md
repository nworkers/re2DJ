# LPTDI 두 번째 응답 소비 탐색 작업 지시

관련 설계: [LPTDI 두 번째 응답 소비 탐색](../design/20260824-055-lptdi-second-response-consumption.md)

## 목표

`0x9c406414`에 `TRUE`와 통제된 104바이트 output을 주입하고, guest가 처음 읽는 offset과 분기 결과를 반복 추적으로 확인한다.

## 작업 범위

1. 첫 response는 8바이트 zero로 고정한다.
2. 두 번째 all-zero와 first-DWORD-one profile을 임시 build 경로에 만든다.
3. 각 profile을 최소 두 번 256~512 step 범위에서 실행한다.
4. 두 번째 IOCTL의 entry/return, output memory reference, 최초 분기 차이와 access violation을 비교한다.
5. 필요할 때만 tracer diagnostic을 보강하고 build/CTest를 수행한다.
6. 결과를 analysis, TODO, 설계와 작업 로그에 반영하고 커밋한다.

## 완료 조건

두 profile이 반복 가능한 제어 흐름 분류를 만들고 두 번째 output의 최초 소비 offset 또는 더 상위 성공 계약을 확인해야 한다. 임시 response 파일은 저장소에 남기지 않는다.

## 완료

- all-zero profile 두 실행과 first-DWORD-one canonical 두 실행을 완료했다.
- offset 0의 통과 조건과 offset 4~11의 8바이트 변환 loop를 확인했다.
- all-zero profile이 initializer AV 주소와 `.data` window를 실행별 challenge에 따라 바꾸는 것을 확인했다.
- first-DWORD-one은 변환 loop를 건너뛰고 private-page #UD로 끝나는 것을 확인했다.
- 소스 변경이나 tracer 보강은 필요하지 않았고 임시 profile은 제거했다.

---

# LPTDI Second-Response Consumption Work Order

Related design: [LPTDI Second-Response Consumption](../design/20260824-055-lptdi-second-response-consumption.md)

## Goal

Inject TRUE and controlled 104-byte output for 0x9c406414, then repeatedly identify the first guest-read offset and resulting branch.

## Scope

Hold the first response at eight zero bytes, create temporary all-zero and first-DWORD-one second responses, run each at least twice with 256–512 steps, compare entry/return, memory references, first branch divergence, and access violations, extend diagnostics only if required, update cumulative documents, and commit.

## Completion criteria

Both profiles must produce repeatable classifications and reveal either the first consumed second-output offset or a higher-level success contract. Temporary response files must not remain in the repository.

## Completion

- Completed two all-zero runs and two canonical first-DWORD-one runs.
- Confirmed the offset-zero advance condition and the eight-byte transformation loop over offsets 4 through 11.
- Confirmed that the all-zero profile changes the initializer AV address and `.data` window according to the per-run challenge.
- Confirmed that first-DWORD-one skips the transformation loop and ends in private-page #UD.
- No source change or tracer extension was required, and the temporary profiles were removed.
