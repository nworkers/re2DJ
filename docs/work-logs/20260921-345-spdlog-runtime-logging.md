# 작업 로그 345: spdlog 런타임 로깅 표준화 / Work log 345: spdlog runtime logging standardization

## 결과 / Result

`spdlog` v1.14.1 기반 공용 `re2dj_logging` 정적 라이브러리를 추가했습니다. 제품 CLI는 option parse보다 먼저 logger를 초기화하며, stderr color sink와 실행별 `logs/re2dj-YYYYMMDD-HHMMSS-mmm.log` file sink에 같은 메시지를 즉시 flush합니다. logger 초기화 자체가 실패한 경우에만 직접 stderr bootstrap fallback을 사용하고 종료 코드 4를 반환합니다.

*Added a shared static `re2dj_logging` library based on `spdlog` v1.14.1. The product CLI initializes it before option parsing and immediately flushes identical messages to a stderr color sink and a per-run `logs/re2dj-YYYYMMDD-HHMMSS-mmm.log` file sink. Only logger-initialization failure uses direct-stderr bootstrap fallback and returns exit code 4.*

기존 제품 CLI의 직접 stderr 진단을 `error`·`warning`·fatal logger 호출로 이관했습니다. 공용 `ImportDispatcher`의 미등록 import, first unimplemented import, 지원되지 않는 실행 옵션·backend, 실행 실패와 guest fault는 spdlog `critical` 및 명시적 `FATAL <분류>` marker를 사용합니다. 별도 프로세스인 Windows injected runtime의 미구현 DirectDraw/Direct3D vtable 호출도 기존 graphics trace에 `FATAL HLE_UNIMPLEMENTED`로 기록합니다. Fatal은 진단 심각도이며 이미 ABI-safe로 확인된 관찰 경로의 지속 여부를 강제로 바꾸지 않습니다. help·version·분석 표의 stdout과 Windows launcher JSONL, VFS, graphics, audio trace는 기존 채널 계약을 유지합니다.

*Migrated direct product-CLI stderr diagnostics to `error`, `warning`, and fatal logger calls. Unknown imports in the shared `ImportDispatcher`, first unimplemented imports, unsupported execution options/backends, execution failures, and guest faults use spdlog `critical` plus an explicit `FATAL <classification>` marker. Unimplemented DirectDraw/Direct3D vtable calls in the separate Windows injected-runtime process now also record `FATAL HLE_UNIMPLEMENTED` in the existing graphics trace. Fatal is a diagnostic severity and does not forcibly change continuation in observation paths already known to be ABI-safe. Help, version, and analysis-table stdout plus Windows launcher JSONL, VFS, graphics, and audio traces retain their existing channel contracts.*

## 참조와 라이선스 / Reference and license

[rePIU](https://github.com/nworkers/rePIU)의 commit `c770da45896b9519924cefc9debdc242ff1e5210`에서 stderr color sink, `[%X.%e] [%8l] [%n] %v` pattern, host/guest 출력 분리와 `critical`+`FATAL` 정책을 참고했습니다. 구현 코드는 복사하지 않았습니다. spdlog는 MIT License이며 고정 버전과 upstream/license 링크를 `THIRD_PARTY_NOTICES.md`와 [KB](../kb/spdlog-runtime-logging.md)에 기록했습니다.

*Referenced the stderr color sink, `[%X.%e] [%8l] [%n] %v` pattern, host/guest output separation, and `critical`+`FATAL` policy from [rePIU](https://github.com/nworkers/rePIU) commit `c770da45896b9519924cefc9debdc242ff1e5210`. No implementation code was copied. spdlog uses the MIT License; its pinned version and upstream/license links are recorded in `THIRD_PARTY_NOTICES.md` and the [KB](../kb/spdlog-runtime-logging.md).*

## 검증 / Validation

- Linux x64 Debug, warnings-as-errors: `re2dj`, `re2dj_unit_tests` build 성공, CTest 1/1 성공.
- Linux x86 Debug, warnings-as-errors: `re2dj`, `re2dj_unit_tests` build 성공, CTest 1/1 성공.
- Linux x86 helper, warnings-as-errors: `re2dj_linux_native_ipc_helper` build 성공.
- Windows x86 Debug, warnings-as-errors: `re2dj`, `re2dj_unit_tests`, `re2dj_windows_injected_runtime` build 성공. `re2dj_unit_tests.exe` 1,868 checks, 0 failures.
- Linux x64 `re2dj --version`: 시작·파일 경로가 stderr에 즉시 출력되고 stdout `0.0.51` 유지.
- Linux x64 미지원 `--fullscreen`: `[critical] [re2dj] FATAL EXECUTION_UNSUPPORTED`가 stderr와 해당 실행별 파일에 동일하게 기록됨.
- Windows x86 `re2dj.exe --version`: 시작·파일 경로가 stderr에 즉시 출력되고 stdout `0.0.51` 유지.
- Windows 전체 CTest는 이미 `docs/TODO.md`에 기록된 `re2dj_windows_vfs_runtime_probe` hang 때문에 중단했습니다. 이번 변경을 포함한 공용 단위 테스트 실행 파일은 직접 실행하여 통과했습니다.

*Validation: Linux x64 Debug and x86 Debug warnings-as-errors builds succeeded for `re2dj` and `re2dj_unit_tests`, with CTest 1/1 passing on each; the Linux x86 `re2dj_linux_native_ipc_helper` also built successfully. Windows x86 Debug warnings-as-errors builds succeeded for `re2dj`, `re2dj_unit_tests`, and `re2dj_windows_injected_runtime`; `re2dj_unit_tests.exe` passed 1,868 checks with zero failures. Linux x64 and Windows x86 `--version` runs immediately emitted startup/file-path messages on stderr while preserving stdout `0.0.51`. An unsupported Linux `--fullscreen` run wrote identical `[critical] [re2dj] FATAL EXECUTION_UNSUPPORTED` lines to stderr and its per-run file. Full Windows CTest was interrupted because of the existing `re2dj_windows_vfs_runtime_probe` hang already tracked in `docs/TODO.md`; the shared unit-test executable containing this change was run directly and passed.*

## 후속 / Follow-up

Windows injected runtime과 개별 분석 도구의 전용 trace writer는 구조화 형식과 process 경계가 다르므로 이번 작업에서 바꾸지 않았습니다. 필요할 때 각 계약을 유지하는 별도 작업으로 이관합니다.

*The Windows injected runtime and individual analysis tools retain dedicated trace writers because their structured formats and process boundaries differ. Migrate them in separate work while preserving each contract when needed.*
