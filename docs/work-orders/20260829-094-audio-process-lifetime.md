# 작업 094 — Win32 SDL 오디오 process-lifetime 작업 지시

관련 설계: [Win32 SDL 오디오 process-lifetime 설계](../design/20260829-094-audio-process-lifetime.md)

## 상태

**완료.** process-lifetime backend와 실제 WASAPI exit child 회귀를 구현했다. 수정 전 probe는 5초 timeout으로 실패했고 수정 후 0.56초에 통과했다. 실제 제품 실행도 종료 중 thread 1개 교착 없이 부모와 자식이 모두 끝났다.

## 작업

1. SDL3 mixer backend singleton을 atexit에서 소멸하지 않는 process-lifetime 객체로 변경한다.
2. Windows runtime probe에 WASAPI 초기화 후 native `ExitProcess`를 호출하는 child 검증을 추가한다.
3. Debug/Release Windows x86 build와 CTest를 실행한다.
4. 실제 1st SE 실행으로 종료 중 교착 해소 여부를 확인한다.
5. ARCHITECTURE, analysis, IMPLEMENTED와 작업 로그를 결과에 맞게 갱신한다.

## 비범위

- 원본이 `ExitProcess`에 진입한 최초 조건의 수정
- 모든 exit import를 `TerminateProcess`로 교체
- 원본 자산 또는 overlay 내용 변경

---

# Task 094 — Win32 SDL audio process-lifetime work order

Related design: [Win32 SDL audio process-lifetime design](../design/20260829-094-audio-process-lifetime.md)

## Status

**Complete.** The process-lifetime backend and real WASAPI exit-child regression are implemented. The pre-fix probe timed out after five seconds; the corrected probe passed in 0.56 seconds. A live product run also ended both parent and child without the former one-thread shutdown deadlock.

## Work

1. Change the SDL3 mixer backend singleton to a process-lifetime object that is not destroyed at atexit.
2. Add a Windows runtime-probe child that initializes WASAPI and calls native `ExitProcess`.
3. Run Debug and Release Windows x86 builds and CTest.
4. Verify the shutdown deadlock with a live 1st SE run.
5. Update ARCHITECTURE, analysis, IMPLEMENTED, and the work log with results.

## Out of scope

- Fixing the initial condition that makes the original executable enter `ExitProcess`
- Replacing every exit import with `TerminateProcess`
- Modifying original assets or overlay contents
