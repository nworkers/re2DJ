# 작업 지시 345: spdlog 런타임 로깅 표준화 / Work order 345: spdlog runtime logging standardization

설계: [spdlog 런타임 로깅 표준화](../design/20260921-345-spdlog-runtime-logging.md)

*Design: [spdlog runtime logging standardization](../design/20260921-345-spdlog-runtime-logging.md)*

## 구현 순서 / Implementation sequence

1. `spdlog` v1.14.1의 package-first/FetchContent fallback과 third-party notice를 추가합니다.
   *Add package-first discovery with an `spdlog` v1.14.1 FetchContent fallback and a third-party notice.*
2. 공용 `re2dj_logging` library에 stderr/file multi-sink, 공통 pattern, 모든 메시지 immediate flush, per-run path 생성과 fatal helper를 구현합니다.
   *Implement stderr/file multi-sinks, the shared pattern, immediate flush for every message, per-run path generation, and a fatal helper in a shared `re2dj_logging` library.*
3. 제품 CLI 시작 시 logger를 초기화하고 직접 stderr 진단을 level별 호출로 바꿉니다. help/version/표 형태 stdout은 유지합니다.
   *Initialize the logger at product-CLI startup and replace direct-stderr diagnostics with level-specific calls while preserving help/version/table stdout.*
4. `ImportDispatcher`의 unknown binding을 `HLE_UNIMPLEMENTED` fatal로 기록합니다.
   *Record unknown `ImportDispatcher` bindings as `HLE_UNIMPLEMENTED` fatal.*
5. Windows injected runtime의 미구현 graphics HLE 전용 trace도 `FATAL HLE_UNIMPLEMENTED`로 분류합니다.
   *Classify dedicated Windows injected-runtime traces for unimplemented graphics HLE as `FATAL HLE_UNIMPLEMENTED`.*
6. logger file/format/fatal 단위 테스트를 추가합니다.
   *Add unit tests for logger files, format, and fatal severity.*
7. README, architecture, KB, TODO와 작업 로그를 갱신하고 영향받는 build/test를 검증합니다.
   *Update README, architecture, KB, TODO, and the work log, then validate affected builds/tests.*

## 완료 조건 / Completion criteria

- 제품 CLI를 시작하면 첫 host 진단이 즉시 stderr에 출력되고 `logs/` 아래 실행별 파일 경로가 함께 기록됩니다.
  *Starting the product CLI immediately emits the first host diagnostic to stderr and records the per-run file path under `logs/`.*
- logger를 통한 모든 메시지는 stderr와 파일에 같은 pattern/level/message로 남고 즉시 flush됩니다.
  *Every logger message reaches stderr and the file with the same pattern/level/message and is flushed immediately.*
- 미구현 import와 지원되지 않는 실행 경계는 `critical` 레벨과 `FATAL` marker를 갖습니다.
  *Unimplemented imports and unsupported execution boundaries carry `critical` severity and a `FATAL` marker.*
- 기존 구조화 분석 로그와 stdout CLI 결과의 계약은 유지됩니다.
  *Existing structured analysis logs and stdout CLI-result contracts remain intact.*
- 관련 Linux x64/x86 및 Windows build와 단위 테스트가 통과하거나 불가능한 이유가 작업 로그에 기록됩니다.
  *Relevant Linux x64/x86 and Windows builds plus unit tests pass, or any impossibility is recorded in the work log.*
