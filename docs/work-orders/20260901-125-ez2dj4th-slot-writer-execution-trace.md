# 작업 125 — ez2dj4th pointer-slot writer 실행 추적

## 목적

private slot <code>0x00AF0CF4</code>를 기록하는 세 명령에 관찰 전용
hardware execution breakpoint를 적용해 실제 실행 위치와 저장 예정 EAX를
확인합니다.

관련 설계: [ez2dj4th pointer-slot writer 실행 추적 설계](../design/20260901-125-ez2dj4th-slot-writer-execution-trace.md)

## 작업 범위

1. <code>ez2dj4th</code> 전용 <code>--slot-writer-trace</code> option을 추가합니다.
2. primary thread와 새 thread에 세 writer execution breakpoint를 설정합니다.
3. writer hit에서 EAX, pre-store slot, instruction bytes, DR6를 기록합니다.
4. 원본 instruction을 변경하지 않고 hit를 처리해 실행을 계속합니다.
5. 실제 CHD trace로 writer 실행 여부와 값을 확인합니다.
6. 관련 analysis, TODO, architecture, 작업 로그를 갱신합니다.

## 완료 조건

* Windows x86 Debug launcher build와 기존 회귀 검증이 통과합니다.
* breakpoint ready 상태와 bounded hit 또는 no-hit 결과가 JSONL에 남습니다.
* hit 처리 과정에서 원본 code와 slot을 수정하지 않습니다.
* 관찰되지 않은 writer나 보호 응답을 확인된 사실로 단정하지 않습니다.
* 원본 자산과 runtime log는 저장소에 추가되지 않습니다.

## English

## Purpose

Apply observation-only hardware execution breakpoints to the three instructions
that write private slot <code>0x00AF0CF4</code>, identifying the executed writer
and the EAX value about to be stored.

Related design: [ez2dj4th pointer-slot writer execution trace design](../design/20260901-125-ez2dj4th-slot-writer-execution-trace.md)

## Scope

1. Add the <code>ez2dj4th</code>-specific <code>--slot-writer-trace</code> option.
2. Arm three writer execution breakpoints on the primary and newly created threads.
3. Record EAX, pre-store slot, instruction bytes, and DR6 at each hit.
4. Continue the original instruction without modifying it.
5. Identify writer execution and values in a real CHD trace.
6. Update the related analysis, TODO, architecture, and work log.

## Completion criteria

* The Windows x86 Debug launcher build and existing regressions pass.
* JSONL records breakpoint readiness and a bounded hit or no-hit result.
* Hit handling does not modify original code or slot data.
* Unobserved writers or protection responses are not asserted as confirmed.
* Original assets and runtime logs are not added to the repository.
