# Win32 창 닫기와 원본 프로세스 종료 작업 로그

관련 설계: [Win32 창 닫기와 원본 프로세스 종료](../design/20260829-090-window-close-process-exit.md)  
작업 지시: [Win32 창 닫기와 원본 프로세스 종료](../work-orders/20260829-090-window-close-process-exit.md)

## 결과

- 사용자가 작업 089 빌드에서 Music Select 중앙 곡 그림 복구를 확인했다.
- 같은 실행에서 제목 표시줄 닫기 뒤 창만 사라지고 `ez2dj.exe`가 남는 process-lifetime 결손을 확인했다.
- 고정 SDL 3.4.14 source를 대조해 external HWND의 `WM_CLOSE`가 SDL close event와 저장된 원본 WndProc 양쪽에 전달되는 구조를 확인했다.
- 1차 구현은 공용 backend가 close request를 sticky 상태로 보존하고 Windows `SurfaceFlip` 호출 thread에 `PostQuitMessage(0)`을 남겼지만 사용자 재검증에서 실패했다.
- rendering thread와 HWND owner/message-loop thread가 같다는 1차 가정을 기각하고 해당 shared-backend/Flip 종료 정책을 제거했다.
- `window_mode`가 원본 WndProc를 window property에 보존하고 adapter를 한 번만 설치하도록 보정했다.
- adapter는 HWND owner thread에서 `WM_CLOSE`를 원본 WndProc에 먼저 전달한 뒤 `PostQuitMessage(0)`을 호출하므로 다음 `Flip`이나 frame이 필요하지 않다.
- 2차 owner-thread `PostQuitMessage(0)`도 사용자 재검증에서 실패해 원본 cabinet message loop가 host close를 종료 조건으로 처리한다는 가정을 기각했다.
- 3차 `WM_CLOSE` 전용 `ExitProcess(0)` 보정도 사용자 재검증에서 실패했다. 실행 `20260829-101126-498`의 launcher 종료 코드가 `1`이므로 새 exit 경계가 실행되지 않은 것이 확인됐다.
- 제목 표시줄 X의 최초 메시지인 `WM_SYSCOMMAND/SC_CLOSE`를 host close 판정에 추가했다. 판정은 원본 WndProc 호출 전에 보존하며, 원본 정리 반환 뒤 `ExitProcess(0)`으로 process 전체를 정상 Win32 종료한다. 제품 경로에서는 `TerminateProcess`를 사용하지 않는다.
- 4차 `SC_CLOSE` 직접 판정도 사용자 재검증에서 실패했다. 실행 `20260829-101836-623` 역시 launcher 종료 코드 `1`을 기록했으므로 실제 close가 adapter 전체를 우회하는 것으로 범위를 좁혔다.
- Windows DirectDraw `SurfaceFlip`은 SDL event pump가 포함된 `Present` 직후, 성공 여부와 무관하게 저장된 원본 HWND의 파괴·숨김 상태를 검사한다. 유일한 표시 HWND가 사라졌으면 `ExitProcess(0)`을 호출한다.
- runtime probe child는 adapter 설치 뒤 WndProc를 의도적으로 덮어쓰고 `SC_CLOSE`에서 HWND만 직접 파괴해 message chain 우회를 모사한다. 새 lifetime fallback을 호출했을 때 parent는 child process 전체가 5초 안에 exit code 0으로 끝나는 것을 확인한다. timeout의 test child 정리에만 `TerminateProcess` fallback을 둔다.
- 5차 `Flip` 직후 lifetime fallback도 사용자 재검증에서 실패했다. 실행 `20260829-102517-424`이 종료 코드 `1`을 기록했으므로 창 소멸 뒤 guest가 다음 `Flip`을 호출하지 않는 것으로 확인했다.
- window mode 적용 완료 시 process당 하나의 watcher thread를 시작한다. 반복 적용은 `InterlockedExchangePointer`로 최신 HWND만 갱신하고 추가 thread를 만들지 않는다. watcher는 50 ms 간격으로 `IsWindow`/`IsWindowVisible`을 검사해 message/render loop와 독립적으로 `ExitProcess(0)`을 호출한다.
- 최종 child probe는 adapter 설치 뒤 WndProc를 교체하고 HWND만 파괴한 다음 직접 lifetime 함수를 부르지 않고 2초 대기한다. Debug에서는 약 0.35초, Release에서는 약 0.32초의 전체 runtime-probe 시간 안에 watcher가 child를 exit code 0으로 종료했다.
- watcher 적용 제품 실행 `20260829-111555-138`도 사용자 관찰상 종료되지 않고 launcher 종료 코드 `1`을 기록했다. 종료 후 process 조회에는 대상이 남지 않아 잔존 process identity는 아직 확인하지 못했다.
- `window_mode`는 기존 graphics trace에 watcher target, 초당 valid/visible/WndProc 표본, close message, `flip-exit`, `watcher-exit`을 기록한다. 표본은 최초 10분·최대 600개로 제한하고 HWND와 process/thread ID 외에 원본 자산이나 window text는 기록하지 않는다.
- runtime probe parent는 고유 임시 trace 경로를 child에 전달하고 종료 뒤 `event=target`, `event=sample`, `event=watcher-exit` 세 레코드를 모두 검사한다. 따라서 계측 코드가 compile된 것뿐 아니라 실제 worker에서 기록되는 것까지 자동 검증한다.
- 사용자가 계측 Debug 창을 닫은 실행 `20260829-112237-831`은 같은 HWND에서 close message 2회와 `visible=0`, `watcher-exit`을 확인했다. 즉 adapter와 watcher는 실제 제품에서도 정상 동작했다.
- 직후 PID 41488은 `HasExited=True`, thread 1개, handle 410개인 종료 중 상태로 남았고 parent PID 16892는 계속 대기했다. `taskkill /F`는 running instance가 없다고 했으며 Debug DLL 재링크는 `LNK1168`로 실패했다. 마지막 thread 강제 종료는 위험 정책상 사용자 명시 승인 없이 수행하지 않았다.
- host-close 종료 함수는 `ExitProcess(0)` 대신 current-process `TerminateProcess(..., 0)`을 사용한다. 원본 WndProc 정리 뒤 확인된 close 경계에서만 사용하며 launcher 외부 PID 종료로 확장하지 않는다.
- 사용자가 이전 교착 `ez2dj.exe`를 정리한 뒤 다음 Debug 실행에서 창 닫기 시 process 종료를 확인했다. 대응 로그 `20260829-112906-743`은 close message 2회, 같은 HWND의 `visible=0`, `watcher-exit`, `runtime_detached_exit` code `0x00000000`, 성공 outcome을 기록한다.
- 잠금 해제 뒤 Debug build와 CTest 3/3을 다시 수행해 통과했다. Release build와 CTest 3/3도 이미 통과했다.
- 원본 EXE 수정은 사용하지 않았다.

## 검증

- `cmake --build build\windows-x86 --config Debug`: 통과
- `ctest --test-dir build\windows-x86 -C Debug --output-on-failure`: 3/3 통과
- `cmake --build build\windows-x86 --config Release`: 통과
- `ctest --test-dir build\windows-x86 -C Release --output-on-failure`: 3/3 통과
- 원본 자산과 runtime 로그는 저장소에 추가하지 않았다.

## 남은 확인

없음. 실제 제품 사용자 검증, Debug/Release build와 CTest, exit code 0 로그 확인이 모두 완료됐다.

---

# Win32 Window Close and Original Process Exit Work Log

Related design: [Win32 Window Close and Original Process Exit](../design/20260829-090-window-close-process-exit.md)  
Work order: [Win32 Window Close and Original Process Exit](../work-orders/20260829-090-window-close-process-exit.md)

## Result

- The user confirmed restoration of the Music Select center artwork in the Task 089 build.
- The same run confirmed a process-lifetime gap: title-bar close removed the window but left `ez2dj.exe` alive.
- Pinned SDL 3.4.14 source confirms that external-HWND `WM_CLOSE` reaches both an SDL close event and the saved original WndProc.
- The first implementation retained close in the shared backend and posted quit on the Windows `SurfaceFlip` caller thread, but failed user revalidation.
- The assumption that rendering and HWND owner/message-loop threads are identical is rejected, and the shared-backend/Flip lifetime policy was removed.
- `window_mode` now preserves the original WndProc in a window property and installs exactly one adapter.
- On the HWND owner thread, the adapter forwards `WM_CLOSE` to the original WndProc first and then calls `PostQuitMessage(0)`, requiring no later `Flip` or frame.
- The second owner-thread `PostQuitMessage(0)` approach also failed user revalidation, rejecting the assumption that the original cabinet message loop treats host close as termination.
- The third `WM_CLOSE`-only `ExitProcess(0)` correction also failed user revalidation. Launcher exit code `1` in run `20260829-101126-498` confirms that the new exit boundary did not execute.
- The host-close predicate now includes the title bar's initial `WM_SYSCOMMAND/SC_CLOSE`. It is preserved before invoking the original WndProc, and normal full-process Win32 termination through `ExitProcess(0)` follows original cleanup. The product path does not use `TerminateProcess`.
- The fourth direct `SC_CLOSE` detection also failed user revalidation. Run `20260829-101836-623` again recorded launcher exit code `1`, narrowing actual close to bypassing the entire adapter.
- Immediately after `Present`, which includes the SDL event pump, Windows DirectDraw `SurfaceFlip` checks the retained original HWND for destruction or hiding regardless of presentation success. It calls `ExitProcess(0)` when the only display HWND is gone.
- The runtime-probe child deliberately overwrites the WndProc after adapter installation and destroys only the HWND on `SC_CLOSE`, reproducing message-chain bypass. After invoking the lifetime fallback, the parent confirms full-process exit code zero within five seconds. `TerminateProcess` is only a timeout cleanup fallback for the test child.
- The fifth immediate post-`Flip` lifetime fallback also failed user revalidation. Run `20260829-102517-424` recorded exit code `1`, confirming that the guest does not call another `Flip` after window disappearance.
- Successful window-mode application starts one watcher thread per process. Repeated application updates only the latest HWND through `InterlockedExchangePointer` without creating another thread. Every 50 ms, the watcher checks `IsWindow`/`IsWindowVisible` and calls `ExitProcess(0)` independently of message/render-loop progress.
- The final child probe replaces the WndProc after adapter installation, destroys only the HWND, and waits for two seconds without directly invoking the lifetime function. The watcher exits the child with code zero within roughly 0.35 seconds of total Debug probe time and 0.32 seconds in Release.
- Watcher-enabled product run `20260829-111555-138` still did not terminate by user observation and recorded launcher exit code `1`. No matching process remained when inspected afterward, so the identity of the observed remaining process is unresolved.
- `window_mode` now records watcher target, one-per-second valid/visible/WndProc samples, close messages, `flip-exit`, and `watcher-exit` in the existing graphics trace. Samples are bounded to the first ten minutes and 600 records; no original assets or window text are recorded beyond HWND and process/thread IDs.
- The runtime-probe parent supplies a unique temporary trace path to its child and requires `event=target`, `event=sample`, and `event=watcher-exit` after termination. This verifies actual worker-side recording rather than compilation alone.
- In instrumented Debug run `20260829-112237-831`, closing the window records two close messages, `visible=0`, and `watcher-exit` for the same HWND. The adapter and watcher therefore work in the actual product.
- Immediately afterward, PID 41488 remains terminating with `HasExited=True`, one thread, and 410 handles while parent PID 16892 keeps waiting. `taskkill /F` reports no running instance and relinking the Debug DLL fails with `LNK1168`. Risk policy rejected last-thread force termination without explicit user approval.
- The host-close termination function now uses current-process `TerminateProcess(..., 0)` instead of `ExitProcess(0)`. It is limited to the confirmed close boundary after original-WndProc cleanup and is not expanded to launcher-side arbitrary-PID termination.
- After the user cleared the previous deadlocked `ez2dj.exe`, the next Debug run terminated the process on window close. Corresponding log `20260829-112906-743` records two close messages, `visible=0` and `watcher-exit` for the same HWND, `runtime_detached_exit` code `0x00000000`, and a successful outcome.
- After the lock was released, the Debug build and CTest 3/3 passed again. The Release build and CTest 3/3 had already passed.
- The original executable is not modified.

## Verification

- `cmake --build build\windows-x86 --config Debug`: passed
- `ctest --test-dir build\windows-x86 -C Debug --output-on-failure`: 3/3 passed
- `cmake --build build\windows-x86 --config Release`: passed
- `ctest --test-dir build\windows-x86 -C Release --output-on-failure`: 3/3 passed
- Original assets and runtime logs were not added to the repository.

## Remaining validation

None. Actual-product user validation, Debug/Release builds and CTest, and the exit-code-zero log are all complete.
