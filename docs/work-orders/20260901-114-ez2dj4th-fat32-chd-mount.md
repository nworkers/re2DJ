# ez2dj4th FAT32/CHD 실행 연결 작업 지시

## 한국어

관련 설계: [ez2dj4th FAT32/CHD 실행 경계 설계](../design/20260901-114-ez2dj4th-fat32-chd-mount.md)

### 목표

실제 `4thTrax.chd`를 `libchdr`와 FAT32 reader로 열어 `EZ2DJ/EZ2DJ.EXE`를 찾고, PE32 검증 및 Windows 원본 실행 launcher까지 연결한다.

### 구현 범위

1. `Fat32Volume` 공용 read-only 계층을 추가한다.
2. MBR/BPB/FAT chain/directory/LFN/file-range read를 단위 테스트하고 실제 CHD probe에 연결한다.
3. `ez2dj4th` built-in profile에 확인된 executable path와 sibling fingerprint를 반영한다.
4. CHD probe가 FAT geometry, executable path/size, PE header를 출력하도록 확장한다.
5. Windows launcher가 executable만 staging하고 injected runtime이 CHD에서 guest files를 직접 읽도록 CHD pseudo-handle 경계를 추가한다.
6. `git diff --check`, unit test, 가능한 Windows build 및 실제 `4thTrax.chd` probe/launch preparation 결과를 작업 로그에 남긴다.

### 제외 범위

* CHD codec을 직접 재구현하지 않는다. `libchdr`를 우선 사용한다.
* 원본 HDD/CHD 내용을 저장소에 추가하거나 overlay에 복사하지 않는다.
* FAT32 쓰기, journaling, exFAT/NTFS, 전체 디스크 mount driver는 구현하지 않는다.
* Windows 이외 host에서 원본 Win32 process execution을 새로 약속하지 않는다.

### 완료 기준

* 실제 `4thTrax.chd`에서 MBR, FAT32 BPB, root, `EZ2DJ/EZ2DJ.EXE` file chain을 읽는다.
* 읽은 executable bytes가 PE32/i386/non-DLL로 검증된다.
* Windows launcher가 staging executable과 CHD path를 함께 전달하고, runtime VFS setup이 성공 가능한 코드 경계를 갖는다.
* 원본 자산 없이도 공용 build와 unit tests가 통과한다.

## English

Related design: [ez2dj4th FAT32/CHD execution-boundary design](../design/20260901-114-ez2dj4th-fat32-chd-mount.md)

### Goal

Open the real `4thTrax.chd` through `libchdr` and a FAT32 reader, locate and validate `EZ2DJ/EZ2DJ.EXE`, and connect its PE32 bytes to the Windows original-process launcher.

### Scope

Add a portable read-only FAT32 volume, tests and probe output, update the confirmed `ez2dj4th` profile, stage only the executable for `CreateProcessW`, and add CHD-backed pseudo handles in the injected runtime for guest reads. Keep all writes in the existing overlay and reuse `libchdr` for CHD decoding.

### Out of scope

No CHD codec rewrite, original-asset commit, FAT32 writes, filesystem-driver mount, or new non-Windows original-process promise is made here.
