# ez2dj4th Hardlock 외부 비밀 설정 작업 지시

관련 설계: [ez2dj4th Hardlock 외부 비밀 설정](../design/20260901-126-ez2dj4th-hardlock-external-config.md)

*Related design: [ez2dj4th Hardlock external secret configuration](../design/20260901-126-ez2dj4th-hardlock-external-config.md).* 

## 범위

1. 플랫폼 중립 Hardlock 비밀 설정 parser와 Git checkout 내부 경로 거부 정책을 추가합니다.
2. ez2dj4th target profile에 외부 설정 필수 capability와 `\\.\FEnteDev` 장치 경계를 연결합니다.
3. 제품 CLI, Windows original-process backend 및 x86 launcher를 통해 설정 경로와 메모리 내 값을 전달합니다.
4. injected runtime에 비밀값 export와 redacted descriptor 진단 경계를 추가합니다.
5. 실제 변환 알고리즘과 무관한 synthetic 단위 테스트 및 외부 설정 통합 검증을 수행합니다.
6. 설계, 아키텍처, 설정 안내, 작업 로그와 TODO를 실제 검증 결과에 맞게 갱신합니다.

*Add a platform-neutral Hardlock secret parser and Git-checkout path rejection; connect the ez2dj4th target profile to an external-config-required `\\.\FEnteDev` device boundary; carry the path and in-memory values through the product CLI, Windows original-process backend, and x86 launcher; add secret exports and redacted descriptor diagnostics to the injected runtime; run synthetic unit tests and an external-config integration check independent of the transform algorithm; and update design, architecture, configuration guidance, work log, and TODO to match verified results.*

## 완료 조건

- 저장소 추적 파일과 Git diff에 사용자가 제공한 비밀값이 없습니다.
- 설정 파일이 Git checkout 안에 있거나 선택 프로파일의 필수 key가 잘못되면 값 비노출 오류로 실패합니다.
- ez2dj4th는 외부 설정 없이는 실행 argument 생성 단계에서 실패하며, 다른 프로파일은 해당 옵션을 거부합니다.
- launcher는 네 값을 runtime memory로 주입하고 값 없는 적재 상태만 기록합니다.
- 미문서화 Function `0x0e` transform을 구현하지 않은 상태를 명확히 유지합니다.
- 범위에 맞는 build와 test가 통과하고 작업 로그가 작성됩니다.

*Completion requires no user-provided secret values in tracked files or Git diff; value-redacted failures for Git-contained files or malformed selected-profile fields; mandatory external configuration for ez2dj4th and rejection for other profiles; direct launcher injection of all four values into runtime memory with only value-free loaded status logging; an explicit unresolved state for the undocumented Function `0x0e` transform; and passing scoped builds and tests with a completed work log.*
