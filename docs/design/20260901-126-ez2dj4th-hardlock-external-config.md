# ez2dj4th Hardlock 외부 비밀 설정 설계

## 목적

ez2dj4th 프로파일이 Hardlock 모듈 주소와 세 개의 16비트 seed를 저장소 밖 설정 파일에서만 읽도록 합니다. 비밀값은 소스 코드, 테스트 fixture, 문서, 작업 기록, Git 객체 및 진단 로그에 남기지 않습니다.

*Make the ez2dj4th profile read its Hardlock module address and three 16-bit seeds only from a configuration file outside the repository. Secret values must not appear in source code, test fixtures, documentation, work records, Git objects, or diagnostic logs.*

## 경계와 데이터 흐름

```mermaid
flowchart LR
    A[저장소 밖 설정 파일] --> B[공용 strict parser]
    B --> C[선택 프로파일의 메모리 객체]
    C --> D[Windows x86 launcher]
    D --> E[injected runtime exports]
    E --> F[Hardlock device HLE 경계]
```

설정 파일 경로는 제품 CLI의 `--hardlock-config <path>`로 받습니다. parser는 선택한 프로파일 section의 `modad`, `seed1`, `seed2`, `seed3`만 읽고 네 값이 모두 정확한 16비트 정수인지 검증합니다. 값은 decimal 또는 `0x` 접두 hex를 허용합니다. 오류에는 원문 line이나 값 자체를 포함하지 않습니다.

*The product CLI accepts the configuration path through `--hardlock-config <path>`. The parser reads only `modad`, `seed1`, `seed2`, and `seed3` from the selected profile section and validates that all four are exact 16-bit integers. Decimal and `0x`-prefixed hexadecimal forms are accepted. Errors never contain the original line or value.*

## 저장소 외부 강제 정책

parser는 설정 파일과 그 상위 경로 중 `.git` directory가 있는 경로를 거부합니다. 이 검사는 현재 작업 directory에 의존하지 않으므로 소스 checkout 위치가 달라도 동일하게 적용됩니다. 예시 문서에는 실제 값 대신 `<16-bit value>` placeholder만 둡니다.

*The parser rejects a configuration file when that path or any ancestor contains a `.git` directory. This check does not depend on the current working directory and therefore behaves consistently across checkout locations. Examples use only `<16-bit value>` placeholders.*

## 프로파일 정책

`TargetLptdiPolicy`에는 비밀값이 아닌 `hardlock_secret_config_required` capability만 추가합니다. ez2dj4th는 `\\.\FEnteDev` synthetic device와 외부 설정을 요구합니다. 다른 프로파일에 `--hardlock-config`를 전달하면 거부합니다. 기존 프로파일 내 diagnostic target state는 이번 작업에서 변경하지 않습니다.

*Add only the non-secret `hardlock_secret_config_required` capability to `TargetLptdiPolicy`. The ez2dj4th profile requires the `\\.\FEnteDev` synthetic device and external configuration. Passing `--hardlock-config` to another profile is rejected. Existing per-profile diagnostic target state remains unchanged in this task.*

## 런타임 및 알고리즘 범위

launcher는 파싱한 네 값을 명령행에 재직렬화하지 않고 injected runtime의 별도 export에 직접 씁니다. 로그에는 설정을 성공적으로 적재했다는 boolean 상태만 기록합니다. 외부 설정 mode에서는 descriptor의 module address와 ID 계측도 비밀 유추를 막기 위해 redaction합니다.

*The launcher does not serialize the four parsed values back onto the command line; it writes them directly to dedicated injected-runtime exports. Logs record only a Boolean loaded state. In external-configuration mode, descriptor module-address and ID diagnostics are redacted to prevent inference of secret material.*

미문서화 E-Y-E Function `0x0e` 변환은 독립적인 원본 분석 또는 BSD/MIT/Apache-2.0 호환 근거가 확보된 뒤 별도 작업으로 구현합니다. 공개 GPL 구현을 복사, 번역, 링크하지 않습니다. 이번 작업은 값의 안전한 수명주기와 HLE 장치 경계 주입을 완성하며 인증 완료를 주장하지 않습니다.

*Implement the undocumented E-Y-E Function `0x0e` transform in a separate task only after independent original-binary reconstruction or a BSD/MIT/Apache-2.0-compatible basis is available. Do not copy, translate, or link a public GPL implementation. This task completes the secure value lifecycle and HLE device-boundary injection without claiming completed authentication.*

## 검증 전략

- runtime-generated synthetic 값으로 parser 성공, 누락, 중복, 범위 초과, 미지원 key와 Git 내부 경로 거부를 단위 테스트합니다.
- product-loader probe에서 4th만 외부 설정을 요구하고 launcher argument에 설정 경로만 전달하는지 확인합니다.
- 실제 사용자 값은 저장소 밖 임시 설정 파일에만 기록해 Windows x86 launcher가 적재·주입하는지 확인합니다. 값이나 원문 설정은 테스트 출력에 남기지 않습니다.
- 전체 변경에 대해 Windows x86 build, 관련 CTest 및 `git diff --check`를 수행합니다.

*Unit-test successful parsing, missing and duplicate fields, overflow, unsupported keys, and rejection of Git-contained paths with synthetic values generated at runtime. Verify with the product-loader probe that only the 4th profile requires external configuration and that only its path enters launcher arguments. Keep real user values solely in a temporary external file while confirming Windows x86 launcher loading and injection without printing values or source text. Run the Windows x86 build, relevant CTest coverage, and `git diff --check`.*
