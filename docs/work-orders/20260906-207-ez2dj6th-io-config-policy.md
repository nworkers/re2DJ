# ez2dj6th I/O 설정 전달 오류 수정 작업 지시서

## 작업 목표

6th CHD 실행에서 1st/4th 전용 `--io-config`가 child-follow launcher까지 전달되어
Usage로 종료되는 오류를 수정한다.

## 범위

* CHD 및 directory-backed 제품 CLI의 profile capability 기반 `io_config` 정리
* Windows original-process backend의 인자 검증 강화
* 6th 실행 정책과 검증 결과 문서화

## 구현 순서

1. CHD 실행 경로에서 legacy I/O가 없는 profile의 `io_config`를 제거하고 안내한다.
2. directory-backed 실행 경로에도 같은 정책을 적용해 두 입력 방식의 동작을 맞춘다.
3. backend가 capability와 맞지 않는 직접 호출을 오류로 거부하도록 한다.
4. Windows Debug 빌드와 unit test를 실행한다.
5. 작업 로그에 원인, 변경, 검증을 기록한다.

## 완료 조건

* `ez2dj6th --io-config <path>`가 launcher Usage로 즉시 종료되지 않는다.
* 6th에는 `--io-config`가 전달되지 않는다.
* legacy I/O가 활성화된 기존 profile에는 설정 경로가 계속 전달된다.
* 빌드와 unit test가 통과한다.

---

# Work Order: Fix ez2dj6th I/O Configuration Forwarding

## Objective

Fix the 6th CHD launch failure caused by forwarding the 1st/4th-only
`--io-config` option into the child-follow launcher, which exits with Usage.

## Scope

* Normalize `io_config` by profile capability in both CHD and directory-backed
  product CLI paths.
* Strengthen Windows original-process backend validation.
* Document the 6th execution policy and verification.

## Implementation Order

1. Remove and report unsupported `io_config` in the CHD path.
2. Apply the same policy to directory-backed execution.
3. Reject capability-incompatible direct backend calls.
4. Run the Windows Debug build and unit tests.
5. Record the cause, changes, and verification in the work log.

## Completion Criteria

* `ez2dj6th --io-config <path>` does not exit immediately with launcher Usage.
* The option is not forwarded to the 6th launcher.
* Existing legacy-I/O profiles continue to receive the configuration path.
* The build and unit tests pass.
