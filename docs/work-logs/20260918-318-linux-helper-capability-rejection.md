# 작업 로그 318: Linux native-helper capability rejection / Work log 318: Linux native-helper capability rejection

## 결과 / Result

Linux native-helper startup에서 required feature가 누락되면 backend는 `LoadImage`를 보내지 않고 `PrepareImage()`를 실패시킵니다. 실패 후 양쪽 pipe를 닫고 child를 최대 약 250 ms 동안 reap하며, 살아 있는 child만 기존 강제 종료 경로로 정리합니다.

*When a required feature is missing during Linux native-helper startup, the backend fails `PrepareImage()` without sending `LoadImage`. It closes both pipes and reaps the child for up to about 250 ms; only a child that remains alive uses the existing forced-stop cleanup path.*

Linux 전용 `re2dj_linux_capability_rejection_helper` fixture는 import metadata bit만 포함한 `HelloResult`를 보내고, host EOF 뒤 임시 상태 파일에 `eof-before-load-image`를 기록합니다. host probe는 기능 누락 오류와 이 상태를 함께 확인합니다. 따라서 image header 또는 image payload가 feature 합의보다 앞서면 probe가 실패합니다. fixture의 상태 파일 경로는 환경 변수로만 전달되며 제품 helper·protocol에는 추가되지 않았습니다.

*The Linux-only `re2dj_linux_capability_rejection_helper` fixture sends a `HelloResult` containing only the import-metadata bit, then records `eof-before-load-image` in a temporary status file after host EOF. The host probe checks both the missing-feature error and that status. The probe therefore fails if an image header or payload precedes feature agreement. The fixture receives its status-file path only through an environment variable and adds nothing to the product helper or protocol.*

## 검증 / Verification

- WSL2 Ubuntu 24.04에서 `bash scripts/test_linux_native_helper_probe.sh` 실행
- Linux x64 Debug CTest 1/1, i386 production helper ELF32 build, x64 success·SIGILL fault·terminal-stop probe 통과
- x64 rejection fixture가 `linux-capability-rejection-probe: rejected before LoadImage` 출력
- Linux x86 Debug CTest와 x86 host probe를 같은 i386 production helper에 대해 실행했고 script가 성공 종료
- x86 rejection fixture까지 포함한 전체 script 성공 종료

*Ran `bash scripts/test_linux_native_helper_probe.sh` on WSL2 Ubuntu 24.04. Linux x64 Debug CTest passed 1/1; the production helper built as ELF32; and x64 success, SIGILL-fault, and terminal-stop probes passed. The x64 rejection fixture printed `linux-capability-rejection-probe: rejected before LoadImage`. The script also ran Linux x86 Debug CTest and the x86 host probe against the same i386 production helper, including the x86 rejection fixture, and completed successfully.*
