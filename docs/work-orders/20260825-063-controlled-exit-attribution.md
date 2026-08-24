# 원본 controlled exit 원인 귀속 작업 지시

관련 설계: [controlled exit 원인 귀속](../design/20260825-063-controlled-exit-attribution.md)

## 상태

**완료.** 공유 종료 helper의 EBP frame에서 실제 오류 caller, format message와 detail 문자열을 반복 복원했다.

## 작업 범위

1. `ExitProcess` breakpoint에서 direct return과 wrapper EBP frame을 함께 읽는다.
2. 확인된 wrapper RVA에 한해 caller, message pointer와 bounded string을 기록한다.
3. 잘못된 frame/pointer를 종료 동작 변경 없이 명시적으로 표시한다.
4. Windows x86 build/CTest와 canonical 두 번으로 재현성을 검증한다.
5. 귀속 결과와 `av_access` 유무를 analysis, TODO, 작업 로그에 반영한다.

## 완료 조건

두 canonical 실행이 같은 실제 종료 caller와 오류 message를 기록하고, 이를 다음 HLE 작업의 확정 근거로 사용할 수 있어야 한다.

---

# Controlled Exit Cause Attribution Work Order

Related design: [Controlled Exit Cause Attribution](../design/20260825-063-controlled-exit-attribution.md)

## Status and scope

**Complete.** Repeatedly recovered the real caller, format message, and detail string from the shared exit helper's EBP frame while preserving original control flow.
