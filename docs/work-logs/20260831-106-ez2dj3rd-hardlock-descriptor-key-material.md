# ez2dj3rd Hardlock descriptor key material 진단 작업 로그

관련 설계: [ez2dj3rd Hardlock descriptor key material capture](../design/20260831-106-ez2dj3rd-hardlock-descriptor-key-material.md)

## 결과

- 공개 packed 32-bit `HL_API` 고정 prefix를 host alignment와 무관하게 raw little-endian bytes에서 읽는 플랫폼 중립 parser를 추가했습니다.
- Windows injected runtime은 synthetic Hardlock IOCTL `0x9c40244c`와 `0x9c402458`에서만 API version, module 정보, block count, function/status, remote/port, `ID_Ref[8]`, `ID_Verify[8]`를 기존 bounded trace budget으로 기록합니다.
- 진단은 response mode보다 먼저 읽기 전용으로 실행되며 return value, `LastError`, output buffer와 bytes-returned를 변경하지 않습니다. 전체 descriptor, reserved bytes와 block payload는 기록하지 않습니다.
- 전용 runtime probe가 synthetic descriptor의 정확한 marker와 실제 일반 LPTDI `0x9c406410` 호출의 비오인을 검증합니다.
- 후속 원본 재검증에서 실제 `ID_Ref`/`ID_Verify`가 두 실행에 걸쳐 동일한 nonzero 값임을 확인했습니다. 이 값은 seed 자체가 아닙니다.
- 공개 GPL seed-recovery 코드는 복사·연결하지 않았습니다. 향후 solver가 필요하면 허용 라이선스를 확인한 별도 설계가 필요합니다.

*A platform-neutral parser now reads the fixed packed-32-bit `HL_API` prefix from raw little-endian bytes without host-alignment assumptions. The Windows injected runtime records only fixed scalars plus `ID_Ref[8]` and `ID_Verify[8]` for synthetic Hardlock IOCTLs `0x9c40244c` and `0x9c402458`, within the existing bounded trace budget. The read-only diagnostic runs before response handling and changes neither return semantics nor buffers. A dedicated runtime probe verifies the exact marker and confirms that an actually issued ordinary LPTDI `0x9c406410` call is not classified as a descriptor. Follow-up original verification confirmed stable nonzero ID fields across two runs; these values are not seeds. No public GPL seed-recovery code was copied or linked.*

## 검증

- Windows x86 Debug 대상 빌드 성공:
  - `re2dj_windows_hardlock_descriptor_probe`
  - `re2dj_unit_tests`
- 선택 CTest 3/3 성공:
  - `re2dj_windows_hardlock_descriptor_probe`
  - `re2dj_windows_product_loader_probe`
  - `re2dj_unit_tests`
- `git diff --check` 성공.
- 전체 VFS runtime probe는 현재 환경에서 새 descriptor 호출에 도달하기 전 기존 windowed client-size policy 경계에서 실패 후 대기해 이번 선택 검증에서 제외했습니다. Task 106 코드는 해당 대형 probe에서 분리되어 있습니다.

*The Windows x86 Debug targets build successfully. The dedicated Hardlock runtime probe, product-loader probe, and unit suite pass 3/3, and `git diff --check` passes. The full VFS runtime probe currently fails and waits at its existing windowed client-size policy boundary before reaching any new descriptor call, so it is excluded from the selected verification; Task 106 is isolated from that large probe.*

## 원본 재검증 추가 결과

- 사용자가 지정한 `roms/ez2dj3rd`에서 원본을 두 번 독립 실행했습니다.
- 두 실행 모두 guest descriptor `0x00a67290`에서 `ID_Ref=478c8b793f201f8a`, `ID_Verify=cc22ae2da344b2a2`가 동일했습니다.
- 고정 scalar도 API version `0x4703`, module ID `0x0000`, module address `0x4c51`, block count `0`, function `0x0000`, status `38`, remote `1`로 일치했습니다.
- 원본 파일이나 전체 descriptor dump는 저장소에 복사하지 않았고, 실행 종료 시 detached child만 종료했습니다.
- 이 값은 seed나 유효 response로 확정하지 않습니다. 별도 작업 107에서 허용 라이선스 SMT 접근의 입력으로만 사용합니다.

*The original executable was run independently twice from the user-specified `roms/ez2dj3rd`. Both runs produced the same guest descriptor at `0x00a67290`: `ID_Ref=478c8b793f201f8a`, `ID_Verify=cc22ae2da344b2a2`, API version `0x4703`, module ID `0x0000`, module address `0x4c51`, block count `0`, function `0x0000`, status `38`, and remote `1`. No original file or complete descriptor dump was copied into the repository, and only the detached child was terminated after observation. These values are not promoted to seeds or valid responses; Task 107 will use them only as inputs to a license-compatible SMT investigation.*
