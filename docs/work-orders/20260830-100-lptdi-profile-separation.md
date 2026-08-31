# LPTDI 프로파일 분리 작업 지시

## 목적

`ez2dj3rd` 실행과 1st SE 정책 강제 적용 결과를 바탕으로 LPTDI·legacy raw I/O 실행 정책을 프로파일별로 분리하고, 확인되지 않은 3rd 장치 계약에 1st 설정이 전달되지 않게 한다.

*Based on the 3rd execution and the forced 1st-policy comparison, separate LPTDI and legacy raw-I/O execution policy per profile so that unconfirmed 3rd device behavior never receives 1st settings.*

## 작업 범위

1. [LPTDI 프로파일 분리 설계](../design/20260830-100-lptdi-profile-separation.md)의 근거와 구조를 반영해 `TargetLptdiPolicy`를 추가한다.
2. 1st SE의 raw I/O, device mock, target state를 전용 정책으로 이동하고 3rd는 비활성·빈 값으로 명시한다.
3. Windows original-process backend와 launcher가 중첩 LPTDI 정책을 사용하도록 갱신한다.
4. launcher가 3rd에 `--device-mock-lptdi*`를 적용하려는 경우 import 패치 단계까지 진행하지 않고 프로파일 정책 오류로 거절하도록 한다.
5. target-profile 및 product-loader 테스트를 갱신한다.
6. Windows x86 Debug build/CTest, 3rd shortcut 실행, 3rd 강제 LPTDI 거부를 검증한다.
7. 관련 analysis, `ARCHITECTURE.md`, 작업 로그를 갱신하고 커밋한다.

*Scope: add `TargetLptdiPolicy` from the linked design; move 1st SE raw I/O, device mock, and target state into it while explicitly disabling and clearing 3rd; update the Windows original-process backend and launcher; reject `--device-mock-lptdi*` for 3rd at profile-policy validation before import patching; update target-profile and product-loader tests; run the Windows x86 Debug build/CTest, 3rd shortcut, and forced-LPTDI rejection; update analysis, `ARCHITECTURE.md`, and the work log; and commit.*

## 제외 범위

- 3rd LPTDI 응답 알고리즘 또는 물리 동글 에뮬레이션
- 3rd `UseIOCard`의 의미를 확인되지 않은 상태에서 raw I/O로 단정
- 원본 HDD·실행 파일의 저장소 복사 또는 수정
- 게임 로직 재구현

*Excluded: 3rd LPTDI response algorithms or physical-dongle emulation; treating 3rd `UseIOCard` as raw I/O without evidence; copying or modifying original HDD/executable contents; and reimplementing gameplay.*

## 완료 조건

- 1st와 3rd의 LPTDI 정책 필드가 코드·테스트에서 분리되어 있다.
- 3rd 프로파일 기본 실행 인자에 `--hle-io-ports`와 LPTDI target state가 없다.
- 3rd에 1st 전용 device mock을 명시해도 프로파일 정책 오류로 종료한다.
- Windows x86 Debug build와 CTest가 통과한다.
- 실제 3rd 실행 및 검증 로그와 남은 미확정 사항이 analysis/work log에 기록된다.

*Completion requires separate 1st/3rd LPTDI policy fields in code and tests; no `--hle-io-ports` or LPTDI target state in 3rd’s profile-derived arguments; an explicit 1st-only device mock on 3rd to terminate with a profile-policy error; passing Windows x86 Debug build and CTest; and recording the real 3rd run, verification logs, and remaining unknowns in analysis/work log.*
