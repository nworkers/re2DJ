# Windows x86 원본 프로세스 entry launcher 작업 지시

## 작업 내용

1. Win32 x86 기본 preset에서 원본 프로세스 launcher probe를 빌드한다.
2. `DEBUG_ONLY_THIS_PROCESS` 디버그 이벤트 흐름에서 일반 x86 debug register로 원본 entry hardware breakpoint를 설정한다.
3. entry 직전 single-step에서 주 이미지 기준 주소와 144개 IAT slot의 loader 해석 상태를 검증한다.
4. 확인 결과와 x64 보류 경로와의 차이를 analysis, architecture, porting plan에 기록한다.
5. warnings-as-errors Win32 build, CTest, 실제 HDD 입력 probe를 실행한다.

원본 HDD는 읽기 전용 입력으로만 사용하며, probe는 원본 entry를 실행하지 않고 자식 프로세스를 종료한다.

## English

1. Build an original-process launcher probe in the primary Win32 x86 preset.
2. Set an original-entry hardware breakpoint with normal x86 debug registers in the `DEBUG_ONLY_THIS_PROCESS` event flow.
3. At the pre-entry single-step, verify the main-image base and loader-resolution state of all 144 IAT slots.
4. Record the result and the difference from the deferred x64 path in analysis, architecture, and the porting plan.
5. Run a warnings-as-errors Win32 build, CTest, and the probe with live HDD input.

The original HDD is read-only input only. The probe terminates the child without executing the original entry point.
