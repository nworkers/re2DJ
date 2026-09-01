# ez2dj4th Hardlock 외부 비밀 설정 작업 로그

관련 설계: [ez2dj4th Hardlock 외부 비밀 설정](../design/20260901-126-ez2dj4th-hardlock-external-config.md)

*Related design: [ez2dj4th Hardlock external secret configuration](../design/20260901-126-ez2dj4th-hardlock-external-config.md).*

## 결과

- 플랫폼 중립 strict INI parser가 선택 프로파일의 module address와 세 16-bit seed를 memory object로 읽습니다.
- parser는 설정 경로를 canonicalize하고 모든 상위 경로의 `.git` marker를 검사해 Git work tree 내부 파일을 거부합니다.
- ez2dj4th profile은 `\\.\FEnteDev` device mock과 외부 Hardlock 설정을 필수로 요구합니다. 다른 프로파일은 해당 설정 옵션을 거부합니다.
- 제품 CLI와 Windows backend는 설정 경로만 launcher에 전달합니다. launcher가 값을 읽어 injected runtime의 네 value export를 먼저 쓰고 enable flag를 마지막에 설정합니다.
- 외부 설정 mode의 descriptor trace는 module address와 ID field를 기록하지 않고 `secret_fields=redacted` 상태만 기록합니다.
- 실제 사용자 제공값은 저장소 밖 임시 설정에만 기록했습니다. 저장소 문서·소스·테스트에는 값이나 설정 원문을 넣지 않았습니다.

*A platform-neutral strict INI parser reads the selected profile's module address and three 16-bit seeds into an in-memory object. It canonicalizes the path and rejects files beneath any ancestor with a `.git` marker. The ez2dj4th profile requires both the `\\.\FEnteDev` device mock and external Hardlock configuration, while other profiles reject the option. The product CLI and Windows backend pass only the path; the launcher writes the four value exports before enabling the injected-runtime state. Descriptor traces record only `secret_fields=redacted` in this mode. Real user-supplied values were written only to a temporary file outside the repository; no repository document, source, test, or configuration text contains them.*

## 검증

- Windows x86 Debug에서 `re2dj_unit_tests`는 1,050 checks, 0 failures였습니다.
- Windows x86 Debug의 Hardlock descriptor runtime probe, product-loader probe 및 unit test CTest 3/3이 통과했습니다.
- 실제 4th CHD 실행에서 외부 설정의 `loaded=true` event와 `outcome=success`를 값 비노출 방식으로 확인했습니다. 실행 후 일치하는 원본 process는 남지 않았습니다.
- Windows x64 재구성은 sandbox network에서 SDL dependency를 다시 내려받지 못해 완료하지 못했습니다. 변경된 공용 parser 자체는 Windows x86 core build와 단위 테스트로 검증했습니다.
- `git diff --check`와 비밀 literal 비포함 검사를 최종 수행합니다.

*Windows x86 Debug `re2dj_unit_tests` completed 1,050 checks with no failures. The Hardlock descriptor runtime probe, product-loader probe, and unit-test CTest set passed 3/3. A real 4th CHD run confirmed value-free `loaded=true` and `outcome=success` events, with no matching original process left afterward. Windows x64 regeneration could not finish because the sandbox network could not redownload SDL; the changed shared parser itself was verified by the Windows x86 core build and unit suite. The task concludes with `git diff --check` and a secret-literal absence check.*

## 남은 범위

이번 작업은 외부 비밀값 수명주기와 runtime 주입을 완료했으며 Hardlock 인증 완료를 주장하지 않습니다. 미문서화 E-Y-E Function `0x0e` 변환은 원본 binary에서 독립 복원하거나 허용 라이선스 근거를 확보한 뒤 Task 127에서 구현합니다. 공개 GPL 구현은 저장소 코드로 복사·번역·링크하지 않습니다.

*This task completes the external-secret lifecycle and runtime injection; it does not claim completed Hardlock authentication. Task 127 will implement the undocumented E-Y-E Function `0x0e` transform only after independent reconstruction from the original binary or a policy-compatible source is available. No public GPL implementation is copied, translated, or linked into repository code.*
