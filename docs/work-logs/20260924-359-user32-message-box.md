# 작업 359 작업 로그 — `user32!MessageBoxA` facade export / Task 359 work log — `user32!MessageBoxA` facade export

설계: [20260924-359-user32-message-box.md](../design/20260924-359-user32-message-box.md)
작업 지시서: [20260924-359-user32-message-box.md](../work-orders/20260924-359-user32-message-box.md)

## 변경 / Changes

- `user32` facade에 `MessageBoxA`(stdcall, 인자 4개)를 추가했다. 반환값은 `MessageBoxDefaultButton(uType)`로 정하는 기본 버튼 ID다. 버튼 ID 상수와 규칙은 `user32_module.h`에 공개했다.
  *Added `MessageBoxA` (stdcall, four arguments) to the `user32` facade, returning the default button ID from `MessageBoxDefaultButton(uType)`; the button-ID constants and the rule are public in `user32_module.h`.*
- `OriginalApiCall`에 `second_text`를 추가했다. continuation은 facade export의 ANSI 문자열 인자를 두 개까지 기록한다. `MessageBoxA`에서는 문구와 제목이다.
  *Added `second_text` to `OriginalApiCall`; the continuation records up to two ANSI string arguments of a facade export — the text and caption for `MessageBoxA`.*
- CLI는 게스트 문자열에서 인쇄 가능한 ASCII 밖의 byte를 `\xNN`으로 출력한다. 기존 ASCII 출력(예: `\\.\NTICE`)은 바뀌지 않는다.
  *The CLI prints guest-string bytes outside printable ASCII as `\xNN`; existing ASCII output such as `\\.\NTICE` is unchanged.*
- 단위 테스트는 descriptor의 export 2개, 각 버튼 종류의 기본 버튼, `MB_DEFBUTTON2/3`, 아이콘 bit, 범위 밖 fallback, handler 반환값과 인자 형태 거절을 검사한다.
  *Unit tests cover the two-export descriptor, each button set's default, `MB_DEFBUTTON2/3`, icon bits, out-of-range fallbacks, the handler result, and argument-shape rejection.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Linux x64·x86 debug build, CTest | 경고·오류 없음, 각각 3/3 통과 / no warnings or errors, 3/3 each |
| 두 폭 in-process probe, x86 guest-module probe | 통과(x86 `process-exit=7` 포함) / pass, including x86 `process-exit=7` |
| `scripts/test_linux_native_helper_probe.sh` | exit 0, 두 host 모두 `result=51` / both hosts |
| `--linux-in-process-*` 네 진단 / four diagnostics | 두 폭 모두 작업 358과 같음(trace 43 frame 포함) / same as Task 358, including the 43-frame trace |
| Windows x86 build | 오류·경고 없음 / no errors or warnings |
| Windows x86 CTest | 6개 중 5개 통과. `re2dj_windows_vfs_runtime_probe`는 작업 358에서 기존 문제로 확인한 같은 실패 / 5 of 6; the same pre-existing `re2dj_windows_vfs_runtime_probe` failure confirmed in Task 358 |

실제 4th CHD(`roms/ez2dj4th/4thTrax.chd`, 읽기 전용)의 기본 `re2dj ez2dj4th` 실행은 x64와 x86이 같다.

*The default `re2dj ez2dj4th` run on the real 4th CHD (`roms/ez2dj4th/4thTrax.chd`, read-only) is identical on x64 and x86:*

```text
#0019 user32.dll!MessageBoxA   ret=00ae4d9c args=00000000,<stack>,00ae72f0,00002010
      "Error 1009 : Cannot open Hardlock driver.\x0d\x0a" "Hardlock  " -> eax=00000001
#0020 kernel32.dll!GetProcAddress ret=00aeed26 args=6f000000,00aef4b4 "ExitProcess" -> eax=6f00204c
#0021 kernel32.dll!ExitProcess ret=00aeeba2 args=00000009 -> eax=00000000
continuation    : process exit, ExitProcess(0x00000009)
```

작업 358의 종료 경로(`ExitNativeGuestProcess`)가 실제 원본에서 처음 쓰였다. 두 폭 모두 fault 없이 정상 종료로 보고됐다. 문구는 Windows host의 3rd Hardlock 대화상자와 같다. 해석은 [분석 문서](../analysis/ez2dj4th-linux-inprocess-first-import.md)의 작업 359 절에 두었다.

*Task 358's exit path (`ExitNativeGuestProcess`) ran on the real original for the first time, reported as a normal exit without a fault on both widths. The text matches the Windows host's 3rd Hardlock dialog; the interpretation is in the Task 359 section of the [analysis](../analysis/ez2dj4th-linux-inprocess-first-import.md).*

## 다음 / Next

게스트는 Hardlock 장치 열기가 실패하자 스스로 종료했다. 이 경계를 넘으려면 Linux에도 Hardlock 장치 HLE가 필요하다. `\\.\NTICE`·`\\.\FEnteDev` 열기와 `DeviceIoControl` 경계를 Windows 작업 127·128의 플랫폼 중립 oracle과 연결하는 설계부터 시작한다.

*The guest ended itself after the Hardlock device opens failed. Getting past this needs a Hardlock device HLE on Linux too, starting with a design that connects the `\\.\NTICE`/`\\.\FEnteDev` opens and the `DeviceIoControl` boundary to the platform-neutral oracle of Windows Tasks 127–128.*
