# Win32 caption과 DPI 실행 분석

## 한국어

- **확인됨:** 실제 1st SE 제품 실행에서 제목 문자열, icon과 style 상태는 정상인데 DWM caption content만 보이지 않았다. `DWMNCRP_DISABLED`는 내용을 표시했지만 frame layout을 바꿨다.
- **확인됨:** DWM 활성, WndProc 순서, `DwmDefWindowProc`, 정상 제목 메시지와 caption 색상만 바꿔서는 해결되지 않았다.
- **확인됨:** 실제 host/guest HWND는 SDL 초기화 뒤 per-monitor aware, DPI 144였다. 초기 outer `1296×999`는 1280×960 client에 96-DPI frame만 더한 크기였고 제목과 icon을 clip했다.
- **확인됨:** SDL 초기화 뒤 `AdjustWindowRectExForDpi`를 DPI 144로 다시 적용하면 outer `1302×1016`, host/guest client 1280×960이 되며 DWM icon과 제목 전체가 표시된다. 증거는 작업 092 실제 제품 캡처와 크기 조회다.
- **확인됨:** host close 전에 guest/SDL teardown을 호출하면 process termination 정체가 재현되며, guest를 유지한 current-process `TerminateProcess`는 실제 loader까지 종료한다.
- **미확정:** 다른 monitor DPI에서 시작하거나 실행 중 monitor를 옮기는 `WM_DPICHANGED` 대응은 아직 실제 제품으로 검증하지 않았다.

## English

- **Confirmed:** In the actual 1st SE product, title string, icon, and style state were valid while only DWM caption content was absent. `DWMNCRP_DISABLED` exposed the content but changed frame layout.
- **Confirmed:** DWM enablement, WndProc order, `DwmDefWindowProc`, normal title messages, and caption colors alone did not fix it.
- **Confirmed:** After SDL initialization, the actual host/guest HWNDs were per-monitor aware at DPI 144. The initial `1296x999` outer added only a 96-DPI frame to the 1280x960 client and clipped title/icon content.
- **Confirmed:** Reapplying `AdjustWindowRectExForDpi` at DPI 144 after SDL initialization produces a `1302x1016` outer, 1280x960 host/guest clients, and a complete DWM icon/title. Evidence is the Task 092 product capture and size query.
- **Confirmed:** Guest/SDL teardown before host close reproduces process-termination stalling, while current-process `TerminateProcess` with the guest intact exits the real loader as well.
- **Unresolved:** Starting on other monitor DPIs and handling `WM_DPICHANGED` while moving between monitors have not yet been verified in the actual product.
