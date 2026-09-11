# 작업 로그: Hardlock 요청 단위 payload 가산(add) 연산 지원

## 한국어

### 관련 문서

- 설계: [Hardlock 요청 단위 payload 가산(add) 연산 지원](../design/20260912-250-hardlock-payload-add-support.md)
- 작업 지시: [Hardlock 요청 단위 payload 가산(add) 연산 지원](../work-orders/20260912-250-hardlock-payload-add-support.md)
- 선행: [ez2d2m API_CODE payload의 블록별 역할 확인](20260911-249-ez2d2m-api-code-payload-probe.md)

### 작업 요약

1. `HardlockPayloadResponseEntry`에 32비트 little-endian 가산 필드(`output_add`, `output_add_mask`)를 추가했습니다.
2. `ValidateHardlockPayloadResponse`를 구현하여 동일 바이트에 대한 쓰기/가산 중복 지정 방지 및 4바이트 단위의 완전한 그룹 지정 여부를 검증하도록 했습니다.
3. `ApplyHardlockPayloadResponse`에서 가산 필드를 32비트 little-endian 덧셈(올림수 반영)으로 계산하여 게스트 버퍼에 반영하도록 구현했습니다.
4. 주입 런타임 공유 레코드 크기(`kHardlockPayloadRecordSize`)를 확장하고 `PackHardlockPayloadResponse`와 `UnpackHardlockPayloadResponse`에 반영했습니다.
5. `tests/unit/hardlock_payload_responses_test.cpp`에 가산 계산, 배타성 검증, 불완전 그룹 거절, 직렬화 라운드트립에 관한 단위 테스트를 추가했습니다.

### 검증 결과

| 빌드 구성 | 대상 | 결과 |
| --- | --- | --- |
| Windows x86 Debug | `re2dj_unit_tests.exe` | 1649개 검사 모두 통과 |
| Windows x86 Release | `re2dj_unit_tests.exe` | 1649개 검사 모두 통과 |

---

## English

### Related Documents

- Design: [Hardlock request-level payload add operation support](../design/20260912-250-hardlock-payload-add-support.md)
- Work Order: [Hardlock request-level payload add operation support](../work-orders/20260912-250-hardlock-payload-add-support.md)
- Preceding: [Probe block roles in ez2d2m API_CODE payload](20260911-249-ez2d2m-api-code-payload-probe.md)

### Summary of Work

1. Added 32-bit little-endian addition fields (`output_add`, `output_add_mask`) to `HardlockPayloadResponseEntry`.
2. Implemented `ValidateHardlockPayloadResponse` to verify that no byte is both written and added, and that all add groups are fully specified as 4-byte words.
3. Implemented 32-bit little-endian addition with carry in `ApplyHardlockPayloadResponse`.
4. Expanded `kHardlockPayloadRecordSize` and updated `PackHardlockPayloadResponse` and `UnpackHardlockPayloadResponse` to serialize/deserialize add fields.
5. Added unit tests in `tests/unit/hardlock_payload_responses_test.cpp` covering addition math, mutual exclusion validation, partial group rejection, and record roundtrip.

### Verification Results

| Configuration | Target | Result |
| --- | --- | --- |
| Windows x86 Debug | `re2dj_unit_tests.exe` | 1649 checks passed |
| Windows x86 Release | `re2dj_unit_tests.exe` | 1649 checks passed |
