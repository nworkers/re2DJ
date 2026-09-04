# Task 174: EZ2DJ 4th I/O out helper 트랩 연결

## 작업 목표

ez2dj4th 프로필이 out helper RVA를 명시하게 하여 `out dx, al`이 HLE I/O 포트 경로로 처리되도록 하고, 다음 중단 지점을 관측합니다.

## 선행 문서

- [Task 174 설계](../design/20260904-174-ez2dj4th-io-out-helper.md)
- [Task 173 작업 로그](../work-logs/20260904-173-ez2dj4th-ddcaps2-windowed.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)

## 구현 범위

1. **프로필 설정.** `src/target/target_profile.cpp`의 ez2dj4th 항목에 `legacy_io_out_byte_rva = 0x000c384b`를 설정하고, 근거를 주석으로 남깁니다.
2. **검증 기대값 갱신.** `src/tools/windows_product_loader_probe/main.cpp`의 ez2dj4th 프로필 단언을 새 값으로 고칩니다.
3. **진단 실행과 판정.** 실행 후 `out_dx_al` 항목이 first chance로만 남는지, 실행이 어디까지 진행하는지 관측합니다.
4. **문서 갱신.** 작업 로그와 `docs/analysis/ez2dj4th-hardlock-runtime.md`를 갱신합니다.

## 비범위

- 워드 폭과 dword 폭 helper 트랩 추가.
- 포트 응답 모델 변경.
- `direct3d3_com_facade`(DirectX 6 경로) 변경.

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

- `privileged_instruction` 항목 중 `first_chance:false`가 사라지면 out 경로가 트랩된 것입니다.
- `exit_process` 코드가 `0xc0000096`이 아니면 이 중단 지점이 해소된 것입니다.
- 로그에는 원본 자산 내용과 Hardlock secret 값을 기록하지 않습니다.

---

# Task 174: Wiring the EZ2DJ 4th I/O Out Helper Trap

## Goal

Name the out helper RVA in the ez2dj4th profile so `out dx, al` is served by the HLE I/O port path, then observe the next stopping point.

## Scope

1. Set `legacy_io_out_byte_rva = 0x000c384b` in the ez2dj4th profile with the evidence recorded as a comment.
2. Update the ez2dj4th profile assertion in the product loader probe.
3. Run the entry trace and check whether any `out_dx_al` reaches a second chance and how far execution gets.
4. Update the work log and the analysis topic.

## Out of Scope

Word-width and dword-width helper traps, port response model changes, and DirectX 6 path changes.

## Minimum Verification

Build, unit tests, and the product loader probe, then the entry trace against the real CHD.

## Self-Check

No `first_chance:false` entry among the `privileged_instruction` records means the out path is trapped, and an `exit_process` code other than `0xc0000096` means this stopping point is cleared. Logs never record original asset contents or Hardlock secrets.
