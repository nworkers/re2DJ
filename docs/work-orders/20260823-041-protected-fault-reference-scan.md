# 보호 실행 파일 fault reference scan 작업 지시

관련 설계: [보호 실행 파일 fault reference scan](../design/20260823-041-protected-fault-reference-scan.md)

## 목표

protected illegal-instruction fault target을 저장한 메모리 후보를 수집한다.

## 작업

1. launcher에 `--scan-fault-references` 옵션과 ExitProcess trace 의존성 검사를 추가한다.
2. fault 시 committed private/image memory에서 exact target과 page base의 32-bit reference를 bounded scan한다.
3. JSONL summary·candidate record와 overflow 상태를 기록한다.
4. Windows x86 build·CTest·canonical HDD 실행으로 검증한다.
5. 분석 문서와 작업 로그에 결과 및 해석 한계를 기록한다.

---

# Protected Executable Fault Reference Scan Work Order

Related design: [Protected Executable Fault Reference Scan](../design/20260823-041-protected-fault-reference-scan.md)

## Goal

Collect memory candidates that store the protected illegal-instruction fault target.

## Tasks

1. Add `--scan-fault-references` and validate its ExitProcess-trace dependency.
2. At fault, bounded-scan committed private/image memory for 32-bit references to the exact target and page base.
3. Record JSONL summary, candidate records, and overflow status.
4. Verify using Windows x86 build, CTest, and canonical HDD execution.
5. Record results and interpretation limits in analysis and work log.
