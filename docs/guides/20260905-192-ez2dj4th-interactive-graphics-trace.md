# ez2dj4th 입력 포함 그래픽 trace 절차
# ez2dj4th Interactive Graphics Trace Procedure

## 목적 (Purpose)

코인을 실제로 넣고 게임을 진행하여 `ez2dj4th` Music Select에 진입한 뒤, 해당 화면의 Direct3D draw 상태를 남긴다. 원본 CHD와 HDD는 사용자의 외부 경로에서만 읽으며 저장소에 복사하지 않는다.

Run the original `ez2dj4th` far enough to insert a coin and enter Music Select, then record the Direct3D draw state for that screen. The original CHD and HDD are read only from user-supplied external paths and are not copied into the repository.

## 사전 조건 (Prerequisites)

- 현재 Debug 빌드가 있어야 한다: `build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe`.
- CHD 실행용 staging root에 `EZ2DJ\EZ2DJ.EXE`가 있어야 한다. 기본 경로는 `$env:TEMP\re2dj\chd\ez2dj4th`이다.
- `roms\ez2dj4th\4thTrax.chd`와 보호 설정 `cfg\hardlock.ini`, `cfg\hardlock-ez2dj4th.map`이 현재 환경에 있어야 한다.
- 게임 창이 foreground여야 `GetAsyncKeyState` 기반 키 입력이 전달된다.

- A current Debug build is required: `build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe`.
- The CHD staging root must contain `EZ2DJ\EZ2DJ.EXE`. The default path is `$env:TEMP\re2dj\chd\ez2dj4th`.
- `roms\ez2dj4th\4thTrax.chd` and the protection configuration `cfg\hardlock.ini`, `cfg\hardlock-ez2dj4th.map` must be available in the current environment.
- The game window must be foreground for `GetAsyncKeyState`-based input to be delivered.

## 실행 (Run)

프로젝트 루트의 PowerShell에서 아래를 실행한다. `--run-detached`는 launcher probe가 입력 설정을 전달할 때 필요한 모드이며, 게임 창이 닫힐 때까지 명령이 대기한다.

Run this from a PowerShell at the project root. `--run-detached` is required by the launcher probe when passing the input configuration, and the command waits until the game window exits.

```powershell
$hdd = "$env:TEMP\re2dj\chd\ez2dj4th"
$chd = (Resolve-Path 'roms\ez2dj4th\4thTrax.chd').Path
$io = (Resolve-Path 'config\ez2dj-io.example.ini').Path
$args = @(
  '--hdd', $hdd,
  '--target', 'ez2dj4th',
  '--chd', $chd,
  '--target-executable', 'EZ2DJ/EZ2DJ.EXE',
  '--hle-vfs',
  '--hle-dynamic-vfs',
  '--hle-d3d3',
  '--hle-directsound',
  '--hle-io-ports',
  '--io-config', $io,
  '--device-mock-lptdi',
  '--device-mock-lptdi-path-prefix', '\\.\FEnteDev',
  '--device-mock-wts-console-session',
  '--diagnostic-idle-timeout', '600000',
  '--run-detached'
)
& '.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe' @args
```

## 입력 순서 (Input Sequence)

게임 창을 클릭해 foreground로 만든 뒤, 예제 설정 기준으로 다음을 수행한다.

After focusing the game window, use the example configuration for the following sequence:

1. `F3`을 눌러 코인을 넣는다.
2. `Enter`로 1P를 시작한다.
3. 게임의 모드/스타일 선택을 진행한다. 선곡 화면에서 원본과 같은 곡을 선택하면 비교가 가장 명확하다.
4. 중앙 artwork가 표시된 상태를 5초 이상 유지한다.
5. 화면 캡처가 끝나면 게임 창을 정상적으로 닫는다.

1. Press `F3` to insert a coin.
2. Press `Enter` to start 1P.
3. Proceed through the mode/style selection. Selecting the same song as the reference gives the clearest comparison.
4. Keep the center artwork visible for at least five seconds.
5. Close the game window normally after the screen capture.

## 로그 확인 및 전달 (Inspect And Share Logs)

실행이 끝나면 다음 세 파일을 확인한다. timestamp는 실행마다 달라진다.

After the run exits, identify these three timestamped files:

```powershell
$logDir = 'logs\windows_x86_launcher_probe\ez2dj4th'
Get-ChildItem $logDir -File |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 6 Name,Length,LastWriteTime
```

- `<timestamp>.jsonl`: 실행 준비, `io_port_runtime`, 예외와 종료 상태.
- `<timestamp>.ddraw.log`: `LateDraw`와 Direct3D stage/blend/color-key 상태.
- `<timestamp>.vfs.log`: CHD/VFS 파일 접근 문제 발생 시 보조 자료.

- `<timestamp>.jsonl`: launch preparation, `io_port_runtime`, exceptions, and exit status.
- `<timestamp>.ddraw.log`: `LateDraw` and Direct3D stage/blend/color-key state.
- `<timestamp>.vfs.log`: supplemental evidence if CHD/VFS file access fails.

가장 최근 실행의 중앙·측면 후보 draw를 빠르게 확인하려면 다음을 실행한다. texture ID는 실행마다 달라질 수 있으므로 마지막 frame의 draw를 함께 전달하는 것이 좋다.

To quickly inspect the latest run's center/side candidate draws, run the following. Texture IDs may differ between runs, so include the draws from the last frame when sharing the evidence.

```powershell
$draw = Get-ChildItem $logDir -Filter '*.ddraw.log' -File |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1
rg 'LateDraw:.*(alphaop|srcblend|dstblend|colorkey|minfilter|magfilter|addressu|addressv)' $draw.FullName | Select-Object -Last 30
```

`jsonl`, `ddraw.log`, `vfs.log`만 전달하면 되며 CHD, IMG, 실행 파일, 원본 자산은 전달하지 않는다.

Share only the `jsonl`, `ddraw.log`, and `vfs.log` files. Do not share the CHD, IMG, executable, or other original assets.

## 확인 포인트 (Acceptance Points)

- `.jsonl`에 `io_port_runtime`와 `config\ez2dj-io.example.ini`의 절대 경로가 준비된 실행으로 기록되어야 한다.
- F3 입력 후 coin counter 관련 draw/상태가 변하고, Enter 이후 선곡 화면 draw가 관찰되어야 한다.
- 중앙 artwork 후보 draw에서 `alphaop`, `alphaarg1`, `alphaarg2`, `srcblend`, `dstblend`, `key`, `colorkey`를 확인한다.

- The `.jsonl` must show a prepared `io_port_runtime` for a run that supplied the absolute path to `config\ez2dj-io.example.ini`.
- After F3, the coin counter-related state should change, and after Enter, the song-selection draw should be observed.
- Inspect `alphaop`, `alphaarg1`, `alphaarg2`, `srcblend`, `dstblend`, `key`, and `colorkey` on the center-artwork candidate draw.

관련 분석은 [Task 191 그래픽 경로 분석](../analysis/ez2dj4th-graphics-path.md)과 [Task 191 작업 로그](../work-logs/20260905-191-alpha-stage-state-diagnostics.md)에 있다.

Related analysis is in [Task 191 Graphics Path Analysis](../analysis/ez2dj4th-graphics-path.md) and the [Task 191 Work Log](../work-logs/20260905-191-alpha-stage-state-diagnostics.md).
