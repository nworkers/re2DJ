# Windows Preferred-Base Diagnostic

## 한국어

`ez2dj1.exe`는 base relocation directory가 없어 `0x00400000` preferred base에서만 실행할 수 있습니다. Windows helper는 해당 주소의 `VirtualAlloc` 실패 시 Win32 error code, 요청 base/size, `VirtualQuery`의 allocation base·region size·state·protect·type 및 매핑 파일·모듈 정보를 오류에 기록합니다.

실제 helper 진단으로 `0x00400000`의 `C_949.NLS` 읽기 전용 매핑을 확인했습니다. helper 전용 매니페스트의 UTF-8 활성 코드 페이지는 이 매핑을 제거했지만, 같은 위치에 파일 경로가 공개되지 않는 48 KiB `MEM_MAPPED` 뷰가 남았습니다. 이 매핑은 새 helper 프로세스의 초기 호스트 상태이며 guest의 일부가 아닙니다.

NLS 매핑을 `UnmapViewOfFile`로 제거한 뒤 CRT를 계속 사용하는 방식은 안전하지 않았으므로 채택하지 않습니다. CRT 이전 bootstrap도 같은 주소 예약에 실패하여, 매핑은 CRT가 아닌 Windows x86 프로세스 loader가 entry 호출 전에 만드는 상태로 확인되었습니다. 따라서 현재 helper는 충돌을 상세 오류로 반환하며 임의의 host 매핑을 해제하지 않습니다. 고정 base PE의 실제 실행은 이 Windows 실행 경계에 대응할 별도 backend 전략이 정해진 뒤에 다시 진행합니다.

## English

`ez2dj1.exe` has no base-relocation directory and can run only at preferred base `0x00400000`. On failure of `VirtualAlloc` at that address, the Windows helper records the Win32 error code, requested base/size, and `VirtualQuery` allocation base, region size, state, protection, type, mapped file, and module data in the error.

The live helper diagnostic confirmed a read-only `C_949.NLS` mapping at `0x00400000`. The helper-only UTF-8 active-code-page manifest removed that mapping, but a 48 KiB `MEM_MAPPED` view with no exposed file path remained at the same address. This is initial host state in a newly created helper process, not guest content.

Removing the NLS mapping with `UnmapViewOfFile` while continuing to use the CRT was unsafe and is not adopted. A pre-CRT bootstrap also failed to reserve the same address, confirming that the mapping is created by the Windows x86 process loader before entry invocation rather than by the CRT. The helper therefore returns the detailed conflict error and does not unmap arbitrary host mappings. Actual execution of fixed-base PEs is deferred until a separate backend strategy for this Windows execution boundary is selected.
