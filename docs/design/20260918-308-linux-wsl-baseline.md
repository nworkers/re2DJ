# Linux 기준선 빌드 복구 / Linux Baseline Build Repair

L0 재현 중 GCC 13의 `-Werror=range-loop-construct`가 FAT32 긴 파일명 slot의 읽기 전용 범위 표 복사를 거부했다. `AppendLongNameSlot`의 structured binding을 `const auto&`로 바꿔 불필요한 복사를 제거한다. 표의 수명은 함수 scope이며 offset/count 사용 의미는 바뀌지 않는다. 기존 FAT32 이름 테스트와 Linux x64/i386 build로 검증한다. 새 HLE나 제품 실행 경로는 추가하지 않는다.

*During L0 reproduction, GCC 13 rejected copies of read-only FAT32 long-name slot ranges under -Werror=range-loop-construct. Change the structured binding in AppendLongNameSlot to const auto&. The table lives for the function scope and offset/count semantics remain unchanged. Validate with existing FAT32 name tests and Linux x64/i386 builds. No new HLE or product execution path is introduced.*

기존 테스트 fixture의 `MakeLongNameSlot`에도 같은 읽기 전용 순회가 있어 동일하게 참조로 정리한다. 새 테스트를 추가하지 않고 기존 회귀를 실행한다.

*Apply the same reference binding to the existing MakeLongNameSlot test fixture. Run existing regressions without adding a test for this mechanical change.*

전체 x64 build에서는 CLI의 `PrepareChdStaging`과 `NormalizeIoConfigForProfile`이 Windows 분기에서만 호출되어 unused-function 오류가 발생했다. 정의에도 호출부와 같은 `_WIN32` 조건을 적용한다. 플랫폼 공용 정책을 새로 분리하는 작업은 L1 이후이며 현재 실행 의미는 바꾸지 않는다.

*The full x64 build reports unused-function errors for PrepareChdStaging and NormalizeIoConfigForProfile, whose callers are Windows-only. Guard their definitions with the same _WIN32 condition. Shared-policy extraction belongs to later work; runtime semantics are unchanged.*

GCC에서 `hardlock_engine_test.cpp`의 `std::memcpy`와 `std::copy_n` 선언이 누락되어 빌드가 중단되었다. `<cstring>`·`<algorithm>`과 사용하는 `<array>`를 직접 포함해 다른 헤더의 간접 include 의존성을 제거한다.

*GCC also found missing std::memcpy and std::copy_n declarations in hardlock_engine_test.cpp. Include cstring, algorithm, and the used array header directly instead of relying on transitive headers.*
