# 작업 로그 195: RGB565 논리 렌더 대상

# Work Log 195: RGB565 Logical Render Target

관련 설계: [RGB565 논리 렌더 대상 설계](../design/20260905-195-rgb565-logical-render-target.md)  
관련 작업 지시: [작업 지시 195](../work-orders/20260905-195-rgb565-logical-render-target.md)

## 결과 / Result

**구현 및 정적 검증 완료, 사용자 화면 비교 대기.** SDL3/OpenGL backend는 guest draw를 host window default framebuffer에 직접 그리지 않고 640×480 RGB565 color attachment와 depth16 attachment에 그린 뒤 `Present`에서 nearest copy합니다. 주소 모드 전달 뒤에도 남은 Music Select 밝기·경계 차이를 검증하기 위한 변경입니다.

*Implementation and static verification are complete; user-visible comparison is pending. The SDL3/OpenGL backend no longer draws guest content directly to the host-window default framebuffer. It renders to a 640×480 RGB565 color attachment and depth16 attachment, then uses a nearest copy at `Present`. This change tests the remaining Music Select brightness/edge difference after address-mode forwarding.*

## 구현 내용 / Implementation

- framebuffer, RGB565 color texture, depth16 renderbuffer를 생성하고 완전성을 검사했습니다.
- guest `Draw`와 frame clear를 논리 framebuffer에 고정했습니다.
- `Present`는 current host pixel viewport에서 blend/depth를 끈 full-screen copy만 수행합니다.
- framebuffer object core 이름을 먼저 해석하고, OpenGL 2.1 driver를 위해 `EXT` 이름을 fallback으로 사용합니다.
- framebuffer 지원 또는 attachment 완전성이 없으면 direct host-resolution draw로 되돌아가지 않고 초기화를 명시적으로 실패합니다.

*The implementation creates a framebuffer, RGB565 color texture, and depth16 renderbuffer and validates completeness. Guest draws and clears stay on the logical framebuffer. `Present` only makes a blend/depth-disabled full-screen copy into the current host pixel viewport. It resolves core framebuffer-object names first and falls back to `EXT` names for OpenGL 2.1 drivers. Missing support or incomplete attachments fail initialization explicitly rather than falling back to direct host-resolution drawing.*

사용자 첫 실행에서 논리 target은 정상적으로 표시됐지만 화면 전체가 상하 반전되었습니다. 이는 framebuffer texture의 lower-left V 원점을 guest screen의 top-left 원점으로 복사할 때 V를 뒤집지 않은 presentation pass 결함입니다. presentation quad의 V 좌표를 반전해 guest 방향을 유지하도록 수정했습니다.

*The user's first run displayed the logical target but flipped the entire image vertically. This was a presentation-pass defect: the framebuffer texture's lower-left V origin was copied without converting to the guest screen's top-left origin. The presentation quad now reverses V to preserve guest orientation.*

## 검증 / Verification

- `cmd /c scripts\build_win32.bat`: 통과.
- `build\windows-x86\bin\Debug\re2dj_unit_tests.exe`: `checks: 1265, failures: 0`.
- `ctest --test-dir build/windows-x86 -C Debug -R re2dj_unit_tests --output-on-failure`: 1/1 통과.
- `git diff --check`: 통과.

*`cmd /c scripts\build_win32.bat` passed. The unit executable reported `checks: 1265, failures: 0`; CTest passed 1/1; and `git diff --check` passed.*

## 남은 사용자 검증 / Pending User Verification

새 Debug build에서 기존과 같은 방법으로 Music Select에 진입해 중앙 artwork와 selection ring을 비교합니다. 시작하지 않거나 `OpenGL RGB565 render-target framebuffer is incomplete` 오류가 기록되면, 새 `.ddraw.log`와 `.jsonl` 경로를 제공해야 합니다. 정상 실행되면 화면 캡처와 로그 경로를 함께 비교합니다.

*Enter Music Select by the previous method in the new Debug build and compare the center artwork and selection ring. If startup fails or records `OpenGL RGB565 render-target framebuffer is incomplete`, provide the new `.ddraw.log` and `.jsonl` paths. Otherwise compare a screen capture together with the log path.*
