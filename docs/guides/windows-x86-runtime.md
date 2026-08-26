# Windows x86 원본 실행 가이드

관련 설계: [렌더링 정확성·성능 회복](../design/20260826-072-render-correctness-performance.md)  
관련 작업 로그: [렌더링 정확성·성능 회복 작업 로그](../work-logs/20260826-072-render-correctness-performance.md)

저장소 root의 PowerShell에서 다음 command를 실행한다. `--hdd`에는 합법적으로 보유한 1st SE HDD dump 디렉터리를 지정한다.

```powershell
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe --hdd .\roms\ez2dj1stse --target ez2dj1stse --hle-command-line --hle-windows-directory --hle-vfs --hle-d3d3 --hle-directsound --hle-io-ports --run-detached --device-mock-lptdi-target-state 0900000000000000
```

`--run-detached`는 초기 복원과 IAT 검증 뒤 debugger를 분리하므로 실제 화면·오디오·입력과 성능 확인에 사용한다. 창을 닫으면 launcher도 종료된다. 상세 AV와 API 경계 진단이 필요하면 `--run-detached`만 빼고 실행한다. 이 경우 I/O마다 debugger exception 왕복이 발생하므로 성능 기준으로 사용하지 않는다.

원본 HDD는 읽기 전용 source로 사용하며 guest write는 `overlays/ez2dj1stse`로 보내는 정책이다. 실행 로그는 `logs/windows_x86_launcher_probe/ez2dj1stse`에 생성되며 저장소에는 commit하지 않는다.

---

# Windows x86 Original Runtime Guide

Run the command above from the repository root in PowerShell, replacing `--hdd` with the directory containing a legally owned 1st SE HDD dump. Use `--run-detached` for user-visible graphics, audio, input, and performance validation: the launcher restores and verifies the process, detaches the debugger, and lets the injected runtime handle the confirmed I/O helpers. Close the game window to finish the launcher.

Remove only `--run-detached` when detailed access-violation or API-boundary diagnostics are required. Debugger mode incurs a first-chance exception round trip for every legacy I/O instruction and is not a performance baseline. Original HDD data remains the read-only source; guest writes use the overlay policy, and diagnostic logs remain uncommitted.
