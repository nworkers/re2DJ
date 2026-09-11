# 작업 로그: 클린룸 Hardlock 엔진 통합 및 동적 HLE 구현

## 한국어

### 관련 문서

- 설계: [클린룸 Hardlock 엔진 통합 및 동적 HLE 구현 설계](../design/20260912-251-hardlock-cleanroom-engine-integration.md)
- 작업 지시: [클린룸 Hardlock 엔진 통합 및 동적 HLE 구현](../work-orders/20260912-251-hardlock-cleanroom-engine-integration.md)
- 선행 작업: `reSoftlock` 클린룸 암호화 엔진 C++20 포팅 및 BSD 3-Clause 전환

### 작업 요약

1. **클린룸 C++20 HardlockEngine 도입**:
   - `include/re2dj/hle/hardlock/engine.h` 및 `src/hle/hardlock/engine.cpp`를 작성했습니다.
   - 외부 의존성 없이 표준 C++20만으로 8바이트 블록 암호화(`CryptBlock` / `EncryptBlock`) 및 56바이트 `API_CODE` 페이로드 계산(`CodePayload` / `CodePayloadInPlace`)을 구현했습니다.
2. **HardlockDevice 동적 분기 구현**:
   - `include/re2dj/hle/hardlock/device.h`에 `HardlockSeeds` 구조체를 정의하고 `HardlockDeviceOptions::seeds` 필드를 추가했습니다.
   - `src/hle/hardlock/device.cpp`의 `Complete` 메서드에서 `seeds`가 설정되어 있을 경우:
     - 0x0009(`API_CODE`) 요청 시 `HardlockEngine::CodePayloadInPlace`로 56바이트 페이로드 동적 계산.
     - 0x000e/0x0011(Transform) 요청 시 각 8바이트 블록에 대해 `HardlockEngine::EncryptBlock`으로 동적 변환 수행.
     - `seeds`가 지정되지 않은 경우 기존 정적 매핑 테이블로 안전하게 fallback되도록 구현했습니다.
3. **시드 검증 및 INI 설정 유틸리티 구현**:
   - `include/re2dj/hle/hardlock/seed_config.h` 및 `src/hle/hardlock/seed_config.cpp`를 작성했습니다.
   - `ReadHardlockSeedIni` / `WriteHardlockSeedIni`: `[profile]` 섹션 기반의 seed INI 파일 파싱 및 생성 지원.
   - `ConfirmSeedsFromAnalysisArtifact`: `reSoftlock`의 분석 아티팩트(`hardlock_analysis.json`)를 읽어 후보 시드들 중 `id_ref` -> `id_verify` 변환이 유효한 시드셋을 기계적으로 판별 및 확정하는 로직 구현.
4. **단위 테스트 작성 및 CMake 연동**:
   - `tests/unit/hardlock_engine_test.cpp`에 엔진 기본 연산, 동적 변환, 페이로드 계산, INI 및 아티팩트 판별 테스트(`TestEngineBasics`, `TestDynamicDeviceTransform`, `TestDynamicDevicePayload`, `TestSeedConfigAndArtifact`)를 작성했습니다.
   - `CMakeLists.txt`에 `engine.cpp`와 `seed_config.cpp`를 `re2dj_core`와 `re2dj_windows_injected_runtime`에 추가했습니다.

### 검증 결과

| 빌드 구성 | 대상 | 결과 |
| --- | --- | --- |
| Windows x86 Debug | `re2dj_unit_tests.exe` | 1673개 검사 모두 통과 (신규 24개 검사 통과) |

---

## English

### Related Documents

- Design: [Clean-room Hardlock Engine Integration and Dynamic HLE Design](../design/20260912-251-hardlock-cleanroom-engine-integration.md)
- Work Order: [Clean-room Hardlock Engine Integration and Dynamic HLE Work Order](../work-orders/20260912-251-hardlock-cleanroom-engine-integration.md)
- Preceding Work: `reSoftlock` clean-room crypto engine C++20 port and BSD 3-Clause re-licensing

### Summary of Work

1. **Clean-room C++20 HardlockEngine Integration**:
   - Implemented `include/re2dj/hle/hardlock/engine.h` and `src/hle/hardlock/engine.cpp`.
   - Provided self-contained C++20 implementation of 8-byte block encryption (`CryptBlock` / `EncryptBlock`) and 56-byte `API_CODE` payload computation (`CodePayload` / `CodePayloadInPlace`) without external dependencies.
2. **Dynamic Dispatch in HardlockDevice**:
   - Defined `HardlockSeeds` struct in `include/re2dj/hle/hardlock/device.h` and added `HardlockDeviceOptions::seeds`.
   - In `src/hle/hardlock/device.cpp` (`Complete`):
     - Dynamically computes 56-byte payload using `HardlockEngine::CodePayloadInPlace` for 0x0009 (`API_CODE`).
     - Dynamically computes block-by-block encryption using `HardlockEngine::EncryptBlock` for 0x000e/0x0011 (Transform).
     - Safely falls back to existing static mapping tables if `seeds` is not configured.
3. **Seed Verification and INI Utilities**:
   - Implemented `include/re2dj/hle/hardlock/seed_config.h` and `src/hle/hardlock/seed_config.cpp`.
   - `ReadHardlockSeedIni` / `WriteHardlockSeedIni`: Supports parsing and emitting `[profile]` based seed INI files.
   - `ConfirmSeedsFromAnalysisArtifact`: Parses `reSoftlock`'s analysis artifact (`hardlock_analysis.json`) and validates seed candidates against `id_ref` -> `id_verify` transformation.
4. **Unit Tests and CMake Integration**:
   - Created `tests/unit/hardlock_engine_test.cpp` covering engine basics, dynamic device transform, payload processing, and seed config/artifact confirmation (`TestEngineBasics`, `TestDynamicDeviceTransform`, `TestDynamicDevicePayload`, `TestSeedConfigAndArtifact`).
   - Updated `CMakeLists.txt` to include `engine.cpp` and `seed_config.cpp` in `re2dj_core` and `re2dj_windows_injected_runtime`.

### Verification Results

| Configuration | Target | Result |
| --- | --- | --- |
| Windows x86 Debug | `re2dj_unit_tests.exe` | 1673 checks passed (24 new checks passed) |
