# 작업 로그 321: Linux ez2dj4th first-boundary observation / Work log 321: Linux ez2dj4th first-boundary observation

## 결과 / Result

초기 실행은 Linux CLI가 CHD-backed `--run`을 Windows x86 launcher 전용으로 제한해 원본 코드 진입 전에 중단됐습니다. `Fat32Volume::MaterializeFile()`은 이미 플랫폼 중립이므로, canonical PE staging helper를 Windows 전용 preprocessor guard에서 분리하고 Linux branch가 staged PE를 `RunOriginalUntilBoundary()`에 전달하게 했습니다. staging은 temporary path에만 쓰며 4thTrax CHD는 변경하지 않습니다.

*The initial run stopped before original-code entry because the Linux CLI limited CHD-backed `--run` to the Windows x86 launcher. `Fat32Volume::MaterializeFile()` was already platform-neutral, so the canonical-PE staging helper was moved out of its Windows-only preprocessor guard and the Linux branch now passes the staged PE to `RunOriginalUntilBoundary()`. Staging writes only to a temporary path and does not modify 4thTrax CHD.*

`istreambuf_iterator` 뒤 `eof()`가 항상 set된다고 가정한 Linux runner reader도 staging PE를 읽지 못하게 했습니다. reader는 empty input 또는 stream `bad()`만 실패로 처리하도록 수정했습니다.

*The Linux runner reader also prevented staged PE reading by assuming `eof()` is always set after `istreambuf_iterator`. It now fails only for empty input or a bad stream.*

## 관찰 / Observation

WSL Ubuntu 24.04 Linux x64에서 `re2dj ez2dj4th --run --linux-helper <i386 helper>`를 실행했습니다. helper는 ELF32 i386였고, CHD FAT32 label은 `EZ2DJ3_S`였습니다. canonical PE가 `0x00400000`에 적재되어 entry `0x00ae0240`을 보고한 뒤 첫 boundary가 `kernel32.dll!GetModuleHandleA`였습니다.

*Ran `re2dj ez2dj4th --run --linux-helper <i386 helper>` on Linux x64 under WSL Ubuntu 24.04. The helper was ELF32 i386 and the CHD FAT32 label was `EZ2DJ3_S`. The canonical PE loaded at `0x00400000`, reported entry `0x00ae0240`, and reached `kernel32.dll!GetModuleHandleA` as its first boundary.*

Linux x86 product는 clean rebuild 후 같은 CHD staging 경로와 i386 helper로 실행 command가 성공 종료했습니다. `bash scripts/test_linux_native_helper_probe.sh`도 x64·x86 CTest와 기존 helper probe를 포함해 성공 종료했습니다.

*The Linux x86 product completed successfully after a clean rebuild using the same CHD staging path and i386 helper. `bash scripts/test_linux_native_helper_probe.sh` also completed successfully with x64/x86 CTest and the existing helper probes.*

이 결과는 첫 import metadata 관찰이며 `GetModuleHandleA` binding 또는 protection/game execution 성공이 아닙니다. 다음 작업은 해당 API ABI와 process/module identity를 원본 관찰에 근거해 설계하는 것입니다.

*This result observes first-import metadata; it is not a `GetModuleHandleA` binding or successful protection/game execution. The next task designs that API ABI and process/module identity from original observations.*
