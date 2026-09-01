# ez2dj4th Hardlock runtime 작업 로그

관련 설계: [ez2dj4th Hardlock runtime](../design/20260901-127-ez2dj4th-hardlock-runtime.md)

*Related design: [ez2dj4th Hardlock runtime](../design/20260901-127-ez2dj4th-hardlock-runtime.md).*

## 결과

- 실제 설정을 루트 `.gitignore`의 `/cfg/` 규칙이 제외하는 `cfg/hardlock.ini`에 두었습니다. 기존 임시 설정 파일은 제거했습니다.
- 제품 CLI와 x86 launcher는 ez2dj4th에서 명시적 옵션이 없으면 기본 설정을 선택합니다. 저장소 내부 경로는 `cfg/` 아래만 허용하며 외부 경로도 계속 지원합니다.
- 옵션 없는 실제 CHD 제품 실행에서 설정 적재 boolean과 정상 종료를 확인했습니다. 키와 module address는 명령행·제품 로그에 기록되지 않았습니다.
- slot-writer bounded 실행에서 실제 `NTICE`/`FEnteDev` open과 `0x9c402468` (`0/0`)을 재확인했습니다. active-console HLE 실행은 동일한 6-byte in-place `0x9c402450`을 세 번 기록했습니다.
- 플랫폼 중립 `HardlockProtocolTracker`를 추가해 네 IOCTL의 순서·크기, descriptor Function/block count, configured module-address match를 검증합니다. runtime 로그는 값 대신 boolean만 기록합니다.
- 기존 synthetic `0x450` replay와 nonzero tail 분기 실험을 active-console HLE와 결합한 4th bounded 실행은 initialize 1회, handshake 2회, descriptor 37회, transform 36회를 기록하고 child exit로 끝났습니다. 모든 shape/sequence가 유효했고 모든 descriptor가 외부 profile의 module address와 일치했습니다. 이 replay/tail은 실제 동글 응답으로 승격하지 않습니다.
- 현재 buffer-preserving `0x450` success는 `0x44c`로 진행하지 못합니다. 유효한 driver response와 허용 가능한 Function `0x0e` bit-level 근거가 없어 변환은 구현하지 않았습니다. copyleft 구현이나 추측 응답은 사용하지 않았습니다.
- active-console 관찰은 필요한 evidence를 얻은 뒤 중단했으며, 해당 launcher와 원본 process가 남아 있지 않음을 확인했습니다.

*The real configuration now lives at `cfg/hardlock.ini`, excluded by the root `/cfg/` ignore rule, and the former temporary file was removed. The product CLI and x86 launcher select this default for ez2dj4th without an option, allow only `cfg/` for repository-internal paths, and continue to support explicit external paths. An option-free real-CHD product launch confirmed Boolean-only loading and successful exit without putting keys or the module address on the command line or in product logs. A bounded slot-writer run reacquired the real `NTICE`/`FEnteDev` opens and zero-sized `0x9c402468`; active-console HLE reached six-byte `0x450`. Combining the existing synthetic `0x450` replay and nonzero-tail branch experiment with active-console HLE produced one initialize, two handshakes, 37 descriptors, and 36 transforms before child exit. Every shape and sequence was valid and every descriptor matched the external profile module address; these branch values are not promoted to real dongle responses. The tracker logs only value-free Booleans. Buffer-preserving `0x450` success still does not reach `0x44c`; no valid driver response or policy-compatible Function `0x0e` basis is available, so no copyleft-derived or guessed transform was implemented. No matching launcher or original process remained.*

## 검증

- Windows x86 Debug build: unit tests, injected runtime, Hardlock descriptor probe, product-loader probe, launcher, product CLI 통과
- Windows x86 selected CTest: `3/3` 통과
- Unit tests: `1070` checks, `0` failures
- Windows x64 추가 build는 비어 있는 SDL dependency를 GitHub에서 다시 받아야 했으나 현재 sandbox network가 차단해 configure 단계에서 중단됨
- 실제 CHD option-free 설정 적재와 bounded `0x468` protocol trace 통과
- 실제 CHD synthetic branch의 `0x44c/0x458` shape·sequence·module-match oracle 통과
- `cfg/hardlock.ini` ignore 및 비밀값 비추적 검사 통과
- `git diff --check` 통과

*Verification: the Windows x86 Debug unit tests, injected runtime, Hardlock descriptor probe, product-loader probe, launcher, and product CLI build; selected CTest passes `3/3`; unit tests pass 1,070 checks with zero failures; an additional Windows x64 build could not reconfigure because its empty SDL dependency required a GitHub fetch blocked by sandbox networking; the real CHD confirms option-free loading and bounded `0x468` protocol tracing; the ignored configuration and secret non-tracking checks pass; and `git diff --check` passes.*
