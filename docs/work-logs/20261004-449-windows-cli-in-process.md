# 작업 449 작업 로그 — Windows CLI의 in-process 전환 / Task 449 work log — switching the Windows CLI to the in-process runner

설계: [20261004-449-windows-cli-in-process.md](../design/20261004-449-windows-cli-in-process.md) · 지시서: [20261004-449-windows-cli-in-process.md](../work-orders/20261004-449-windows-cli-in-process.md)

## 2026-10-04

- **런처**: `child_run_options.h`(공용 옵션 이름), `WindowsHostProcessLauncher`(`CreateProcessA`로 실행 중인 실행 파일을 띄우고, 상속 가능한 쓰기 끝 핸들 값을 `--guest-exit-code-fd`로 넘기며, `WaitForSingleObject`·`ReadFile`로 종료 코드를 받음), Windows `WriteGuestExitCode`, `QuoteCommandLineArgument`(CRT 분해 규칙).
- **CLI**: `RE2DJ_IN_PROCESS_HOST`(Linux·Windows). Windows 주입 실행 블록 두 개와 `NormalizeIoConfigForProfile` 제거. 이름 정리(`RunInProcessOriginal` 등). `--demo-volume`·`--audio-volume-trace`·`--guest-wait-trace`·`--vsync`는 두 OS 모두 거부. 사용법의 "Linux:" 표시 정리. MSVC가 모든 enum을 다룬 `switch` 뒤 반환 없음을 경고(C4715)해 알 수 없는 경계에 대한 반환을 더했다.
- **빌드**: Windows backend에 SDL host·키보드·오디오(SDL3_mixer)·런처, `RE2DJ_SDL_HOST_AUDIO`. `re2dj.exe`는 `re2dj_windows_native_backend`를 링크하고 `/BASE:0x60000000 /DYNAMICBASE:NO /LARGEADDRESSAWARE`.
- **0x400000**: 첫 실행이 PE 세션 준비에서 실패했다. 실패 지점을 알리도록 세션 오류 문구를 고친 뒤("cannot map the PE32 image at 0x00400000"), 임시 주소 배치 출력으로 원인을 찾았다.
  - `/STACK:16MB`로 링크한 주 스레드 스택이 그 자리에 있었다.
  - 스택을 빼도 로더가 매핑한 시스템 데이터(MEM_MAPPED 0x31000)가 그 자리에 있었다. 실행 파일의 TLS callback에서 예약해도 늦었다(ERROR_INVALID_ADDRESS 487).
  - 설계 449의 일시 정지 재실행 + `VirtualAllocEx` 예약으로 해결했고, 게스트는 16 MiB 스택의 전용 스레드에서 돈다. 임시 진단 코드는 모두 지웠다.
- **이미지 덤프**: `native_image_dump.{h,cpp}`(virtual 레이아웃, 예전 sidecar 필드 + `source`), continuation의 `SetupWithImageDump`(entry)와 import 처리(resumed), 지연 전 종료 경고, CLI의 요청(`logs/image-dumps/<target>/`, 시각 + 실행 파일 이름), `OriginalRunEnvironment::ImageDumpRequest`.
- **문서**: 설계 449, README(두 OS의 실행 방식), ARCHITECTURE(실행 모델, 계층 표 상태), 이미지 덤프 가이드(새 위치·시점).

  *Launcher: `child_run_options.h` with the shared option names; `WindowsHostProcessLauncher` (`CreateProcessA` of the running executable, the inheritable write end's handle value through `--guest-exit-code-fd`, the exit code through `WaitForSingleObject` and `ReadFile`); the Windows `WriteGuestExitCode`; `QuoteCommandLineArgument` under the CRT's splitting rules. CLI: `RE2DJ_IN_PROCESS_HOST` for Linux and Windows; both Windows injection blocks and `NormalizeIoConfigForProfile` gone; renames (`RunInProcessOriginal` and the rest); `--demo-volume`, `--audio-volume-trace`, `--guest-wait-trace` and `--vsync` refused on both OSes; "Linux:" marks removed from the usage; a return for an unknown boundary added after MSVC warned (C4715) about the `switch` covering every enumerator. Build: SDL hosts, keyboard, audio (SDL3_mixer) and the launcher in the Windows backend with `RE2DJ_SDL_HOST_AUDIO`; re2dj.exe links `re2dj_windows_native_backend` with `/BASE:0x60000000 /DYNAMICBASE:NO /LARGEADDRESSAWARE`. 0x400000: the first run failed preparing the PE session; with the session error made specific a temporary layout dump found the `/STACK:16MB` main thread stack there and, without it, system data the loader maps (MEM_MAPPED, 0x31000), already present when the executable's TLS callback tried to reserve (ERROR_INVALID_ADDRESS 487); design 449's suspended relaunch with a `VirtualAllocEx` reservation fixed it, the guest running on a dedicated 16 MiB-stack thread, and every temporary diagnostic was removed. Image dump: `native_image_dump.{h,cpp}` (virtual layout, the former sidecar fields plus `source`), the continuation's `SetupWithImageDump` (entry) and import handling (resumed) with a warning when the run ends first, the CLI request (`logs/image-dumps/<target>/`, time plus executable name), `OriginalRunEnvironment::ImageDumpRequest`. Documents: design 449, README, ARCHITECTURE, the image dump guide.*

- **검증**
  - Windows x86 Debug(MSVC, 경고를 오류로): 전체 build 성공, CTest 8개 통과.
  - **Windows 실게임 in-process**(`re2dj <target> --call-limit 30000`, 60초 한도). 모두 호출 한도에서 스스로 멈췄다.

    | 타깃 | `Flip` 횟수 |
    | --- | --- |
    | 4th | 599 |
    | 1st SE | 855 |
    | 5th | 592 |
    | EZ2Dancer 2nd MOVE | 132 |
    | 6th(런처 → 6th 자식, 자식이 한도에서 멈춘 뒤 런처 `ExitProcess(0)`) | 719 |

    한도 없이 띄운 6th은 30초 동안 SDL 창(16비트 색)·오디오·`gamepads ready`로 실행되었다.
  - `re2dj ez2dj4th --image-dump --image-dump-delay 3000`(Windows): entry·resumed 각 7,446,528 바이트(`SizeOfImage` 0x71a000). 두 덤프는 2,314,361 바이트가 달라 실행 중 복호화된 코드가 resumed에 담겼다. sidecar의 timestamp 0x3d369bfd는 4th 프로파일 지문과 같다.
  - WSL Linux x64·x86 debug(경고를 오류로): build 성공, CTest 각 5개 통과. Linux `re2dj ez2dj6th --image-dump`: 런처와 6th 자식이 각자 entry·resumed를 쓰고 창을 닫을 때 정상 종료.
  - **확인하지 못한 것**: Windows 게임패드 실제 입력(이 PC의 Xbox 패드가 지금 연결되어 있지 않아 `0 connected`). 창을 손으로 닫는 종료, `--fullscreen`, OSD 조작은 사람이 확인한다. Windows 릴리스 패키지(`package_release.ps1`)는 450에서 정리한다.

  *Verification: the Windows x86 Debug build (MSVC, warnings as errors) passes in full with 8 CTest tests. Real games in-process on Windows (`re2dj <target> --call-limit 30000`, 60 s cap) all stopped on their own at the limit: 4th 599 `Flip`s, 1st SE 855, 5th 592, EZ2Dancer 2nd MOVE 132, 6th 719 (launcher → 6th child, the launcher ending with `ExitProcess(0)` after the child stopped); 6th without a limit ran 30 s with its SDL window (16-bit colour), audio and `gamepads ready`. `re2dj ez2dj4th --image-dump --image-dump-delay 3000` on Windows wrote entry and resumed dumps of 7,446,528 bytes each (`SizeOfImage` 0x71a000) differing in 2,314,361 bytes, the resumed one holding the code decrypted while running; the sidecar's timestamp 0x3d369bfd matches the 4th profile's fingerprint. The WSL Linux x64 and x86 debug builds (warnings as errors) pass with 5 CTest tests each; on Linux `re2dj ez2dj6th --image-dump` has the launcher and the 6th child each write their dumps and ends cleanly when the window closes. **Not checked**: real gamepad input on Windows (this PC's Xbox pad is not connected now); closing the window by hand, `--fullscreen` and the OSD need a person; the Windows release package (`package_release.ps1`) is cleaned in 450.*
