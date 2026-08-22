# Windows Preferred-Base Diagnostic Result

## 한국어

Windows x64 observer와 stage된 Win32 helper를 실제 `roms\ez2dj1stse` HDD 입력으로 실행했습니다. `ez2dj1.exe`의 image base는 `0x00400000`, `SizeOfImage`는 `0x01ad1000`이며 base relocation directory가 없습니다.

helper의 requested-base 예약은 다음 충돌을 보고했습니다.

```text
occupied 0x00400000 ... type 0x40000 ... C_949.NLS
```

`0x00400000`은 helper의 `C_949.NLS` mapped view와 충돌합니다. UTF-8 active-code-page 매니페스트, mapped view 해제, CRT 이전 bootstrap 예약을 순서대로 검증했습니다. UTF-8에서는 다른 `MEM_MAPPED` view가 남았고, view 해제 뒤 CRT를 사용하는 helper는 종료했습니다. CRT 이전 bootstrap도 예약에 실패했습니다. 따라서 충돌 매핑은 helper CRT가 아니라 Windows x86 process loader가 entry 호출 전에 만드는 상태로 확인되었습니다.

안전하지 않은 실험 코드는 제거했습니다. 최종 구현은 requested-base 실패 시 충돌 주소, allocation, protection, type, mapped file 및 module을 반환합니다. 이 HDD 조합에서 import 관찰 단계에는 아직 도달하지 못했습니다. 고정 base PE를 실행하려면 이 loader 제약을 우회하지 않는 별도 Windows execution backend 전략이 필요합니다.

검증:

1. `scripts\stage_windows_native_helper.ps1` 성공
2. `re2dj_windows_import_observer --hdd .\roms\ez2dj1stse`가 위의 상세 충돌 진단을 반환

## English

The Windows x64 observer and staged Win32 helper were run against the live `roms\ez2dj1stse` HDD input. `ez2dj1.exe` has image base `0x00400000`, `SizeOfImage` `0x01ad1000`, and no base-relocation directory.

The requested-base reservation reported this conflict:

```text
occupied 0x00400000 ... type 0x40000 ... C_949.NLS
```

`0x00400000` conflicts with the helper's mapped `C_949.NLS` view. A UTF-8 active-code-page manifest, mapped-view removal, and a pre-CRT bootstrap reservation were tested in order. UTF-8 left another `MEM_MAPPED` view, the helper exited when it continued to use the CRT after view removal, and the pre-CRT bootstrap also failed to reserve the address. The conflicting mapping is therefore established by the Windows x86 process loader before entry invocation, not by the helper CRT.

Unsafe experimental code was removed. The final implementation reports the conflicting address, allocation, protection, type, mapped file, and module when requested-base reservation fails. This HDD combination has not yet reached import observation. Running fixed-base PEs requires a separate Windows execution-backend strategy that does not bypass this loader constraint.

Verification:

1. `scripts\stage_windows_native_helper.ps1` succeeded.
2. `re2dj_windows_import_observer --hdd .\roms\ez2dj1stse` returned the detailed conflict diagnostic above.
