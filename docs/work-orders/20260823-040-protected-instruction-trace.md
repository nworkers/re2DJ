# 보호 실행 파일 instruction trace 작업 지시

관련 설계: [보호 실행 파일 instruction trace](../design/20260823-040-protected-instruction-trace.md)

## 목표

protected `ez2dj.exe`의 post-entry invalid target 전이에 대한 직전 instruction 근거를 수집한다.

## 작업

1. launcher에 `--instruction-trace <max-steps>` 옵션을 추가한다.
2. software entry stop에서 EIP와 Trap Flag를 설정하고 single-step history를 수집한다.
3. illegal instruction 또는 step limit에서 JSONL에 bounded history와 결과를 남긴다.
4. Windows x86 build·CTest와 canonical HDD 실행으로 기록을 검증한다.
5. 분석 문서 및 작업 로그에 확인됨·추정·미확정을 구분해 반영한다.

---

# Protected Executable Instruction Trace Work Order

Related design: [Protected Executable Instruction Trace](../design/20260823-040-protected-instruction-trace.md)

## Goal

Collect direct preceding-instruction evidence for the post-entry invalid-target transfer of protected `ez2dj.exe`.

## Tasks

1. Add `--instruction-trace <max-steps>` to the launcher.
2. Set EIP and Trap Flag at the software-entry stop and retain single-step history.
3. Write bounded history and an outcome to JSONL at an illegal instruction or step limit.
4. Verify through Windows x86 build, CTest, and a canonical HDD run.
5. Update analysis and work log with confirmed, inferred, and unresolved findings.
