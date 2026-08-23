# LPTDI 외부 응답 profile 작업 지시

관련 설계: [LPTDI 외부 응답 profile](../design/20260824-054-lptdi-external-response-profile.md)

## 목표

확인되지 않은 동글 값을 코드에 고정하지 않고, 외부 text profile의 code별 output을 합성 LPTDI `DeviceIoControl`에 주입해 첫 DWORD zero/nonzero 경로를 비교한다.

## 작업 범위

1. 공용 LPTDI response profile parser와 단위 테스트를 추가한다.
2. injected runtime에 code별 response export와 profile mode를 추가한다.
3. launcher에 `--device-mock-lptdi-response-profile <path>` 로딩·검증·원격 복사를 추가한다.
4. runtime probe에서 copy와 missing-code 계약을 검증한다.
5. Windows x86 build와 CTest를 수행한다.
6. first-DWORD 0/1 profile을 각 두 번 이상 실행하고 access violation과 제어 흐름을 비교한다.
7. 결과를 analysis, architecture, TODO와 작업 로그에 반영하고 커밋한다.

## 완료 조건

원본 자산 없이 parser/runtime 테스트가 통과하고, 잘못된 profile이 guest 실행 전에 거부되며, 0/1 profile별 반복 실행이 같은 흐름 분류를 보여야 한다.

## 완료

공용 versioned parser, runtime response slot/profile mode, launcher 원격 주입과 probe·단위 테스트를 구현했다. Windows x86 build와 CTest 2/2가 통과했다. 첫 DWORD 0/1 profile을 각각 두 번 실행해 0은 두 번째 IOCTL과 원본 `.text`/기존 initializer AV로, 1은 첫 IOCTL 3회와 private-page #UD로 반복 분리됨을 확인했다.

---

# LPTDI External Response Profile Work Order

Related design: [LPTDI External Response Profile](../design/20260824-054-lptdi-external-response-profile.md)

## Goal

Inject code-specific output from an external text profile into synthetic LPTDI DeviceIoControl and compare first-DWORD zero/nonzero paths without hard-coding an unconfirmed dongle value.

## Scope

Add a shared parser and unit tests, runtime response exports and profile mode, launcher loading/validation/remote copying, runtime-probe coverage, Windows x86 build/CTest, two or more runs for each zero/one profile, cumulative documentation, and a task commit.

## Completion criteria

Parser and runtime tests must pass without original assets, malformed profiles must fail before guest execution, and repeated zero/one profile runs must produce consistent control-flow classifications.

## Completion

Implemented the shared versioned parser, runtime response slots/profile mode, launcher remote injection, and probe/unit coverage. The Windows x86 build and CTest 2/2 passed. Two runs each showed that first-DWORD zero reaches the second IOCTL and original `.text`/known initializer AV, while one repeats the first IOCTL three times and selects private-page #UD.
