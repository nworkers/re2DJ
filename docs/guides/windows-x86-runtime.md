# Windows x86 원본 실행 가이드

관련 설계: [렌더링 정확성·성능 회복](../design/20260826-072-render-correctness-performance.md)

관련 제품 loader 설계: [Win32 제품 loader 통합](../design/20260828-079-win32-product-loader.md)

관련 오디오 설계: [Win32 오디오 master gain](../design/20260828-080-win32-audio-master-gain.md)

관련 작업 로그: [렌더링 정확성·성능 회복](../work-logs/20260826-072-render-correctness-performance.md), [Win32 제품 loader 통합](../work-logs/20260828-079-win32-product-loader.md)

저장소 root의 PowerShell에서 제품 loader를 실행한다. `--hdd`에는 합법적으로 보유한 1st SE HDD dump 디렉터리를 지정한다.

```powershell
.\build\windows-x86\bin\Debug\re2dj.exe --hdd .\roms\ez2dj1stse --target ez2dj1stse --run
```

제품 facade는 command line, Windows directory, VFS, DirectDraw/Direct3D 3, DirectSound, legacy I/O port, LPTDI target state와 detached 실행의 현재 canonical policy를 선택한다. 초기 복원과 IAT 검증 뒤 debugger를 분리하므로 실제 화면·오디오·입력과 성능 확인에 사용한다. 창을 닫으면 loader도 종료된다.

Win32 출력에는 기본 `+6 dB` master gain이 적용된다. 더 크게 들으려면 아래처럼 `12`를 시도하고, clipping이나 왜곡이 들리면 `6`, `3`, `0` 순서로 낮춘다. 허용 범위는 `-24..+18 dB`다.

```powershell
.\build\windows-x86\bin\Debug\re2dj.exe --hdd .\roms\ez2dj1stse --target ez2dj1stse --audio-gain-db 12 --run
```

상세 AV와 API 경계 진단에는 호환 유지되는 진단 entry를 사용한다.

```powershell
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe --hdd .\roms\ez2dj1stse --target ez2dj1stse --hle-command-line --hle-windows-directory --hle-vfs --hle-d3d3 --hle-directsound --hle-io-ports --device-mock-lptdi-target-state 0900000000000000 --api-trace
```

debugger mode는 I/O마다 exception 왕복이 발생하므로 성능 기준으로 사용하지 않는다.

원본 HDD는 읽기 전용 source로 사용하며 guest write는 `overlays/ez2dj1stse`로 보내는 정책이다. 실행 로그는 `logs/windows_x86_launcher_probe/ez2dj1stse`에 생성되며 저장소에는 commit하지 않는다.

---

# Windows x86 Original Runtime Guide

Run the product-loader command above from the repository root in PowerShell, replacing `--hdd` with a legally owned 1st SE HDD directory. The facade selects the current canonical command-line, Windows-directory, VFS, graphics, audio, legacy-I/O, LPTDI, and detached-runtime policy. It restores and verifies the process, detaches the debugger, and lets the injected runtime handle confirmed boundaries. Close the game window to finish the loader.

Win32 output applies `+6 dB` master gain by default. Try `--audio-gain-db 12` for more output, reducing it to `6`, `3`, or `0` if clipping or distortion is audible. The accepted range is `-24..+18 dB`; this master adjustment preserves relative DirectSound buffer levels.

Use the diagnostic launcher entry shown above when detailed access-violation or API-boundary options are required. Debugger mode incurs a first-chance exception round trip for every legacy I/O instruction and is not a performance baseline. Original HDD data remains the read-only source; guest writes use the overlay policy, and diagnostic logs remain uncommitted.
