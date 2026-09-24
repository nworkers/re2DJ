# 작업 350 작업 지시서 — 게스트 SEH 체인 관찰 및 INT3 예외 전달 / Task 350 Work Order — Guest SEH Chain Observation and INT3 Exception Dispatch

설계: [20260923-350-guest-seh-chain-observation.md](../design/20260923-350-guest-seh-chain-observation.md)  
선행: [작업 349 작업 로그](../work-logs/20260923-349-linux-inprocess-continuation.md)

## 작업 목표 / Objectives

1. `NativeProcessBootstrap` 및 `NativeFaultObservation` / `OriginalFaultObservation`에 게스트 TEB `FS:[0]` (Tib.ExceptionList) 및 SEH 프레임 관찰 기능을 추가합니다.
2. CLI 진단 출력에 fault 시점의 `FS:[0]` 값, SEH 체인 헤드 포인터 및 핸들러 주소를 포함시킵니다.
3. 실제 4th CHD 연속 실행에서 `0x00af1135` (`INT3`) 시점의 SEH 상태를 관찰하고, 핸들러 등록 여부와 핸들러 코드를 확인합니다.
4. 관찰된 핸들러를 바탕으로 게스트 `INT3`를 Win32 `EXCEPTION_BREAKPOINT`로 변환하여 게스트 SEH 체인에 전달하는 디스패치 경로를 구현 및 검증합니다.

*1. Add guest TEB `FS:[0]` (Tib.ExceptionList) and SEH frame observation to `NativeProcessBootstrap` and `NativeFaultObservation` / `OriginalFaultObservation`.*  
*2. Include `FS:[0]`, SEH frame head pointer, and handler address in CLI diagnostic fault output.*  
*3. Observe the SEH state at `0x00af1135` (`INT3`) during real 4th CHD continuation, confirming whether an SEH handler is registered and its code.*  
*4. Implement and verify the dispatch path converting guest `INT3` to Win32 `EXCEPTION_BREAKPOINT` and delivering it to the guest SEH chain.*

---

## 작업 단계 / Tasks

### 1단계: SEH 체인 관찰 기능 추가 / Step 1: Add SEH Chain Observation
- [ ] `NativeProcessBootstrap`에 `teb()` 접근자 추가
- [ ] `NativeFaultObservation`에 `fs_base`, `seh_frame_address`, `seh_next`, `seh_handler`, `seh_frame_observed` 추가
- [ ] `CaptureNativeFaultObservation`에서 TEB `0x00` (`FS:[0]`)을 읽고 유효 스택 주소인 경우 `next`와 `handler` 역참조
- [ ] `OriginalFaultObservation`에 대응 필드 반영 및 `CopyNativeFaultObservation` 갱신
- [ ] CLI `main.cpp`에서 fault 보고 시 SEH 체인 상태 출력
- [ ] 단위 테스트 작성 및 빌드 검증

### 2단계: 실제 4th CHD 연속 실행 관찰 / Step 2: Real 4th CHD Continuation Observation
- [ ] WSL i386에서 `re2dj --run ez2dj4th --linux-in-process-continue` 실행
- [ ] `0x00af1135` 시점의 `FS:[0]` 값, 등록된 핸들러 주소 확인
- [ ] 핸들러 주소 역어셈블 및 바이트 분석 후 분석 문서 `docs/analysis/ez2dj4th-linux-inprocess-first-import.md` 갱신

### 3단계: INT3 SEH 디스패치 구현 및 검증 / Step 3: INT3 SEH Dispatch Implementation and Verification
- [ ] `NativeProcessBootstrap` 또는 runner에서 게스트 `INT3` (`SIGTRAP`) 발생 시 SEH 핸들러 호출 준비
- [ ] Win32 x86 cdecl 규약에 따른 인자 구성 (`EXCEPTION_RECORD`, `CONTEXT`)
- [ ] 핸들러 호출 및 반환 처리 (`ExceptionContinueExecution`)
- [ ] 실제 4th CHD 대상 연속 실행 재검증

### 4단계: 작업 로그 및 TODO 갱신 / Step 4: Work Log and TODO Update
- [ ] `docs/work-logs/20260923-350-guest-seh-chain-observation.md` 작성
- [ ] `docs/TODO.md` 갱신
