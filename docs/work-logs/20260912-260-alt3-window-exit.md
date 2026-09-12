# 작업 로그: Alt+3 창 전환 중 종료 수정

## 결과

`ez2dj4th`의 제공 trace를 재검토한 결과, launcher handoff는 `outcome success`였고 실제 종료 직전 graphics trace에는 `window-lifetime:event=watcher-exit:valid=1:visible=0`가 있었다. 따라서 FAT32-LBA CHD, `EZ2DJ/EZ2DJ.EXE` 경로, profile 누락이 아니라 Win32 host 창 모드 전환과 lifetime watcher 사이의 경쟁 조건으로 확정했다.

## 구현

- `ConfigureRe2djHostWindow`가 host style에 `WS_VISIBLE`을 유지하도록 수정했다.
- `ApplyRe2djWindowMode`에 전환 guard를 추가했다.
- lifetime watcher는 HWND 무효화를 즉시 종료하되, 전환 중 숨김을 무시하고 전환 외 숨김은 20회 polling(50ms 간격, 약 1초) 뒤에만 종료한다.
- `Re2djExitIfWindowClosed`는 `Flip` 중 `visible=false`만으로 종료하지 않고 HWND 무효화만 즉시 종료하도록 조정했다.
- Windows runtime probe가 windowed/fullscreen/Alt+3 상태에서 `WS_VISIBLE`을 검증하도록 보강했다.
- CHD-backed VFS read-only 경로, profile 값, 원본 실행 파일은 변경하지 않았다.

## 검증

- `scripts\build_win32.bat`: 성공
- `ctest --test-dir build/windows-x86 -C Debug --output-on-failure -E re2dj_windows_vfs_runtime_probe`: 4/4 통과
- `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only`: exit code 0
- 전체 `re2dj_windows_vfs_runtime_probe`: 25초 timeout. 기존 장치/오디오 lifecycle 대기 구간에서 멈췄으며, timeout 전 창 정책 검사에서 실패 출력은 없었다.
- `git diff --check`: 통과

새 build로 실제 `ez2dj4th` 창에서 `Alt+3`을 누른 사용자-visible 결과는 아직 확인하지 못했다. 다만 기존 trace에서 종료를 직접 일으킨 `visible=0` watcher 경로를 제거했고, 자동 probe의 Alt+3 client-size 및 visibility 검사를 통과했다.

## English

### Result

Review of the supplied `ez2dj4th` trace shows a successful launcher handoff and `window-lifetime:event=watcher-exit:valid=1:visible=0` immediately before the observed exit. The cause is therefore classified as a race between Win32 host window-mode reconfiguration and the lifetime watcher, not a FAT32-LBA CHD issue, an `EZ2DJ/EZ2DJ.EXE` path issue, or a missing profile value.

### Implementation

- `ConfigureRe2djHostWindow` now preserves `WS_VISIBLE` in the host style.
- `ApplyRe2djWindowMode` now has a transition guard.
- The lifetime watcher still terminates immediately for an invalid HWND, ignores hiding during a mode transition, and requires 20 polls at 50 ms intervals (about one second) for hiding outside a transition.
- `Re2djExitIfWindowClosed` no longer terminates on `visible=false` alone during `Flip`; it terminates immediately only for an invalid HWND.
- The Windows runtime probe now checks `WS_VISIBLE` in windowed, fullscreen, and Alt+3 states.
- The CHD-backed read-only VFS path, profile values, and original executable were not changed.

### Verification

- `scripts\build_win32.bat`: passed
- Existing CTest excluding the lifecycle probe: 4/4 passed
- `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only`: exit code 0
- Full `re2dj_windows_vfs_runtime_probe`: timed out after 25 seconds in the existing device/audio lifecycle wait; no window-policy failure was printed before the timeout.
- `git diff --check`: passed

A user-visible rerun with the new build and the actual `ez2dj4th` window remains pending. The watcher path that directly caused the supplied exit has nevertheless been removed, and the automated Alt+3 client-size and visibility checks pass.
