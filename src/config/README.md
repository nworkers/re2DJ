# src/config

설정 구현이 들어갑니다. Hardlock 비밀 설정 parser와 기본 경로 정책이 구현되어 있습니다.

*Configuration implementation. The Hardlock secret parser and default-path policy are implemented here.*

설계가 확정되기 전까지는 비워 둡니다. 설정 형식이 정해지면 `docs/design/`에 먼저 설계를 남깁니다.

*The format and current secret-handling policy are defined in `docs/design/20260901-127-ez2dj4th-hardlock-runtime.md`.*

`hardlock_secret_config.cpp`는 `[profile-id]` section의 `modad`, `seed1`, `seed2`, `seed3`을 strict 16-bit 값으로 파싱합니다. 설정 원문과 값은 로그로 출력하지 않습니다.

*`hardlock_secret_config.cpp` strictly parses `modad`, `seed1`, `seed2`, and `seed3` as 16-bit values from a `[profile-id]` section. It does not log source text or values.*
