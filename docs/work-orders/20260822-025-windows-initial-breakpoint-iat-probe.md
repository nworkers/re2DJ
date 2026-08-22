# Windows Initial-Breakpoint IAT Probe

## 한국어

1. original-process probe에 debugger initial-breakpoint 모드를 추가합니다.
2. create/load-DLL event를 계속 처리하고 첫 breakpoint에서 child를 멈춥니다.
3. 정지된 child의 main image base와 모든 IAT slot을 읽기 전용 검증합니다.
4. guest entry를 continue하지 않고 child를 종료합니다.
5. Windows build·unit test·실제 HDD 결과를 작업 로그에 기록합니다.

## English

1. Add debugger initial-breakpoint mode to the original-process probe.
2. Continue create/load-DLL events and stop the child at the first breakpoint.
3. Verify the stopped child's main-image base and every IAT slot read-only.
4. End the child without continuing guest entry.
5. Record Windows build, unit-test, and live HDD results in the work log.
