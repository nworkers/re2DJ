# ez2dj6th transform 입력 trace 작업 지시

## 목표

CHD가 연결된 실제 bootstrap → child 실행에서 `0x458` input block과 candidate map key의 불일치 원인을 분리합니다.

*Objective*

Separate the cause of the mismatch between `0x458` input blocks and candidate-map keys during a real CHD-backed bootstrap-to-child run.

## 구현 계획

1. injected runtime에 raw input을 남기지 않는 transform-input trace flag를 추가합니다.
2. launcher와 child handoff에서 flag를 전달합니다.
3. transform 직전 block hash와 block count를 child VFS trace에 기록합니다.
4. Windows x86 Debug build와 unit tests를 실행합니다.
5. candidate key hash와 child trace를 비교하고, 결과에 따라 challenge source 또는 실행 경계를 다음 조사 대상으로 지정합니다.

*Implementation plan*

1. Add a transform-input trace flag to the injected runtime without retaining raw input.
2. Pass the flag through the launcher and child handoff.
3. Record the block count and block hashes in the child VFS trace immediately before transform.
4. Run the Windows x86 Debug build and unit tests.
5. Compare candidate-key hashes with the child trace and select either the challenge source or execution boundary for the next investigation.

## 안전 조건

- raw block/response/seed를 repository 문서나 tracked log에 기록하지 않습니다.
- 기본 실행에서는 새 trace가 비활성입니다.
- 기존 Hardlock response map lookup은 수정하지 않습니다.
- 원본 CHD와 staging root는 읽기 전용으로 취급합니다.

*Safety conditions*

- Do not write raw blocks, responses, or seeds to repository documentation or tracked logs.
- Keep the new trace disabled by default.
- Do not modify existing Hardlock response-map lookup.
- Treat the original CHD and staging root as read-only.
