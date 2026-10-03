# 작업 445 작업 지시서 — Windows host 게임패드 입력 / Task 445 work order — gamepad input on the Windows host

설계: [20261004-445-windows-gamepad-input.md](../design/20261004-445-windows-gamepad-input.md)

## 절차 / Steps

1. `Sdl3GamepadReader`에 `Update()`와 `SetDeviceObserver`를 더하고, `re2dj_sdl3_gamepad_test`에 펌프 없는 경로 검사를 더한다.
   *Add `Update()` and `SetDeviceObserver` to `Sdl3GamepadReader`, and a pump-less check to `re2dj_sdl3_gamepad_test`.*
2. `Ez2DjKeyboardInput`·`Ez2DancerKeyboardInput`을 공용 로더 위에 다시 쓰고, `Poll`이 게임패드 상태를 받게 하며, `ReadKeyboardKeyBinding`을 지운다.
   *Rewrite `Ez2DjKeyboardInput` and `Ez2DancerKeyboardInput` on the shared loader, let `Poll` take the gamepad state, and remove `ReadKeyboardKeyBinding`.*
3. 주입 런타임: reader 초기화·tick당 갱신·`Poll` 전달·로그. CMake에 세 소스 추가.
   *The injected runtime: reader initialisation, a refresh per tick, the controls into `Poll`, and the log lines; the three sources in CMake.*
4. 두 키보드 입력 테스트에 게임패드 검사를 더한다.
   *Gamepad checks in both keyboard input tests.*
5. 문서: README·ARCHITECTURE·Windows 런타임 가이드의 "Windows는 키보드만" 문구 갱신, 작업 444 설계의 4절에 후속 표시, 작업 로그.
   *Documents: the "Windows keyboard only" wording in README, ARCHITECTURE and the Windows runtime guide, a follow-up note in task 444's section 4, and the work log.*
6. 검증: Windows x86 build와 세 테스트, Xbox 패드를 꽂은 `re2dj.exe ez2dj6th` 실행 로그, Linux x64 build와 CTest.
   *Verification: the Windows x86 build and the three tests, a `re2dj.exe ez2dj6th` run log with the Xbox pad attached, and the Linux x64 build and CTest.*

## 완료 조건 / Done when

두 host의 build·테스트가 통과하고, Windows 실행 로그가 원본 프로세스 안에서 패드를 열었음을 보이며, 버튼 입력 확인은 사용자 몫으로 작업 로그에 적혀 있다.

*Both hosts' builds and tests pass, the Windows run log shows the pad opened inside the original process, and the button check is recorded in the work log as the user's.*
