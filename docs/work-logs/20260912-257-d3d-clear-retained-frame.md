# 작업 로그: Direct3D Clear와 retained frame 연결

## 작업 요약

`ez2d2m`과 `ez2dj4th`의 잔상처럼 보이는 화면을 profile 설정과 공통 HLE로 나누어 조사했습니다. 두 실행 모두 primary surface에 `caps=0x00002218`, `back_buffers=1`을 요청하므로 retained-frame 선택은 profile 누락이 아니었습니다. 두 실행의 `LegacyDeviceClear`는 첫 draw 전에 발생하지만 기존 HLE가 trace만 남기고 logical render target을 지우지 않는 것이 직접 원인으로 확인되었습니다.

## 변경 내용

- `D3DCOLOR` RGB를 RGB565로 변환하는 공통 facade helper를 추가했습니다.
- 현재 device render target이 presentation surface인 전체 대상 `D3DCLEAR_TARGET`을 `Sdl3OpenGlBackend::ClearRenderTarget`으로 전달합니다.
- backend가 아직 생성되지 않은 첫 frame의 clear는 root pending 상태로 보존하고 첫 draw 전에 적용합니다.
- 전체 화면 `DDBLT_COLORFILL`도 같은 pending/immediate clear 요청 경계를 사용합니다.
- CPU surface backing이 있으면 동일한 RGB565 값으로 갱신해 lock/readback 상태를 일치시킵니다.
- 관련 설계, 작업 지시서, 분석 index, architecture를 추가·갱신했습니다.

## 검증

1. `powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "cmd /c scripts\build_win32.bat"`
   - 성공. Windows x86 Debug 제품 및 runtime DLL을 포함한 전체 빌드가 완료되었습니다.
2. `build\windows-x86\bin\Debug\re2dj_unit_tests.exe`
   - 성공: `checks: 1679, failures: 0`
3. `ez2d2m` 실제 launcher probe
   - 새 trace: `logs/windows_x86_launcher_probe/ez2d2m/20260912-183024-164.ddraw.log`
   - `CreateSurface:caps=0x00002218:back_buffers=1` 및 첫 frame 전후의 `LegacyDeviceClear`를 확인했습니다.
   - trace는 frame 3315까지 생성되었고, 검증 후 해당 interactive process를 종료했습니다.
4. `ez2dj4th` launcher probe
   - 새 실행은 `runtime_detached`까지 도달했지만 `create_ex_patched=false`인 보호/동적 import 조건에서 새 DDraw trace를 충분히 만들지 못했습니다.
   - 기존 실행 trace `logs/windows_x86_launcher_probe/ez2dj4th/20260912-015202-423.ddraw.log`에서 동일한 `caps=0x00002218`, `LegacyDeviceClear:flags=0x00000003`, `Flip` 경로를 재확인했습니다.
5. `git diff --check`
   - whitespace 오류가 없습니다.

## 확인하지 못한 것

새 build의 최종 화면을 두 profile 모두에서 사용자 시각으로 비교하는 단계는 남아 있습니다. 부분 `D3DRECT` clear와 presentation surface가 아닌 render target은 이번 작업의 범위에 포함하지 않았습니다.

## English

### Summary

The afterimage-like output in `ez2d2m` and `ez2dj4th` was investigated as either a profile setting or a shared HLE defect. Both runs request a primary surface with `caps=0x00002218` and `back_buffers=1`, so retained-frame selection was not missing from the profiles. Both issue `LegacyDeviceClear` before the first draw, while the previous HLE only traced the call and did not clear the logical render target. That was confirmed as the direct cause.

### Changes

- Added a shared facade helper converting RGB components of `D3DCOLOR` to RGB565.
- Forwarded full-target `D3DCLEAR_TARGET` from the current presentation render target to `Sdl3OpenGlBackend::ClearRenderTarget`.
- Preserved a clear arriving before backend creation as pending root state and applied it before the first draw.
- Sent full-surface `DDBLT_COLORFILL` through the same pending/immediate clear boundary.
- Updated CPU surface backing with the same RGB565 value when available, keeping lock/readback state coherent.
- Added or updated the related design, work order, analysis index, and architecture documentation.

### Verification

1. `powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "cmd /c scripts\build_win32.bat"`
   - Passed. The complete Windows x86 Debug product and runtime DLL build completed.
2. `build\windows-x86\bin\Debug\re2dj_unit_tests.exe`
   - Passed: `checks: 1679, failures: 0`.
3. Actual `ez2d2m` launcher probe
   - New trace: `logs/windows_x86_launcher_probe/ez2d2m/20260912-183024-164.ddraw.log`.
   - Confirmed `CreateSurface:caps=0x00002218:back_buffers=1` and `LegacyDeviceClear` around the first frames.
   - The trace reached frame 3315; the interactive process was stopped after verification.
4. `ez2dj4th` launcher probe
   - The new run reached `runtime_detached` but did not produce a sufficient new DDraw trace because the protected/dynamic-import condition reported `create_ex_patched=false`.
   - The existing runtime trace `logs/windows_x86_launcher_probe/ez2dj4th/20260912-015202-423.ddraw.log` reconfirmed the same `caps=0x00002218`, `LegacyDeviceClear:flags=0x00000003`, and `Flip` path.
5. `git diff --check`
   - Passed with no whitespace errors.

### Not verified

A final user-visible comparison of the new build in both profiles remains pending. Partial `D3DRECT` clears and render targets other than the presentation surface were outside this task.
