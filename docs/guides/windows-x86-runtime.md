# Windows x86 원본 실행 가이드

관련 설계: [렌더링 정확성·성능 회복](../design/20260826-072-render-correctness-performance.md)

관련 제품 loader 설계: [Win32 제품 loader 통합](../design/20260828-079-win32-product-loader.md)

관련 오디오 설계: [Win32 오디오 master gain](../design/20260828-080-win32-audio-master-gain.md)

관련 streaming 설계: [DirectSound streaming/ring-buffer 동기화](../design/20260828-083-directsound-streaming-ring-buffer.md)

관련 데모 음량 설계: [DirectSound 데모 음량 설정 HLE](../design/20260829-086-directsound-volume-transition.md)

관련 창 설계: [Win32 창 모드와 메시지 pump](../design/20260828-084-window-mode-message-pump.md), [Win32 실행 창 제목과 기본 2배 확대](../design/20260829-091-window-title-default-scale.md)

관련 작업 로그: [렌더링 정확성·성능 회복](../work-logs/20260826-072-render-correctness-performance.md), [Win32 제품 loader 통합](../work-logs/20260828-079-win32-product-loader.md)

저장소 root의 PowerShell에서 제품 loader를 실행한다. `--hdd`에는 합법적으로 보유한 1st SE HDD dump 디렉터리를 지정한다.

```powershell
.\build\windows-x86\bin\Debug\re2dj.exe --hdd .\roms\ez2dj1stse --target ez2dj1stse --run
```

제품 facade는 command line, Windows directory, VFS, DirectDraw/Direct3D 3, DirectSound, legacy I/O port, LPTDI target state와 detached 실행의 현재 canonical policy를 선택한다. 초기 복원과 IAT 검증 뒤 debugger를 분리하므로 실제 화면·오디오·입력과 성능 확인에 사용한다. 창을 닫으면 loader도 종료된다.

기본 실행은 version, build date, SDL3 OpenGL renderer와 FPS를 제목에 표시하는 resize 가능한 1280×960 client-area 창이다. 원본의 640×480 논리 표시는 기본 가로·세로 2배로 확대된다. monitor 크기의 borderless fullscreen을 선택하려면 원본 INI를 수정하지 않고 다음처럼 외부 옵션을 추가한다.

```powershell
.\build\windows-x86\bin\Debug\re2dj.exe --hdd .\roms\ez2dj1stse --target ez2dj1stse --run --fullscreen
```

키보드 I/O를 사용하려면 [예제 INI](../../config/ez2dj-io.example.ini)를 복사·수정한 뒤 외부 설정으로 주입한다.

```powershell
.\build\windows-x86\bin\Debug\re2dj.exe --hdd .\roms\ez2dj1stse --target ez2dj1stse --run --io-config .\config\ez2dj-io.example.ini
```

`[buttons]` 값은 `A`~`Z`, `0`~`9`, `F1`~`F24`, `ENTER`, `SPACE`, `LSHIFT`, `RSHIFT`, 방향키, `NUMPAD0`~`NUMPAD9`, `DECIMAL`, `NONE`을 지원한다. `[turntables] step`은 1~32다. 설정을 생략하면 I/O board는 idle 상태로 동작한다.

창을 클릭하거나 이동한 뒤에도 화면 갱신이 계속되고 Windows 작업 관리자에서 응답 중인지 확인한다. fullscreen 종료 뒤 desktop 해상도가 바뀌지 않아야 한다.

제품은 원본 HDD의 `ez2dj.ini`를 수정하지 않고 `GAMEASSIGNMENTS/DemoVolume`만 기본 profile 3으로 재정의한다. profile 0..3은 원본 DirectSound 값 `-10000`, `-2222`, `-1111`, `0`에 대응한다. 원본 profile을 비교하려면 다음처럼 선택한다.

```powershell
.\build\windows-x86\bin\Debug\re2dj.exe --hdd .\roms\ez2dj1stse --target ez2dj1stse --demo-volume 2 --run
```

SDL 최종 master gain 기본값은 0 dB다. 장치별 보정이 필요할 때만 `--audio-gain-db`를 사용한다. 허용 범위는 `-24..+18 dB`이며 양의 값에서는 clipping 가능성을 확인한다.

상세 AV와 API 경계 진단에는 호환 유지되는 진단 entry를 사용한다.

```powershell
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe --hdd .\roms\ez2dj1stse --target ez2dj1stse --hle-command-line --hle-windows-directory --hle-vfs --hle-d3d3 --hle-directsound --hle-io-ports --device-mock-lptdi-target-state 0900000000000000 --api-trace
```

debugger mode는 I/O마다 exception 왕복이 발생하므로 성능 기준으로 사용하지 않는다.

원본 HDD는 읽기 전용 source로 사용하며 guest write는 `overlays/ez2dj1stse`로 보내는 정책이다. 실행 로그는 `logs/windows_x86_launcher_probe/ez2dj1stse`에 생성되며 저장소에는 commit하지 않는다.

---

# Windows x86 Original Runtime Guide

Run the product-loader command above from the repository root in PowerShell, replacing `--hdd` with a legally owned 1st SE HDD directory. The facade selects the current canonical command-line, Windows-directory, VFS, graphics, audio, legacy-I/O, LPTDI, and detached-runtime policy. It restores and verifies the process, detaches the debugger, and lets the injected runtime handle confirmed boundaries. Close the game window to finish the loader.

The default run uses a resizable 1280x960 client-area window whose title shows the version, build date, SDL3 OpenGL renderer, and FPS. It initially scales the original 640x480 logical display by 2x in each dimension. Add `--fullscreen` to the same command for monitor-sized borderless fullscreen without editing the original INI. After clicking or moving the window, confirm that frames continue and Task Manager still reports it as responsive. The desktop resolution must remain unchanged after fullscreen exits.

For keyboard I/O, copy and edit [`config/ez2dj-io.example.ini`](../../config/ez2dj-io.example.ini), then add `--io-config <path>` to the product command. Button values accept `A` through `Z`, digits, `F1` through `F24`, the named navigation/numpad keys shown above, or `NONE`; turntable `step` accepts 1 through 32. Omitting the option leaves the emulated board idle.

Without modifying the original HDD's `ez2dj.ini`, the product overrides only `GAMEASSIGNMENTS/DemoVolume` to profile 3 by default. Profiles 0..3 map to the original DirectSound values `-10000`, `-2222`, `-1111`, and `0`; use `--demo-volume 0..3` to compare them. SDL final master gain now defaults to 0 dB. Use `--audio-gain-db` only for device-specific adjustment within `-24..+18 dB`, checking positive values for clipping.

Use the diagnostic launcher entry shown above when detailed access-violation or API-boundary options are required. Debugger mode incurs a first-chance exception round trip for every legacy I/O instruction and is not a performance baseline. Original HDD data remains the read-only source; guest writes use the overlay policy, and diagnostic logs remain uncommitted.
