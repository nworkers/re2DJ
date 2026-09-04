# Task 180: EZ2DJ 4th 자발적 종료 지점 관측

## 작업 목표

`ExitProcess(1)`을 부르는 게스트 코드 지점을 특정합니다.

## 선행 문서

- [Task 180 설계](../design/20260904-180-ez2dj4th-exit-process-source.md)
- [Task 179 작업 로그](../work-logs/20260904-179-direct3d7-vertex-buffer-facade.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)

## 구현 범위

1. **관측 wrapper 추가.** `src/platform/windows/injected_runtime.cpp`에 `Re2djHleExitProcess`를 추가합니다. 종료 코드, 호출자 반환 주소와 RVA, 반환 주소 앞 바이트 창을 기록한 뒤 실제 `ExitProcess`를 부릅니다.
2. **동적 resolver 연결.** `Re2djHleGetProcAddress`가 `ExitProcess`에 이 wrapper를 돌려주게 합니다.
3. **진단 실행과 판정.** 진입 추적을 실행해 기록된 호출자 RVA를 얻고, 설계 4절 기준으로 판정합니다. 필요하면 그 구간을 코드 범위 덤프로 읽습니다.
4. **문서 갱신.** 작업 로그와 `docs/analysis/ez2dj4th-hardlock-runtime.md`를 갱신하고 확인됨 / 추정 / 미확정을 구분해 표기합니다.

## 비범위

- 종료를 막거나 우회.
- Hardlock 응답 material 변경.
- 새 CLI 옵션 추가.

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
.\build\windows-x86\bin\Debug\re2dj_windows_product_loader_probe.exe
```

실제 CHD가 있으면 진입 추적을 실행합니다.

```powershell
$common = '--hdd', "$env:TEMP\re2dj\chd\ez2dj4th", '--target', 'ez2dj4th',
  '--chd', 'roms\ez2dj4th\4thTrax.chd',
  '--target-executable', 'EZ2DJ/EZ2DJ.EXE',
  '--hle-vfs', '--hle-dynamic-vfs', '--hle-d3d3', '--hle-io-ports',
  '--device-mock-lptdi', '--device-mock-lptdi-path-prefix', '\\.\FEnteDev',
  '--device-mock-wts-console-session', '--diagnostic-idle-timeout', '60000'
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe @common --null-context-entry-trace
```

## 자기 검증 기준

- `.vfs.log`에 `exit-process` 항목이 남고 종료 코드가 `exit_process` 디버그 이벤트의 값과 같아야 합니다.
- 기록된 반환 주소가 이미지 범위 안이면 RVA가 의미를 가집니다. 범위 밖이면 CRT 내부에서 부른 것이므로 한 단계 더 거슬러야 합니다.
- 로그에는 코드 바이트와 종료 코드만 남기고 원본 자산 내용은 남기지 않습니다.

---

# Task 180: Observing the EZ2DJ 4th Deliberate Exit

## Goal

Identify the guest code that calls `ExitProcess(1)`.

## Scope

1. Add a `Re2djHleExitProcess` observation wrapper that records the exit code, the caller's return address and RVA, and the bytes before it, then calls the real `ExitProcess`.
2. Return it from the dynamic resolver for `ExitProcess`.
3. Run the entry trace, read the recorded caller RVA, and apply the design's decision criteria, dumping that code range if needed.
4. Update the work log and the analysis topic.

## Out of Scope

Blocking or bypassing the exit, changing Hardlock material, and adding CLI options.

## Minimum Verification

Build, unit tests, and the product loader probe, then the entry trace against the real CHD.

## Self-Check

The VFS log must carry an `exit-process` record whose code matches the `exit_process` debug event. A return address inside the image makes the RVA meaningful; one outside means the call came from the CRT and needs one more frame. Logs record code bytes and the exit code only.
