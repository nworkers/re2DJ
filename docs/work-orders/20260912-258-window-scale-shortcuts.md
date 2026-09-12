# 작업 지시서: 창 크기 단축키와 더블클릭 전체화면 전환

## 관련 설계

[창 크기 단축키와 더블클릭 전체화면 전환 설계](../design/20260912-258-window-scale-shortcuts.md)

## 작업 범위

1. 현재 Windows host shell과 window mode 구현을 기준으로 입력 전달 경계를 확인한다.
2. 현재 고정 2배 windowed scale을 1/2/3 선택 정책으로 확장한다.
3. `Alt+1`, `Alt+2`, `Alt+3` 및 숫자 키패드 대응을 추가한다.
4. guest HWND subclass에서 mouse double-click을 감지하고 fullscreen을 토글한다.
5. 기존 WndProc 복원, 일반 메시지 위임, scale 상태 복원을 구현한다.
6. runtime probe 검사를 추가하거나 갱신해 세 창 크기와 fullscreen 왕복을 검증한다.
7. 설계·architecture·작업 로그를 갱신하고 build/test 후 커밋한다.

## 제외 범위

- 원본 guest logical render target 크기 변경
- host desktop display mode 변경
- Linux/Web SDL window 조작 정책 변경
- 원본 실행 파일 수정
- 자유로운 임의 해상도 입력 UI

## 완료 조건

- 기본 windowed client가 1280×960으로 유지된다.
- Alt+1/2/3이 각각 640×480, 1280×960, 1920×1440 client area를 적용한다.
- 마우스 더블클릭으로 fullscreen 진입·해제가 된다.
- fullscreen 해제 후 마지막 windowed scale이 복원된다.
- 기존 guest WndProc와 일반 메시지 동작이 보존된다.
- 기존 build/test와 runtime window policy 검증이 통과한다.

## English

### Related design

[Window scale shortcuts and double-click fullscreen design](../design/20260912-258-window-scale-shortcuts.md)

### Scope

1. Confirm the input-delivery boundary in the current Windows host shell and window-mode implementation.
2. Extend the fixed two-times windowed scale into selectable scales 1, 2, and 3.
3. Add `Alt+1`, `Alt+2`, `Alt+3`, and numeric keypad equivalents.
4. Detect mouse double-clicks in the guest HWND subclass and toggle fullscreen.
5. Restore the original WndProc, delegate ordinary messages, and preserve the selected scale across fullscreen.
6. Add or update runtime-probe checks for the three client sizes and fullscreen round trips.
7. Update the design, architecture, and work log, then build, test, and commit.

### Out of scope

- Changing the original guest logical render-target size
- Changing the host desktop display mode
- Changing Linux/Web SDL window policy
- Modifying the original executable
- Adding a free-form arbitrary-resolution UI

### Completion criteria

- The default windowed client remains 1280×960.
- Alt+1/2/3 apply 640×480, 1280×960, and 1920×1440 client areas respectively.
- Mouse double-click enters and leaves fullscreen.
- The last selected windowed scale returns after leaving fullscreen.
- The original guest WndProc and ordinary message behavior remain intact.
- Existing build/tests and runtime window-policy checks pass.
