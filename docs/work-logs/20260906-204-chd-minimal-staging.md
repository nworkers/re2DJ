# ez2dj6th CHD 실행파일 선택과 최소 staging 작업 로그

## 결과

`ez2dj6th` 실행을 막던 `FAT32 file chain ended before the requested range` 오류를 제거했습니다. CHD 실행 전에는 선택된 원본 실행파일 하나만 profile별 임시 root에 staging하며, INI·font·asset은 기존 CHD-backed VFS가 요청 시 읽습니다.

6th CHD를 직접 확인한 결과 `EZ2DJ/EZ2DJ.EXE`는 `.\\EZ2DJ6TH.EXE`를 시작하는 bootstrap이고, DirectDraw·DirectSound·DirectInput을 import하는 실제 게임은 `EZ2DJ/EZ2DJ6th.EXE`였습니다. CHD 프로파일이 내부 실행파일 경로를 명시적으로 소유하도록 바꾸고 6th의 대상을 실제 게임 파일로 교정했습니다.

실제 게임의 IAT는 읽기 전용 `.rdata`에 있어 기존 `WriteProcessMemory`가 실패했습니다. 공용 32-bit IAT writer는 직접 쓰기가 실패한 경우 child가 suspended인 동안 해당 4-byte 범위를 임시 `PAGE_READWRITE`로 바꿔 패치하고 원래 보호 속성을 복원합니다.

4th에서 확인한 raw-I/O helper RVA는 6th에서 확인되지 않았고 실제로 다른 코드 위치를 가리키므로 6th 프로파일에서는 비활성화했습니다.

## 실행 관찰

- 제품 명령은 `EZ2DJ/EZ2DJ6th.EXE`를 PE32/i386으로 확인하고 entry RVA `0x000667d4`에 도달했습니다.
- D3D3, DirectSound, DirectInput, VFS, image-loader IAT 준비와 launcher handoff가 성공했습니다.
- 기본 실행은 6th 전용 Hardlock 응답이 없어 `FEnteDev` handshake를 반복한 뒤 종료합니다.
- 값이 노출되지 않는 진단에서 기존 4th Hardlock material을 적용하자 handshake 2회와 descriptor 2회는 완료됐지만, 게임 주 진입 함수가 `-1`을 반환하고 CRT 종료부 `0x00466cc5`를 통해 종료했습니다. 따라서 4th material을 6th에 자동 상속하지 않았습니다.
- 최종 게임 화면 실행에는 6th Hardlock descriptor 계약을 별도로 확인해야 합니다.

## 검증

- Windows x86 Debug 대상 빌드 성공
- 단위 테스트: 1,372 checks, 0 failures
- `re2dj_windows_product_loader_probe`: profile defaults, 2nd defaults, unsupported target, IAT slot 검증 성공
- `re2dj.exe ez2dj6th`: `EZ2DJ/EZ2DJ6th.EXE`, 137 IAT slots, 7 modules, handoff 성공
- 최종 로그에서 `hle_io_ports=false` 확인

원본 CHD와 추출한 실행파일·sector probe 파일은 Git에 추가하지 않았으며, 조사용으로 `build/` 아래에 만든 임시 파일은 삭제했습니다.

---

# ez2dj6th CHD Executable Selection and Minimal Staging Work Log

## Result

Removed the `FAT32 file chain ended before the requested range` failure that blocked `ez2dj6th`. CHD launch now stages only the selected original executable under a profile-specific temporary root; the existing CHD-backed VFS reads the INI, fonts, and assets on demand.

Direct inspection of the 6th CHD confirmed that `EZ2DJ/EZ2DJ.EXE` is a bootstrap launching `.\\EZ2DJ6TH.EXE`, while `EZ2DJ/EZ2DJ6th.EXE` is the actual game importing DirectDraw, DirectSound, and DirectInput. CHD profiles now explicitly own their internal executable path, and the 6th target selects the actual game.

The actual game's IAT resides in read-only `.rdata`, which rejected the old `WriteProcessMemory` call. When a direct write fails, the shared 32-bit IAT writer now temporarily changes only the four-byte range to `PAGE_READWRITE` while the child is suspended, patches it, and restores the original protection.

The raw-I/O helper RVAs confirmed for 4th are unconfirmed for 6th and point into unrelated 6th code, so the 6th profile now leaves those traps disabled.

## Runtime observations

- The product command validates `EZ2DJ/EZ2DJ6th.EXE` as PE32/i386 and reaches entry RVA `0x000667d4`.
- D3D3, DirectSound, DirectInput, VFS, image-loader IAT preparation, and launcher handoff succeed.
- The default run has no 6th-specific Hardlock response and exits after repeated `FEnteDev` handshakes.
- In a value-redacted diagnostic, existing 4th Hardlock material completed two handshakes and two descriptor requests, but the game main entry returned `-1` and exited through the CRT at `0x00466cc5`. The 4th material was therefore not inherited automatically by 6th.
- Reaching the final game screen still requires independent confirmation of the 6th Hardlock descriptor contract.

## Verification

- Windows x86 Debug targets built successfully
- Unit tests: 1,372 checks, 0 failures
- `re2dj_windows_product_loader_probe`: profile defaults, 2nd defaults, unsupported target, and IAT slot checks passed
- `re2dj.exe ez2dj6th`: `EZ2DJ/EZ2DJ6th.EXE`, 137 IAT slots, 7 modules, successful handoff
- Final diagnostic log confirms `hle_io_ports=false`

No original CHD or extracted executable was added to Git. Temporary executable and sector probe files created under `build/` were removed after investigation.
