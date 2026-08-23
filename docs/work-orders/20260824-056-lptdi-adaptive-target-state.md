# LPTDI 적응형 target-state 응답 작업 지시

관련 설계: [LPTDI 적응형 target-state 응답](../design/20260824-056-lptdi-adaptive-target-state.md)

## 목표

실행별 `0x9c406414` seed에 적응해 지정한 8바이트 guest 내부 상태를 만드는 진단용 응답 정책을 구현하고, 상태와 `.data` 복원 결과의 결정적 관계를 확인한다.

## 작업 범위

1. 확인된 `0x01ed4141` 변환과 8바이트 mask encoding을 플랫폼 공용 device 모듈로 구현한다.
2. 정확히 16자리 hex target state parser와 단위 테스트를 추가한다.
3. Windows injected runtime에 적응형 IOCTL mode와 8바이트 target-state export를 추가한다.
4. launcher에 `--device-mock-lptdi-target-state <16-hex-digits>`를 추가하고 기존 IOCTL 정책과 상호 배타적으로 검증한다.
5. runtime probe에서 410 zero response와 414 adaptive response 계약을 검증한다.
6. Windows x86 build와 CTest를 수행한다.
7. zero target state를 최소 두 번 canonical 실행해 challenge, AV, `.data` window를 비교한다.
8. 누적 분석, TODO, 구현 완료, 아키텍처와 작업 로그를 갱신하고 커밋한다.

## 완료 조건

서로 다른 seed에 대해 guest가 받는 response는 달라도 계산된 내부 target state가 같아야 한다. 두 canonical 실행이 같은 `.data` window와 종료 분류를 재현하거나, 그렇지 않다면 target state 외의 입력이 결과에 관여한다는 반복 증거를 남겨야 한다.

## 완료

- 공용 변환·encoding·hex parsing과 단위 테스트를 구현했다.
- Windows injected runtime mode 4와 launcher target-state 옵션을 구현했다.
- Windows x86 build와 CTest 2/2, x64 build와 CTest 1/1이 통과했다.
- 서로 다른 두 seed의 wire response가 모두 zero guest state를 만들었다.
- 두 실행이 동일한 AV와 `.data` window를 재현해 완료 조건을 충족했다.

---

# LPTDI Adaptive Target-State Response Work Order

Related design: [LPTDI Adaptive Target-State Response](../design/20260824-056-lptdi-adaptive-target-state.md)

## Goal

Implement a diagnostic response policy that adapts to each 0x9c406414 seed to produce a selected eight-byte guest internal state, then test whether that state deterministically controls `.data` restoration.

## Scope

Implement the confirmed 0x01ed4141 transform and eight-byte mask encoder in the shared device module; add an exact 16-hex-digit parser and unit tests; add an adaptive IOCTL mode and target-state export to the Windows injected runtime; add the mutually exclusive `--device-mock-lptdi-target-state <16-hex-digits>` launcher option; cover the 410 zero and 414 adaptive contracts in the runtime probe; run the Windows x86 build and CTest; execute zero target state at least twice against the canonical executable; update cumulative documentation; and commit.

## Completion criteria

Different seeds must produce different wire responses but the same computed guest target state. Two canonical runs must either reproduce the same `.data` window and termination classification or provide repeatable evidence that an input beyond the target state affects the result.

## Completion

- Implemented the shared transform, encoding, hex parser, and unit tests.
- Implemented Windows injected-runtime mode 4 and the launcher target-state option.
- Passed the Windows x86 build and CTest 2/2, plus the x64 build and CTest 1/1.
- Different wire responses for two seeds both produced a zero guest state.
- Both runs reproduced the same AV and `.data` window, satisfying the completion criteria.
