# LPTDI DeviceIoControl 반환 추적 작업 지시

관련 설계: [LPTDI DeviceIoControl 반환 추적](../design/20260824-050-device-io-control-return-trace.md)

## 목표

canonical mock-on 경로의 두 `DeviceIoControl` 호출이 실제로 어떤 buffer와 반환 결과를 주고받는지 비침투적으로 기록한다.

## 작업 범위

1. API watch metadata에 인자 수를 추가하고 DeviceIoControl의 8개 인자를 기록한다.
2. 최대 64바이트 input/output pre-snapshot과 bytes-returned 초기값을 기록한다.
3. thread별 one-shot return breakpoint로 EAX, bytes-returned, output post-snapshot을 기록한다.
4. build·CTest와 mock-on 2회 비교를 수행한다.
5. 결과를 설계, 누적 analysis, TODO, 작업 로그에 반영하고 커밋한다.

## 해석 경계

host API 실패와 buffer 무변화가 확인되어도 그것만으로 특정 정상 응답 값을 알 수는 없다. 합성 응답 구현 전에 caller가 결과를 어떻게 사용하는지 추가 근거가 필요할 수 있다.

## 검증

두 실행에서 IOCTL별 entry/return record가 완결되고 guest의 기존 AV 결과가 관찰되는지로 판정한다.

## 완료 상태

범위 1~5를 완료했다. 두 실행에서 동일한 호출 형식과 실패·buffer 무변화를 확인했고, 입력 challenge는 실행별로 변했다. 최소 응답 합성은 별도 후속 작업으로 분리했다. 결과는 [작업 로그 050](../work-logs/20260824-050-device-io-control-return-trace.md)에 있다.

---

# LPTDI DeviceIoControl Return Trace Work Order

Related design: [LPTDI DeviceIoControl Return Trace](../design/20260824-050-device-io-control-return-trace.md)

## Goal

Non-intrusively record the buffers and return results of both DeviceIoControl calls on the canonical mock-on path.

## Scope

Add API argument-count metadata, eight-argument and bounded-buffer entry capture, a per-thread one-shot return breakpoint for EAX/bytes-returned/post-buffer capture, build and CTest, two comparison runs, cumulative documentation, and a commit.

## Interpretation boundary

Host failure and unchanged buffers do not reveal the correct synthetic response by themselves. Caller result-use evidence may still be required before response HLE.

## Verification

Both runs must contain complete entry/return records for each IOCTL while retaining the existing guest AV outcome.

## Completion status

Scope items 1–5 are complete. Both runs show the same call shapes and failure-without-buffer-change, while challenge input varies by run. Minimal response synthesis is separated into follow-up work. See [work log 050](../work-logs/20260824-050-device-io-control-return-trace.md).
