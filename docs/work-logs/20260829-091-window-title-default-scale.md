# Win32 실행 창 제목과 기본 2배 확대 작업 로그

관련 설계: [Win32 실행 창 제목과 기본 2배 확대](../design/20260829-091-window-title-default-scale.md)  
작업 지시: [Win32 실행 창 제목과 기본 2배 확대](../work-orders/20260829-091-window-title-default-scale.md)

## 한국어

### 결과

- injected runtime과 runtime probe에 저장소 `VERSION` 값을 compile definition으로 전달했다.
- 창 제목을 `re2DJ v<version> - Build <date> - SDL3 OpenGL - FPS : <value>` 형식으로 변경했다.
- DirectDraw의 성공한 `Flip`을 `QueryPerformanceCounter`로 측정하고 약 1초마다 소수점 한 자리 FPS를 갱신한다.
- windowed client 크기를 원본 논리 표시 640×480의 가로·세로 2배인 1280×960으로 변경했다. 논리 display mode, surface 크기와 guest 좌표는 변경하지 않았다.
- 확대 계산 전에 Win32 `LONG` 범위 overflow를 검사한다.
- fullscreen은 기존 monitor bounds `WS_POPUP` 정책을 그대로 유지한다.
- runtime probe가 현재 version과 제목 구성 요소, 초기 `FPS : 0.0`, 표본 `FPS : 59.9`, 1280×960 client 크기, windowed style과 fullscreen monitor bounds를 확인한다.
- 원본 실행 파일과 자산은 수정하거나 저장소에 추가하지 않았다.
- 사용자 제품 검증에서 client 확대는 적용됐지만 caption 제목과 아이콘이 그려지지 않는 현상을 확인했다. client만 2배이고 system non-client metric은 그대로이므로 상대적으로 얇아 보이는 부분과 실제 text 결손을 구분했다.
- 원본 popup WndProc가 text/non-client 메시지를 소비해도 `window_mode` adapter가 `WM_SETTEXT`/`WM_GETTEXT`, non-client 계산·그리기·hit-test와 close 이외 system command를 `DefWindowProcA`로 직접 처리한다.
- windowed style을 `WS_OVERLAPPEDWINDOW`로 변경해 표준 resize frame과 maximize button을 제공하고, class icon이 없는 원본 창에는 Windows 기본 application icon을 설정한다.
- runtime probe의 WndProc가 text/non-client 처리를 의도적으로 소비하도록 바꾸고도 제목, icon, `HTCAPTION`, style, 1280×960 client와 fullscreen 복귀가 모두 유지됨을 확인했다.
- 사용자 재검증에서 첫 보정 빌드도 maximize style과 1280×960 client는 적용됐지만 제목·icon은 처음부터 비어 있었다. title 성공 뒤에만 client resize에 도달하는 코드 순서로 초기 설정 실패를 배제하고 후속 replacement 경로로 범위를 좁혔다. 실제 writer는 아직 미확정이다.
- host 제목 갱신은 `SetWindowTextA` 대신 현재 WndProc chain을 우회하는 직접 `DefWindowProcA(WM_SETTEXT)`를 사용한다. adapter로 들어오는 일반 `WM_SETTEXT`와 `WM_SETICON` replacement는 성공 처리하되 보존된 host 상태를 바꾸지 않는다.
- runtime probe는 host 설정 뒤 빈 제목과 null icon overwrite를 시도하고도 제목 전체와 icon이 유지되는지 검증한다.
- Release 진단에서 실제 제목은 올바르게 저장됐지만 probe의 `"re2DJ v"` 7글자 prefix를 8글자로 비교하던 오류가 드러났다. 비교 길이를 실제 문자열 길이로 고쳤고 Release runtime probe 5회 연속 및 최종 Debug/Release 전체 CTest가 통과했다.
- 반복된 사용자 재검증으로 host 상태 보존만으로 caption이 복구된다는 기존 결론을 폐기했다. 실제 제품 진단 실행 `20260829-125238-338`에서 저장 제목 길이와 prefix, icon, style은 정상이었고 SDL의 빈 `WM_SETTEXT`도 차단됐으며, SDL external-window wrapping 뒤 WndProc가 변경된 사실을 확인했다.
- non-client DC에 제목을 직접 그리는 실험은 DWM에 덮여 표시되지 않았다. 같은 실행 중 `DWMNCRP_DISABLED`를 적용하자 Windows 기본 제목과 icon이 즉시 나타났고 임시 수동 text와 겹쳤으므로, 수동 caption 그리기는 최종안에서 제거했다.
- 최종 구현은 `SetWindowPos(...SWP_FRAMECHANGED)` 뒤 windowed에 `DWMNCRP_DISABLED`, fullscreen에 `DWMNCRP_ENABLED`를 적용하고 frame을 다시 그린다. Windows system `dwmapi` 외의 의존성은 추가하지 않았다.
- 최종 제품 실행 `20260829-161622-270`과 DPI 보정 캡처 `build/window-caption-final.png`에서 icon과 상태 제목이 중복 없이 한 번 표시되고, `FPS : 60.0`과 1280×960 client 영역이 함께 적용된 것을 확인했다. 진단 창은 `WM_CLOSE`로 닫았고 loader와 원본 process가 함께 정상 종료됐다. 캡처는 빌드 산출물이므로 저장소에는 포함하지 않는다.

### 검증

- `cmake --build build\windows-x86 --config Debug`: 통과
- `ctest --test-dir build\windows-x86 -C Debug --output-on-failure`: 3/3 통과
- `cmake --build build\windows-x86 --config Release`: 통과
- `ctest --test-dir build\windows-x86 -C Release --output-on-failure`: 3/3 통과

### 남은 확인

실제 제품 직접 검증은 통과했다. 동일 환경에서 제목·아이콘과 caption 크기가 올바르게 보이는지 최종 사용자 확인만 남는다.

## English

### Result

- The repository `VERSION` is passed to the injected runtime and runtime probe as a compile definition.
- The window title now follows `re2DJ v<version> - Build <date> - SDL3 OpenGL - FPS : <value>`.
- Successful DirectDraw `Flip` calls are measured with `QueryPerformanceCounter`, updating one-decimal FPS about once per second.
- The windowed client is now 1280x960, twice the original 640x480 logical display in each dimension. The logical display mode, surface sizes, and guest coordinates remain unchanged.
- The scale calculation checks the Win32 `LONG` range before multiplication.
- Fullscreen retains the existing monitor-bounds `WS_POPUP` policy.
- The runtime probe checks the current version and title components, initial `FPS : 0.0`, sample `FPS : 59.9`, the 1280x960 client, windowed style, and fullscreen monitor bounds.
- Neither the original executable nor its assets were modified or added to the repository.
- User product validation showed that client scaling applied but caption title text and icon were not painted. The correction distinguishes the expected proportional thinness of unchanged system non-client metrics from the actual missing-text defect.
- Even when the original popup WndProc consumes text/non-client messages, the `window_mode` adapter directly routes `WM_SETTEXT`/`WM_GETTEXT`, non-client calculation/painting/hit testing, and non-close system commands through `DefWindowProcA`.
- Windowed style now uses `WS_OVERLAPPEDWINDOW` for a standard resize frame and maximize button, with the Windows default application icon supplied when the original class has none.
- The runtime-probe WndProc intentionally consumes text/non-client processing; title, icon, `HTCAPTION`, style, 1280x960 client, and fullscreen restoration still pass.
- User revalidation showed that the first corrected build still had an empty title/icon from startup despite the maximize style and 1280x960 client. Because client resize is reached only after successful title assignment, the scope narrows to a later replacement path; the actual writer remains unresolved.
- Host title updates now use direct `DefWindowProcA(WM_SETTEXT)` instead of `SetWindowTextA`, bypassing the active WndProc chain. Ordinary `WM_SETTEXT` and `WM_SETICON` replacements entering the adapter report success without changing preserved host state.
- The runtime probe attempts empty-title and null-icon overwrites after host assignment and requires the full title and icon to remain.
- Release diagnostics showed a correctly stored title but exposed that the probe compared eight bytes against the seven-character `"re2DJ v"` prefix. The comparison now uses the actual literal length; five consecutive Release runtime-probe runs and the final complete Debug/Release CTest pass.
- Repeated user revalidation invalidated the earlier conclusion that preserving host state alone would restore the caption. Actual product diagnostic run `20260829-125238-338` confirmed valid stored-title length and prefix, icon, and style; SDL's empty `WM_SETTEXT` was blocked, and the WndProc changed after SDL external-window wrapping.
- Explicit drawing into the non-client DC was overwritten by DWM. Applying `DWMNCRP_DISABLED` during the same run immediately exposed the Windows-default title and icon and overlapped the temporary text, so custom caption drawing was removed from the final implementation.
- The final implementation applies `DWMNCRP_DISABLED` after `SetWindowPos(...SWP_FRAMECHANGED)` in windowed mode, restores `DWMNCRP_ENABLED` in fullscreen, and redraws the frame. It adds no dependency beyond the Windows system `dwmapi` library.
- Final product run `20260829-161622-270` and the DPI-corrected capture `build/window-caption-final.png` confirmed one non-duplicated icon/status title together with `FPS : 60.0` and the 1280x960 client area. Closing the diagnostic window through `WM_CLOSE` cleanly exited both the loader and original process. The capture remains an ignored build artifact and is not included in the repository.

### Verification

- `cmake --build build\windows-x86 --config Debug`: passed
- `ctest --test-dir build\windows-x86 -C Debug --output-on-failure`: 3/3 passed
- `cmake --build build\windows-x86 --config Release`: passed
- `ctest --test-dir build\windows-x86 -C Release --output-on-failure`: 3/3 passed

### Remaining validation

Direct product validation passed. Only final user confirmation of the title, icon, and caption size in the same environment remains.
