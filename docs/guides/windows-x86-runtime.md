# Windows x86 실행 가이드 / Windows x86 runtime guide

근거: [작업 446 설계(주입 폐기와 통합)](../design/20261004-446-windows-in-process-loader.md), [작업 448 설계(Windows backend)](../design/20261004-448-windows-x86-backend.md), [작업 449 설계(CLI 전환)](../design/20261004-449-windows-cli-in-process.md), [작업 450 로그(주입 경로 제거)](../work-logs/20261004-450-remove-windows-injection.md)

## 한국어

Windows 제품(`re2dj.exe`, 64비트 Windows에서 도는 Win32 x86 프로그램)은 작업 449부터 Linux와 같은 in-process 러너로 원본을 실행한다. 원본 EXE를 별도 프로세스로 띄우거나 DLL을 주입하지 않는다. 화면·입력·소리는 Linux와 같은 SDL3 host다.

### 실행

저장소 root의 PowerShell에서 프로파일 ID를 준다. CHD shortcut 프로파일은 `roms\<프로파일>` 아래의 CHD를 읽는다.

```powershell
.\build\windows-x86\bin\Debug\re2dj.exe ez2dj4th
.\build\windows-x86\bin\Debug\re2dj.exe ez2dj6th --fullscreen
.\build\windows-x86\bin\Debug\re2dj.exe ez2d2m --io-config .\config\ez2dancer-io.example.ini
```

창을 닫으면 끝난다. 6th처럼 런처가 게임을 자식으로 띄우는 프로파일은 자식도 같은 방식의 다른 re2dj 실행이다.

### 시작할 때 일어나는 일

- re2dj.exe는 0x60000000에 고정되어 있고, 시작하자마자 자기 자신을 일시 정지 상태로 한 번 더 띄운다. 새 프로세스의 로더가 돌기 전에 게스트 이미지 영역(0x00400000~0x04400000)을 예약해 두기 위해서다. 그래서 작업 관리자에는 `re2dj.exe`가 둘 보이며, 처음 것은 기다리기만 하고 둘째가 게임을 돌린다. 하나를 끝내면 다른 하나도 같이 끝난다.
- Hardlock 재료(`cfg\hardlock.ini` 등)는 현재 디렉터리의 `cfg\`에서 읽는다. 저장소 root에서 실행한다.

### 진단

- 실행별 로그: `logs\re2dj-<시각>.log`, API 호출 로그 `logs\re2dj-<시각>.api.log`.
- `--call-limit <n>`: 게스트 API 호출 n번 뒤 스스로 멈춘다(회귀 확인용).
- `--image-dump [--image-dump-delay <ms>]`: [이미지 덤프 가이드](decrypted-image-dump.md).
- `--linux-in-process-*`: 이름에 linux가 남아 있지만 Windows에서도 같은 진단으로 동작한다.

### OSD

실행 중 백틱(`` ` ``)으로 OSD를 열고 닫는다. 버전, 대상 프로파일, 실행 파일 이름과, 빌드가 확인된 프로파일이면 Autoplay 토글이 나온다. Autoplay는 곡을 시작할 때 읽히므로 곡 시작 전에 켠다. "32-bit color"는 표시 색 깊이(`--color-depth`)를 바꾼다.

### 바뀐 점

작업 450에서 주입 경로만 받던 옵션 `--demo-volume`, `--audio-volume-trace`, `--guest-wait-trace`, `--vsync`를 지웠다. 예전 VFS trace·launcher 진단 로그(`logs\windows_x86_launcher_probe\`)도 더는 생기지 않는다.

## English

From task 449 the Windows product (`re2dj.exe`, a Win32 x86 program on 64-bit Windows) runs the original through the same in-process runner as Linux, starting no separate original process and injecting no DLL; window, input and sound are the same SDL3 hosts.

Run it from the repository root with a profile ID, CHD shortcut profiles reading the CHD under `roms\<profile>` (examples above). Closing the window ends the run; a launcher's child, as in 6th, is another re2dj run of the same kind.

At start re2dj.exe, fixed at 0x60000000, starts itself once more suspended so the guest image range (0x00400000 to 0x04400000) is reserved before the new process's loader runs; Task Manager therefore shows two `re2dj.exe`, the first only waiting and the second running the game, and ending one ends both. Hardlock material (`cfg\hardlock.ini` and the rest) is read from `cfg\` in the current directory, so run from the repository root.

Diagnostics: per-run logs `logs\re2dj-<time>.log` and the API call log `.api.log`; `--call-limit <n>` stops the run on its own after n guest API calls; `--image-dump` is described in the [image dump guide](decrypted-image-dump.md); the `--linux-in-process-*` diagnostics work on Windows too despite their names.

Backtick (`` ` ``) shows and hides the OSD: version, target profile, executable name and, for a profile whose build is confirmed, the Autoplay toggle, read when a song starts, so tick it before. "32-bit color" switches the display depth (`--color-depth`).

Task 450 removed the options only the injection path took (`--demo-volume`, `--audio-volume-trace`, `--guest-wait-trace`, `--vsync`); the former VFS traces and launcher diagnostic logs (`logs\windows_x86_launcher_probe\`) are no longer written.
