# spdlog 런타임 로깅 / spdlog runtime logging

## sink와 logger / Sinks and loggers

spdlog logger는 하나 이상의 sink에 같은 log event를 전달할 수 있습니다. console sink와 file sink를 한 logger에 묶으면 호출 지점은 한 번만 format하고, 실시간 터미널 표시와 분석용 파일 보존을 동시에 제공할 수 있습니다. sink별 level과 pattern도 설정할 수 있습니다. [spdlog upstream](https://github.com/gabime/spdlog)

*An spdlog logger can send one log event to multiple sinks. Combining console and file sinks in one logger lets a call site format once while providing both live terminal output and an analysis file. Levels and patterns can also be configured per sink. [spdlog upstream](https://github.com/gabime/spdlog)*

## flush와 fatal / Flush and fatal

`flush_on(level)`은 지정 level 이상의 event 뒤에 sink를 flush합니다. 가장 낮은 사용 level로 설정하면 모든 메시지가 즉시 flush되어 crash 직전 로그 보존에 유리하지만, 고빈도 frame/packet trace에는 비용이 큽니다. 따라서 제품 상태 로그에는 immediate flush를 쓰고, 고빈도 구조화 trace에는 기존 bounded writer를 유지하는 것이 적합합니다.

*`flush_on(level)` flushes sinks after events at or above the selected level. Selecting the lowest used level makes every message immediately durable, which helps preserve pre-crash diagnostics but is expensive for high-frequency frame or packet traces. Immediate flush therefore fits product status logs, while existing bounded writers remain appropriate for high-frequency structured traces.*

spdlog의 최고 내장 level 이름은 `critical`입니다. 프로젝트가 사용자에게 `FATAL`이라는 안정된 의미를 보여야 한다면 `critical` severity와 메시지의 명시적 `FATAL` marker를 함께 사용해야 합니다. 로그 심각도와 프로세스 종료 정책은 별도 계약입니다.

*spdlog's highest built-in level name is `critical`. When a project needs a stable user-facing `FATAL` meaning, use critical severity together with an explicit `FATAL` marker in the message. Log severity and process-termination policy are separate contracts.*

## 라이선스 / License

spdlog는 MIT License로 배포됩니다. 고정 버전, upstream과 license 링크를 프로젝트 third-party notice에 기록해야 합니다. [spdlog license](https://github.com/gabime/spdlog/blob/v1.14.1/LICENSE)

*spdlog is distributed under the MIT License. Record the pinned version plus upstream and license links in the project's third-party notice. [spdlog license](https://github.com/gabime/spdlog/blob/v1.14.1/LICENSE)*
