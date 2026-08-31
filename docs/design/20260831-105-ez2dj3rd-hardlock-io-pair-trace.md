# ez2dj3rd Hardlock 입출력 쌍 추적 설계

## 목적

3rd `EZ2DJ.EXE`가 `Function 0x0e` 호출에 전달하는 8바이트 입력과 반환 이후 사용하는 출력 또는 평문을 런타임에서 확인합니다. 디스크의 `.protect` 섹션은 보호된 바이트이므로 정적 역어셈블 결과를 실행 코드로 단정하지 않고, 원본 프로세스가 복호화한 메모리의 실제 명령 흐름을 추적합니다.

*Confirm at runtime the eight-byte input passed by the 3rd `EZ2DJ.EXE` to `Function 0x0e` and the output or plaintext consumed after the call. Because the on-disk `.protect` section contains protected bytes, do not treat its static disassembly as executed code; trace the actual instruction flow from memory after the original process decrypts it.*

## 확인된 제약

- 3rd는 `ExitProcess`를 정적 import하지 않으며 보호 코드에서 API를 동적으로 해석합니다.
- 기존 post-IOCTL trace는 종료 감시를 위해 정적 `ExitProcess` IAT 슬롯을 필수로 요구하므로 3rd에서 trace 준비 단계가 실패합니다.
- 선택적 system API watch 탐색은 없는 export를 로그로 남기고 계속하지만, 기존 구현은 이때 생성된 오류 문자열을 지우지 않아 준비 완료 후 stale error로 중단됩니다.
- post-IOCTL trace 옵션이 전체 system API watch도 암묵적으로 활성화해 보호 코드의 `GetProcAddress` 경로까지 breakpoint를 설치합니다. 3rd 입출력 추적에는 injected runtime의 `DeviceIoControl` watch만 필요합니다.
- 이 trace의 핵심 종료 조건은 step/event cap이며, `ExitProcess` breakpoint는 진단 종료를 보조할 뿐 입출력 추적 자체의 필수 조건이 아닙니다.

*The 3rd executable does not statically import `ExitProcess`; the protected code resolves APIs dynamically. The existing post-IOCTL trace requires a static `ExitProcess` IAT slot for termination monitoring, so preparation fails on 3rd. Optional system API watch discovery logs missing exports and continues, but the existing implementation leaves the resulting error string behind and later stops on that stale error. The trace is already bounded by step and event caps, making the `ExitProcess` breakpoint an auxiliary termination mechanism rather than a prerequisite for input/output tracing.*

## 설계

1. post-IOCTL trace가 활성화된 경우에만 정적 `ExitProcess` import 부재를 허용합니다.
2. import가 없으면 exit target을 `0`으로 유지하고 bounded debug event loop를 실행합니다.
3. 일반 `--break-exit-process` 요청은 기존처럼 정적 import 부재를 오류로 처리합니다.
4. 진단 로그에 fallback 활성화를 명시해 정상 breakpoint 설치와 구분합니다.
5. 선택적 API watch의 missing export 오류는 해당 누락을 기록한 직후 제거합니다.
6. post-IOCTL trace만 요청한 경우에는 injected runtime의 `DeviceIoControl` watch만 설치하고, 명시적인 `--api-trace`가 함께 요청된 경우에만 전체 system API watch를 설치합니다.
7. post-IOCTL trace에는 선택적 control-code filter를 두어 앞선 보호 초기화 호출을 single-step하지 않고 선택한 호출의 반환부터만 추적할 수 있게 합니다.
8. `0x9c402458` 호출 뒤 실제 명령, output buffer alias, 메모리 접근을 기록하여 입력 블록별 출력/평문 후보를 분리합니다.

*Allow the missing static `ExitProcess` import only when post-IOCTL tracing is active; keep the exit target at zero and run the bounded debug-event loop; preserve the existing error for a normal `--break-exit-process` request; record the fallback explicitly; and trace instructions, output-buffer aliases, and memory use after `0x9c402458` to distinguish output or plaintext candidates for each input block.*

## 검증

- Windows x86 Debug launcher를 빌드합니다.
- 기존 CTest를 실행합니다.
- 3rd full-success synthetic device 실행에서 post-IOCTL instruction event가 생성되는지 확인합니다.
- 관찰된 입력과 출력은 전체 원본 dump가 아니라 8바이트 블록, 주소, 비교/복사 동작만 분석 문서에 기록합니다.

*Build the Windows x86 Debug launcher, run the existing CTest suite, confirm post-IOCTL instruction events in a 3rd full-success synthetic-device run, and record only the eight-byte blocks, addresses, and comparison/copy behavior rather than any complete original dump.*

## 후속 진단 범위 / Follow-up diagnostic scope

중단점 기반 trace가 보호 초기화에 영향을 주므로 injected runtime의 synthetic device 경계에는 control code와 입출력 크기만 기록하는 경량 VFS 진단을 둡니다. 응답 바이트를 만들거나 변경하지 않으며, 프로파일의 기존 IOCTL 정책 뒤에 새로운 성공 조건을 추가하지 않습니다. 이 로그는 현재 실행이 `0x468`에서 끝나는지 또는 향후 `0x44c/0x458`까지 진행하는지를 비침습적으로 구분하는 용도입니다.

*Because breakpoint-based tracing perturbs protection initialization, the injected runtime records only the control code and input/output sizes at the synthetic-device boundary. It neither creates nor changes response bytes and adds no success condition to existing profile IOCTL policy. The lightweight log distinguishes the current `0x468` boundary from future runs that may reach `0x44c/0x458`.*
