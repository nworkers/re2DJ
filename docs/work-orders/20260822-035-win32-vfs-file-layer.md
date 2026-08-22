# Win32 VFS file layer 작업 지시

1. 공용 guest handle table을 구현하고 Windows runtime handle 정책을 분리한다.
2. 최소 Win32 file API를 runtime HLE에 연결하고 launcher가 IAT와 root configuration을 전달하게 한다.
3. `VfsRoots`를 사용해 overlay-first read와 overlay-only copy-on-write를 적용한다.
4. synthetic file API 테스트를 수행하고, 원본 HDD가 제공될 때 original entry 제한 실행 절차를 준비한다.
5. 설계·architecture·TODO·porting plan·work log를 갱신하고 하나의 커밋으로 남긴다.

## English

1. Implement the shared guest handle table and keep the Windows runtime handle policy separate.
2. Connect the minimum Win32 file APIs to the runtime HLE, with launcher IAT and root configuration delivery.
3. Apply overlay-first reads and overlay-only copy-on-write through `VfsRoots`.
4. Run synthetic file-API tests and prepare a limited original-entry procedure for when an HDD is supplied.
5. Update design, architecture, TODO, porting plan, and work log, leaving one commit.
