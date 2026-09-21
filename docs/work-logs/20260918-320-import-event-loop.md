# 작업 로그 320: Import event loop / Work log 320: Import event loop

## 결과 / Result

공용 HLE에 `RunImportEventLoop()`를 추가했습니다. loop는 loaded import gate를 검증해 dispatcher completion 뒤 다음 event를 기다리고, terminal event와 완료 import 수를 반환합니다. 미등록 import와 다른 dispatch 실패는 completion 없이 backend terminal stop을 요청합니다. Linux CLI의 기존 첫 import 관찰 동작은 바꾸지 않았습니다.

*Added shared-HLE `RunImportEventLoop()`. It validates loaded import gates, waits for the next event after dispatcher completion, and returns terminal events plus completed-import count. An unregistered import or another dispatch failure requests backend terminal stop without completion. The Linux CLI's existing first-import observation behavior is unchanged.*

## 검증 / Verification

- fake backend unit test: 두 import의 연속 completion 뒤 process exit, 미등록 import의 terminal stop
- WSL2 Ubuntu 24.04에서 Linux x64 CTest 1/1 통과
- `bash scripts/test_linux_native_helper_probe.sh`가 x64·x86 build, CTest, i386 helper와 기존 integration probe를 포함해 성공 종료

*Verification: fake-backend unit test for two consecutive completions then process exit and terminal stop for an unregistered import; Linux x64 CTest passed 1/1 on WSL2 Ubuntu 24.04; `bash scripts/test_linux_native_helper_probe.sh` completed successfully with x64/x86 builds, CTest, the i386 helper, and existing integration probes.*
