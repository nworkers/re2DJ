# target working directory VFS mount 작업 지시

관련 설계: [target working directory 기반 VFS mount](../design/20260825-065-target-working-directory-vfs-mount.md)

## 상태

**완료**

## 작업 범위

1. target profile의 working directory를 안전하게 해석한다.
2. 해석된 경로를 Windows x86 injected runtime의 HDD source mount로 주입하고 진단 event를 남긴다.
3. Windows x86 build와 CTest를 수행한다.
4. canonical 실행 2회에서 `coin0.wav` open과 다음 경계, `av_access`를 확인한다.
5. 누적 문서와 작업 로그를 갱신한다.

---

# Target Working-Directory VFS Mount Work Order

Related design: [Target Working-Directory VFS Mount](../design/20260825-065-target-working-directory-vfs-mount.md)

## Status and scope

**Complete.** Resolved the selected target's working directory, injected it as the Windows x86 runtime source mount with a diagnostic event, built and tested, then ran the canonical path twice to classify the corrected `coin0.wav` open and the next access violation.
