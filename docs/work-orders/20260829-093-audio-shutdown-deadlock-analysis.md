# 작업 093 — Win32 오디오 종료 교착 분석

## 목표

`re2dj --run`으로 실행한 1st SE가 진행 중 멈추고 부모와 자식 process가 살아 있는 현상을, 실행 중인 process를 종료하지 않고 진단한다.

## 범위

1. `re2dj.exe`와 `ez2dj.exe`의 process·thread·CPU 상태를 표본화한다.
2. 현재 제품 trace의 마지막 VFS와 Direct3D 동작을 확인한다.
3. WOW64 thread context와 stack을 읽어 대기 중인 API와 호출자를 식별한다.
4. 새로 확인된 사실을 누적 analysis에 반영한다.

## 검증 기준

- 대상 process를 종료하거나 원본 자산을 변경하지 않는다.
- 확인됨, 추정, 미확정을 구분한다.
- 진단용 임시 산출물과 runtime trace는 커밋하지 않는다.

---

# Task 093 — Win32 audio shutdown deadlock analysis

## Objective

Diagnose a 1st SE run that stopped progressing while both the child and parent processes remained alive, without terminating the live process.

## Scope

1. Sample process, thread, and CPU state for re2dj.exe and ez2dj.exe.
2. Inspect the final VFS and Direct3D operations in the current product trace.
3. Read the WOW64 thread context and stack to identify the waiting API and its caller.
4. Record newly confirmed facts in the cumulative analysis.

## Verification criteria

- Do not terminate the target process or modify original assets.
- Distinguish confirmed, inferred, and unresolved findings.
- Do not commit temporary diagnostic artifacts or runtime traces.
