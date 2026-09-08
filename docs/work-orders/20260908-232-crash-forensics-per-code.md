# 작업 지시서: 예외 코드별 크래시 포렌식

## 한국어

### 관련 설계

[예외 코드별 크래시 포렌식 설계](../design/20260908-232-crash-forensics-per-code.md)

### 작업 항목

1. `ReportCrashException`의 단일 기록 자리를 예외 코드별 기록으로 바꾸고, 서로 다른 코드를 최대 8개까지 받습니다.
2. Windows x86 build와 시험을 검증합니다.
3. StreetMix 진입 실행으로 `0xC0000094` 포렌식 기록이 남는지 확인합니다.
4. 진입부 `0xC0000005` 기록이 유지되는지 확인합니다.
5. 3rd·4th 회귀를 확인합니다.
6. 작업 로그에 포렌식 결과와 다음 단계를 남깁니다.

### 제외 범위

- 0 제수의 근본 원인 수정
- VEH 등록 시점이나 처리 방식 변경
- 다른 진단 예산 조정

### 완료 조건

- `crash-exception:code=0xc0000094` 줄이 fault 주소·RVA·레지스터·코드 바이트·스택 워드와 함께 기록됩니다.
- 기존 `0xC0000005` 기록도 남습니다.
- unit test와 VFS runtime probe가 통과하고 3rd·4th에 회귀가 없습니다.

## English

### Related design

[Per-Exception-Code Crash Forensics Design](../design/20260908-232-crash-forensics-per-code.md)

### Work items

1. Replace `ReportCrashException`'s single recording slot with one per exception code, accepting at most eight distinct codes.
2. Verify the Windows x86 build and tests.
3. Confirm a run that enters StreetMix records the `0xC0000094` forensics.
4. Confirm the entry-time `0xC0000005` record is still produced.
5. Confirm no regression for 3rd and 4th.
6. Record the forensic result and the next step in the work log.

### Out of scope

- Fixing the source of the zero divisor
- Changing when or how the VEH is registered
- Adjusting other diagnostic budgets

### Completion criteria

- A `crash-exception:code=0xc0000094` line is recorded with the fault address, RVA, registers, code bytes, and stack words.
- The existing `0xC0000005` record still appears.
- Unit tests and the VFS runtime probe pass with no 3rd or 4th regression.
