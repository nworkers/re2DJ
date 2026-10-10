# src/launcher

인자 없이 실행했을 때 뜨는 런처(#12)의 플랫폼 공용 부분입니다. 창이나 OS API를 쓰지 않고 `std::filesystem`만 씁니다.

*The platform-neutral part of the launcher shown when re2dj runs without arguments (#12). It uses `std::filesystem` only, no window or OS API.*

* `launcher_catalog.cpp` — 내장 프로필마다 현재 디렉터리 기준 기본 경로(CHD 디렉터리 또는 덤프 디렉터리)로 가용 여부와 사유를 판정합니다. CHD 판정은 명령줄 shortcut과 같은 `hdd::LocateChdImage`입니다.
* `launcher_settings.cpp` — `cfg/re2dj.ini` 읽기·쓰기와, 고른 설정을 자식 실행의 명령줄 옵션으로 바꾸는 일을 합니다.

  *`launcher_catalog.cpp` judges each built-in profile's availability and reason from its default path (CHD directory or dump directory) under the current directory, the CHD rule being the command-line shortcut's `hdd::LocateChdImage`. `launcher_settings.cpp` reads and writes `cfg/re2dj.ini` and turns chosen settings into the child run's command-line options.*

화면은 `src/ui/launcher_screen.cpp`(ImGui), 창은 `src/platform/sdl/launcher_window.cpp`(SDL3), 흐름은 `src/host/cli/launcher_session.cpp`, 자식 실행은 `include/re2dj/platform/self_process.h`의 OS별 구현에 있습니다. 근거: [설계](../../docs/design/20261010-i012-launcher.md).

*The screen is in `src/ui/launcher_screen.cpp` (ImGui), the window in `src/platform/sdl/launcher_window.cpp` (SDL3), the flow in `src/host/cli/launcher_session.cpp`, and running the child in the per-OS implementations of `include/re2dj/platform/self_process.h`. See the [design](../../docs/design/20261010-i012-launcher.md).*
