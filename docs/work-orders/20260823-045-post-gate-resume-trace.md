# 게이트 이후 복귀 추적 작업 지시

관련 설계: [게이트 이후 복귀 추적](../design/20260823-045-post-gate-resume-trace.md)

## 목표

TF가 WOW64 게이트에서 소멸하기 때문에 관찰되지 않는, `ZwSetEvent` 스텁 32비트 복귀 이후의 최종 전송 명령을 포착한다.

## 작업

1. 언로드 종반 수집기에서 샘플이 `ff d2`(call edx)이고 직전 샘플이 주소 − 5에서 `ba`(mov edx, imm32)로 시작하면, 수집당 최초 1회 그 복귀 주소(샘플 주소 + 2)에 software breakpoint를 설치한다.
2. `EXCEPTION_BREAKPOINT` 처리에서 watched API 조회보다 먼저 복귀 주소 일치를 검사하고, 일치 시 원래 byte 복원·EIP 복귀·TF 재무장·GP 레지스터와 stack 상단 기록(`syscall_resume_hit`)을 수행한 뒤 1회만 처리한다.
3. Windows x86 build·CTest·canonical 실행으로 검증하고, 최종 샘플 해석 결과를 구조 문서와 분석 문서에 반영한다.

---

# Post-Gate Resume Trace Work Order

Related design: [Post-Gate Resume Trace](../design/20260823-045-post-gate-resume-trace.md)

## Goal

Capture the final transferring instruction after the ZwSetEvent stub's return into 32-bit code, which software single-stepping cannot reach because the trap flag dies at the WOW64 gate.

## Tasks

1. In the unload-tail collector, when a sample begins with `ff d2` and the previous sample sits five bytes earlier beginning with `ba`, install a software breakpoint at that sample address plus two — once per collection.
2. In `EXCEPTION_BREAKPOINT` handling, check the resume address before watched-API lookup; on match restore the byte, rewind EIP, re-arm TF, record registers plus top stack words as `syscall_resume_hit`, and process it only once.
3. Verify with the Windows x86 build, CTest, and a canonical run; reflect the decoded final samples in the structures and analysis documents.
