# 작업 지시: Hardlock 요청 단위 payload 가산(add) 연산 지원

## 한국어

### 작업 범위

1. `include/re2dj/hle/hardlock/payload_responses.h`:
   - `kHardlockPayloadAddGroup = 4` 상수 정의.
   - `HardlockPayloadResponseEntry`에 `output_add`, `output_add_mask` 필드 추가.
   - `ValidateHardlockPayloadResponse` 검증 함수 선언.
   - `kHardlockPayloadRecordSize`를 `4 + kHardlockPayloadMaxBytes * 6`으로 확장.
2. `src/hle/hardlock/payload_responses.cpp`:
   - `WellFormed`에 add 필드 크기 일치 확인 추가.
   - `ValidateHardlockPayloadResponse` 구현 (쓰기/가산 배타성, 4바이트 정렬 및 완전 지정).
   - `ApplyHardlockPayloadResponse`에 32비트 little-endian 가산 처리 추가.
   - `PackHardlockPayloadResponse` 및 `UnpackHardlockPayloadResponse`에 가산 필드 직렬화/역직렬화 및 유효성 검사 연동.
3. `tests/unit/hardlock_payload_responses_test.cpp`:
   - 가산 연산, 유효성 검사 충돌 및 부분 지정 거부, 레코드 라운드트립 단위 테스트 추가.

### 검증 계획

- Windows x86 Debug/Release 빌드 및 단위 테스트(`re2dj_unit_tests.exe`) 실행.

---

## English

### Scope of Work

1. `include/re2dj/hle/hardlock/payload_responses.h`:
   - Define `kHardlockPayloadAddGroup = 4` constant.
   - Add `output_add` and `output_add_mask` fields to `HardlockPayloadResponseEntry`.
   - Declare `ValidateHardlockPayloadResponse` validation function.
   - Expand `kHardlockPayloadRecordSize` to `4 + kHardlockPayloadMaxBytes * 6`.
2. `src/hle/hardlock/payload_responses.cpp`:
   - Extend `WellFormed` to check add field sizes.
   - Implement `ValidateHardlockPayloadResponse` (write/add mutual exclusion, 4-byte alignment and full group specification).
   - Implement 32-bit little-endian addition in `ApplyHardlockPayloadResponse`.
   - Update `PackHardlockPayloadResponse` and `UnpackHardlockPayloadResponse` for add fields serialization and validation.
3. `tests/unit/hardlock_payload_responses_test.cpp`:
   - Add unit tests for payload additions, validation conflict/partial group rejections, and record roundtrips.

### Verification Plan

- Windows x86 Debug/Release build and execution of `re2dj_unit_tests.exe`.
