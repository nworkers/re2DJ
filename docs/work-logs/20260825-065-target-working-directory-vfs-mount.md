# target working directory VFS mount 작업 로그

관련 설계: [target working directory 기반 VFS mount](../design/20260825-065-target-working-directory-vfs-mount.md)

관련 작업 지시: [target working directory VFS mount 작업 지시](../work-orders/20260825-065-target-working-directory-vfs-mount.md)

## 결과

launcher가 dump root 대신 선택된 target profile의 `working_directory_relative_path`를 `HddRoot::ResolveDirectory`로 해석해 injected runtime의 source mount로 전달하도록 수정했다. 빈 working directory는 기존처럼 dump root를 사용하며, overlay root와 원본 read-only 정책은 바꾸지 않았다. `vfs_mount` event가 dump, working directory, source, overlay root를 기록한다.

Windows x86 Debug build와 CTest 2/2가 통과했다. 최종 실행 로그는 다음 두 개다.

- `20260825-015837-741.jsonl`
- `20260825-015914-441.jsonl`

두 실행 모두 source root를 `roms/ez2dj1stse/ez2dj`로 기록하고 `coin0.wav`, `coin1.wav`, `WarningMsg.bmp`의 수정된 host 후보까지 진행했다. 기존 KSND controlled exit는 나타나지 않았다.

다음 경계는 두 번 모두 `0x0042292b`에서 주소 0을 읽는 access violation이다. `ECX=0`이고 bytes `8b 11 ff 52 44`는 `mov edx,[ecx]` 뒤 vtable offset `0x44` 호출이다. 객체 종류와 null이 된 선행 생성 경로는 다음 작업에서 정적 caller와 반환값으로 귀속한다. 원본 자산은 변경하지 않았다.

---

# Target Working-Directory VFS Mount Work Log

Related design: [Target Working-Directory VFS Mount](../design/20260825-065-target-working-directory-vfs-mount.md)

Related work order: [Target Working-Directory VFS Mount Work Order](../work-orders/20260825-065-target-working-directory-vfs-mount.md)

The launcher now resolves the selected target profile's working directory and injects it as the runtime source mount instead of using the complete dump root. Empty working directories retain dump-root behavior; overlay and read-only-original policy are unchanged. A `vfs_mount` event records the resolved mapping.

Windows x86 Debug builds and CTest passes 2/2. Final logs `20260825-015837-741.jsonl` and `20260825-015914-441.jsonl` both reach corrected host candidates for `coin0.wav`, `coin1.wav`, and `WarningMsg.bmp`, eliminating the prior KSND controlled exit. Both then reproduce a read access violation at 0x0042292b with ECX zero. Bytes `8b 11 ff 52 44` show a vtable-slot call through a null interface. The object's type and failed creation path remain the next attribution task. Original assets were not modified.
