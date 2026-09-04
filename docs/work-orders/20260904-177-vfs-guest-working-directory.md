# Task 177: VFS 게스트 현재 디렉터리 추적

## 작업 목표

주입 런타임이 게스트의 현재 디렉터리를 추적하고 상대 이름을 그 기준으로 해석하게 하여, 게스트의 자원 적재가 성공하게 합니다.

## 선행 문서

- [Task 177 설계](../design/20260904-177-vfs-guest-working-directory.md)
- [Task 176 작업 로그](../work-logs/20260904-176-fat32-long-name-assembly.md)
- [Task 175 작업 로그](../work-logs/20260904-175-ez2dj4th-uninitialized-out-parameter.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)

## 구현 범위

1. **현재 디렉터리 상태.** `src/platform/windows/injected_runtime.cpp`에 HDD 루트 기준 구성요소 목록을 두고, 초기값은 루트로 둡니다.
2. **해석 일원화.** 상대 이름을 현재 디렉터리에 결합하는 해석을 한 곳에 두고 `MapVfsPath`와 `ChdRelativePath`가 함께 씁니다. 경로 파싱·정규화·결합은 `re2dj/storage/guest_path.h`를 사용합니다.
3. **`Re2djVfsSetCurrentDirectoryA`.** 요청을 해석하고 overlay·native·CHD 순으로 디렉터리 존재를 확인한 뒤 상태를 갱신합니다. 실패 시 상태를 유지하고 `FALSE`와 `ERROR_PATH_NOT_FOUND`를 돌려줍니다. 결과를 VFS 추적에 남깁니다.
4. **`Re2djVfsGetCurrentDirectoryA`.** 매핑된 native 절대 경로를 Win32 버퍼 규약대로 돌려줍니다.
5. **연결.** 동적 resolver와 정적 IAT patch 목록에 두 API를 추가합니다.
6. **문서 갱신.** 작업 로그와 `docs/analysis/ez2dj4th-hardlock-runtime.md`를 갱신하고, `ARCHITECTURE.md`에 현재 디렉터리 추적을 반영합니다.

## 비범위

- 호스트 프로세스 작업 디렉터리 변경.
- wide 문자 API.
- 드라이브별 현재 디렉터리 완전 구현.
- FAT32 쓰기 지원.

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

- `.vfs.log`에 `set-current-directory` 항목이 남고, 그 뒤 `2PLAYERInsertCoin.str` 요청이 `SYSTEM\Common` 아래로 매핑되어야 합니다.
- 실패 열기 34건이 줄어들어야 합니다.
- `exit_process` 코드가 `0xc0000005`가 아니면 이 중단 지점이 해소된 것입니다.
- 로그에는 파일 이름과 경로만 남기고 파일 내용은 남기지 않습니다.

---

# Task 177: Tracking the Guest Working Directory in the VFS

## Goal

Make the injected runtime track the guest's working directory and resolve relative names against it so the guest's resource loads succeed.

## Scope

1. Keep the guest's logical directory as components relative to the HDD root, starting at the root.
2. Resolve relative names against it in one place shared by `MapVfsPath` and `ChdRelativePath`, using the existing `re2dj/storage/guest_path.h` parser.
3. Add `Re2djVfsSetCurrentDirectoryA`, which accepts the change only when the directory exists in the overlay, the native tree, or the CHD, and traces the outcome.
4. Add `Re2djVfsGetCurrentDirectoryA`, returning the mapped native absolute path under the Win32 buffer contract.
5. Register both APIs with the dynamic resolver and the static IAT patch list.
6. Update the work log, the analysis topic, and `ARCHITECTURE.md`.

## Out of Scope

Host directory changes, wide-character APIs, per-drive current directories, and FAT32 write support.

## Minimum Verification

Build, unit tests, and the product loader probe, then the entry trace against the real CHD.

## Self-Check

The VFS log must show the directory change and then map `2PLAYERInsertCoin.str` under `SYSTEM\Common`, the 34 failed opens must drop, and an `exit_process` code other than `0xc0000005` means this stopping point is cleared. Logs record names and paths only.

---

## 범위 확장 기록 (Scope Extension Record)

검증 중 CHD 경로가 상대 경로로 주어지면 주입 런타임이 이미지를 열지 못한다는 것이 드러나, 한 항목을 추가했습니다.

- 런처가 `--chd` 경로를 절대 경로로 바꾼 뒤 런타임에 기록합니다. 런타임은 자식 프로세스 안에서 그 경로를 열고, 자식의 작업 디렉터리는 게스트 쪽이라 상대 경로가 조용히 실패합니다. 이 결함 때문에 CHD 읽기 경로가 한 번도 쓰이지 않았습니다.

Verification revealed that a relative `--chd` path never mounts inside the child, whose working directory is the guest's, so the launcher now makes the path absolute before writing it into the runtime. That defect had kept the CHD read path from ever being used.
