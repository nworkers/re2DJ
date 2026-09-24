# 작업 351 작업 지시서 — Linux i386 게스트 INT3 Win32 SEH 디스패치 / Task 351 Work Order — Linux i386 Guest INT3 Win32 SEH Dispatch

설계: [20260923-351-linux-guest-seh-dispatch.md](../design/20260923-351-linux-guest-seh-dispatch.md)  
선행: [작업 350 작업 로그](../work-logs/20260923-350-guest-seh-chain-observation.md)  
분석: [4th Linux in-process 첫 import](../analysis/ez2dj4th-linux-inprocess-first-import.md)

## 작업 목표 / Objectives

1. x86 32-bit Win32 SEH 호환 구조체(`Win32ExceptionRecord32`, `Win32Context32`, `Win32ExceptionRegistrationRecord32`)를 정의합니다.
2. `NativeProcessBootstrap`의 시그널 핸들러에서 게스트 자체 `INT3` 감지 시 게스트 SEH 핸들러를 호출하고, `ExceptionContinueExecution`(`0`) 반환 시 수정된 context로 게스트 실행을 재개하는 SEH 디스패치를 구현합니다.
3. 실제 4th CHD 연속 실행(`--linux-in-process-continue`)에서 `0x00af1135` (`INT3`)가 `0x00af159b` 핸들러로 전달되고 실행이 재개되어 이후 API 호출(14번째 호출~) 또는 다음 경계에 도달하는지 검증합니다.
4. 작업 로그 및 분석 문서를 갱신하고 변경 사항을 커밋합니다.

*1. Define x86 32-bit Win32 SEH structures (`Win32ExceptionRecord32`, `Win32Context32`, `Win32ExceptionRegistrationRecord32`).*  
*2. Implement SEH dispatch in `NativeProcessBootstrap`'s signal handler to invoke the guest SEH handler on guest `INT3` and resume execution if `ExceptionContinueExecution` (0) is returned.*  
*3. Verify that on real 4th CHD continuation (`--linux-in-process-continue`), `INT3` (`0x00af1135`) is dispatched to `0x00af159b`, execution resumes, and subsequent API calls (14+) or the next boundary are reached.*  
*4. Update the work log and analysis document, and commit the changes.*

---

## 작업 단계 / Tasks

### 1단계: Win32 SEH 데이터 구조체 작성 / Step 1: Win32 SEH Data Structures
- [ ] `src/platform/linux/x86/guest_seh_types.h` 생성
  - `Win32ExceptionRecord32`
  - `Win32FloatingSaveArea32`
  - `Win32Context32`
  - `Win32ExceptionRegistrationRecord32`
  - `Win32ExceptionDisposition` 상수

### 2단계: NativeProcessBootstrap SEH 디스패치 구현 / Step 2: NativeProcessBootstrap SEH Dispatch
- [ ] `GuestSignalHandler`에서 `SIGTRAP` 수신 시 `INT3` 여부 및 `FS:[0]` SEH 프레임 검사
- [ ] `Win32ExceptionRecord32` 및 `Win32Context32` 생성 및 채우기
- [ ] 게스트 SEH 핸들러 cdecl 호출
- [ ] 반환값 `0` 시 `ucontext_t` 레지스터 복원 및 정상 반환 (`return;`)
- [ ] SEH 디스패치 통계 기록 (횟수, 마지막 핸들러, 재개 EIP)

### 3단계: 빌드 및 검증 / Step 3: Build and Verification
- [ ] Linux x86 Debug 및 x64 Debug 빌드 및 ctest 통과
- [ ] 실제 4th CHD 대상 연속 실행 (`--linux-in-process-continue`)
- [ ] `INT3` 재개 확인 및 이후 API 호출 관찰

### 4단계: 문서 갱신 및 커밋 / Step 4: Documentation and Commit
- [ ] `docs/analysis/ez2dj4th-linux-inprocess-first-import.md` 갱신
- [ ] `docs/work-logs/20260923-351-linux-guest-seh-dispatch.md` 작성
- [ ] `docs/TODO.md` 갱신
- [ ] Git 커밋
