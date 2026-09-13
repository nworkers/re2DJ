# 작업 지시서: EZ2Dancer JAM 음원 파일시스템 진단
# Work Order: Diagnose EZ2Dancer JAM Audio Filesystem Boundary

## 한국어

설계 문서 [20260913-269](../design/20260913-269-ez2d2m-jam-audio-diagnosis.md)에 따라
`ez2d2m`의 `JAM` 음원이 파일시스템, 원본 EZW 처리, DirectSound HLE 중 어디에서
끊기는지 진단합니다.

- [x] 현재 브랜치와 작업 트리를 확인합니다.
- [x] CHD 내부 `ez2dancer/Songs/Jam/jam.ezw`의 존재·크기·헤더를 확인합니다.
- [x] CHD 직접 덤프와 기존 추출본의 내용을 비교합니다.
- [x] 전체 `.ezw` 파일의 헤더 data length와 파일 크기를 교차 검사합니다.
- [x] 기존 실행 로그에서 데모 애셋과 실제 JAM 음원 요청을 구분합니다.
- [x] 진단 결과와 사용자 재현 절차를 분석·설계·작업 로그에 남깁니다.
- [x] 실제 JAM 시작을 포함한 VFS 및 오디오 trace를 확보합니다.
- [x] trace 결과에 따라 별도 수정 설계를 작성합니다. 이번 작업에서는 코드를 수정하지 않습니다.

## English

Following [design 20260913-269](../design/20260913-269-ez2d2m-jam-audio-diagnosis.md),
diagnose whether `ez2d2m`'s missing `JAM` audio stops at the filesystem, original EZW
handling, or the DirectSound HLE boundary.

- [x] Check the current branch and worktree.
- [x] Confirm the CHD path, size, and header for `ez2dancer/Songs/Jam/jam.ezw`.
- [x] Compare a direct CHD dump with the existing extracted copy.
- [x] Cross-check EZW header data lengths against file sizes for the whole tree.
- [x] Distinguish the demo asset from the actual JAM audio request in existing logs.
- [x] Record the diagnosis and user reproduction procedure in the analysis, design, and work log.
- [x] Capture VFS and audio traces covering the actual JAM start.
- [x] Write a separate fix design based on that trace. No code is changed in this task.
