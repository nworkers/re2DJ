# 작업 458 작업 로그 — v0.0.62~v0.0.64의 Linux 실기 검증 / Task 458 work log — validating v0.0.62 to v0.0.64 on a Linux desktop

지시서: [20261005-458-linux-desktop-validation.md](../work-orders/20261005-458-linux-desktop-validation.md)

## 2026-10-05

- **환경**: Ubuntu 26.04.1(커널 7.0.0), GNOME Wayland, 모니터 2대(5120x2880), NVIDIA RTX 4090(드라이버 595.91.07, OpenGL 4.6), GCC 15.2, CMake 4.2.3, glibc 2.43. 실물 게임패드 없음. 기준 커밋 `6eb4bc1`(v0.0.64).

  *Environment: Ubuntu 26.04.1 (kernel 7.0.0), GNOME Wayland, two 5120x2880 monitors, NVIDIA RTX 4090 (driver 595.91.07, OpenGL 4.6), GCC 15.2, CMake 4.2.3, glibc 2.43, no physical gamepad; base commit `6eb4bc1` (v0.0.64).*

### 빌드와 테스트 / Build and tests

- `linux-x64-debug` + `RE2DJ_WARNINGS_AS_ERRORS=ON`: 전체 빌드 성공(경고 없음), CTest 5개 통과. `linux-x64-release`도 경고 없이 빌드됐다. 작업 444에서 GCC 13 `-O3`가 오탐을 냈던 것과 달리 GCC 15.2 Release는 경고가 없었다(Release는 경고를 오류로 두지 않고 빌드했다).
- GL probe(Debug): `blend` 10/10, `post_shader` 12/12, `true_color` 13/13. 작업 455의 Windows(RTX 4090)·WSLg(llvmpipe)에 이어 Linux NVIDIA 드라이버에서도 셰이더 경로가 통과한다.
- **하지 못한 것**: clang 빌드와 Linux x86 빌드. 이 머신에는 clang과 i386 multilib 패키지(`g++-multilib`, `libc6-dev-i386`)가 없다. clang은 작업 454에서 WSL clang 18로, x86은 CI에서 확인한다.

  *`linux-x64-debug` with `RE2DJ_WARNINGS_AS_ERRORS=ON` builds in full with no warnings and passes 5 CTest tests; `linux-x64-release` also builds with no warnings (unlike task 444's GCC 13 `-O3` false positive, GCC 15.2 gives none; Release was built without warnings as errors). The Debug GL probes pass `blend` 10/10, `post_shader` 12/12 and `true_color` 13/13, so the shader path passes on the Linux NVIDIA driver after task 455's Windows (RTX 4090) and WSLg (llvmpipe). Not done: the clang and Linux x86 builds, as this machine has neither clang nor the i386 multilib packages; clang was covered by task 454 with WSL clang 18 and x86 is covered in CI.*

### 후처리 셰이더와 명령행 (작업 455·457) / Post shaders and the command line (tasks 455, 457)

Release, 네이티브 Wayland(SDL 기본 드라이버):

| 실행 / run | 결과 / result |
| --- | --- |
| `ez2dj4th --post-shader crt` | `presentation: post shader crt (6 parameters)` |
| `RE2DJ_POST_SHADER=scanline ez2dj4th` | `scanline (RE2DJ_POST_SHADER)`, 2 parameters |
| `--post-shader=scanline ez2dj4th` | scanline |
| `RE2DJ_POST_SHADER=none ez2dj4th --post-shader crt` | crt(명령행 우선 / the command line wins) |
| `--post-shader=crt -- ez2dj4th` | crt |
| `ez2dj4th --post-shader` | `--post-shader requires a value`, exit 1 |
| `ez2dj4th --post-shader=` | `--post-shader needs a shader id, or none`, exit 1 |
| `ez2dj4th --post-shader crt --post-shader=scanline` | scanline(마지막 값 / the last value) |
| `ez2dj4th --post-shader nope` | 경고 `no shader named 'nope' in shaders or built in`, 실행 계속 |
| `--post-shader=crt -- ez2dj6th` | 런처와 `EZ2DJ6TH.EXE` 자식 모두 crt, 자식 창에 적용 |

- 6th 자식의 실제 명령행(`/proc/<pid>/cmdline`): `re2dj --post-shader=crt ez2dj6th --guest-executable EZ2DJ/EZ2DJ6TH.EXE --guest-command-line .\EZ2DJ6TH.EXE --guest-current-directory D:\ez2dj --guest-exit-code-fd …`. `--`가 빠지고 옵션이 대상 앞에 온다. 자식은 런처의 자식 프로세스로 같은 실행 파일에서 시작한다(작업 443). 60초 뒤 SIGINT에 자식이 `host window closed`, 런처가 `ExitProcess(0x00000000)`로 끝나고 남은 프로세스가 없다.
- 프레임률(XWayland, 창 제목의 FPS를 2초마다 20초간): `none`·`crt`·`scanline` 모두 59.8~60.2(시작 직후 한 번 62~64).
- **하지 못한 것**: 셰이더 화면 캡처. GNOME Wayland에서 XWayland 창을 `ffmpeg x11grab`으로 잡으면 검은 화면만 나오고, 셸 스크린샷 D-Bus는 권한 대화 상자가 필요하다. 셰이더 출력 자체는 GL probe의 픽셀 검사로 대신한다. OSD 조작(마우스로 전환·Reload·슬라이더)도 사람이 확인할 항목으로 남는다.

  *Release on native Wayland (SDL's default driver) gives the results in the table above. The 6th child's real command line (`/proc/<pid>/cmdline`) drops `--` and puts the options before the target; the child starts from the same executable as the launcher's child process (task 443), and on SIGINT after 60 s the child ends with `host window closed` and the launcher with `ExitProcess(0x00000000)`, leaving no process behind. Frame rate under XWayland, sampling the window title's FPS every 2 s for 20 s: 59.8 to 60.2 for `none`, `crt` and `scanline` alike (62 to 64 once right after start). Not done: shader screenshots — `ffmpeg x11grab` of an XWayland window under GNOME Wayland captures only black, and the shell's screenshot D-Bus needs a permission dialog — so the GL probe's pixel checks stand in for the shader output; the OSD (switching, Reload and sliders with the mouse) remains for a person to check.*

### 게임패드 (작업 444) / Gamepad (task 444)

실물 패드 대신 `/dev/uinput`(사용자 ACL rw)으로 가상 Xbox 360 패드(`045e:028e`)와 가상 키보드를 만들었다. 스크립트는 scratchpad에 두었고 저장소에 넣지 않았다. 패드 상태가 호스트 입력까지 오는지 보려고 `SdlHostPresentation::Present`에 패드 비트 변화와 창 포커스 변화를 기록하는 **임시 로그**를 넣어 빌드했고, 확인 뒤 되돌려 다시 빌드했다(바이너리에 문자열이 없음을 확인).

*A virtual Xbox 360 pad (`045e:028e`) and a virtual keyboard were created through `/dev/uinput` (user ACL rw) in place of a physical pad; the scripts stayed in the scratchpad, not the repository. To see the pad state reach the host input, a **temporary log** of pad-bit and window-focus changes was built into `SdlHostPresentation::Present`, then reverted and rebuilt (the binary confirmed free of its strings).*

- **핫플러그(실물 udev 경로)**: 실행 중 연결하면 약 0.3초 안에 `input: gamepad added: Xbox 360 Controller (1 connected)`, 분리하면 `removed … (0 connected)`. 실행 전에 꽂혀 있으면 `gamepads ready, 1 connected` 뒤 `added`가 한 번 더 찍힌다(SDL이 기존 장치에도 ADDED 이벤트를 보내기 때문으로 보임, 연결 수는 1로 맞다).
- **버튼과 축 → 비트**(임시 로그): A=bit1, BACK=bit5, START=bit7. X축 최소/최대 → `LSTICK_LEFT`(bit22)/`LSTICK_RIGHT`(bit23), 해트 X −1 → `DPAD_LEFT`(bit14), 해트 Y +1 → `DPAD_DOWN`(bit13), LT 255 → `LT`(bit16), RX 20000 → `RSTICK_RIGHT`(bit27). X축 −8000(절반 미만)은 비트 없음. 설계 444의 표와 절반 기울임 규칙대로다.
- **게임 입력(6th)**: 어트랙트 중 BACK(코인)을 두 번 누르면 각각 1 ms 안에 같은 사운드 버퍼(`ef601598`)의 `IDirectSoundBuffer::Play`가 호출되고(코인 효과음), 입력 없는 대조 실행에서 +64초에 읽던 `StreetMix.gds`(어트랙트 순환)를 읽지 않는다. uinput 키보드의 F5도 같은 버퍼의 `Play`를 부른다. 패드 BACK → I/O 보드 → 게임 경로가 키보드와 같이 동작한다.
- **START**: 패드 START와 키보드 `1` 모두 코인 두 개 뒤 6th 타이틀에서 눈에 띄는 반응(소리·파일 읽기)이 없었다. 둘이 같으므로 패드 문제는 아니다. 4th에서도 키보드·패드 모두 타이틀을 넘기지 못했다. 크레딧 설정이나 입력 시점 때문인지는 **미확정**이며 이 작업의 범위 밖으로 둔다. 화면을 볼 수 없어 판단 근거가 API 로그뿐이다.
- 창 포커스: 실행 도중 검증 스크립트의 조작 없이 포커스가 빠졌다 돌아오는 기록이 여러 번 있었다(데스크톱을 함께 쓰는 중). SDL은 포커스가 없으면 패드 버튼을 버리므로(설계 445 3-1) 패드 대조 실행은 `SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1`로 포커스 영향을 없앴다. 포커스가 있을 때는 이 힌트 없이도 비트가 그대로 들어왔다.

  *Hot-plug through the real udev path: connecting during a run logs `input: gamepad added: Xbox 360 Controller (1 connected)` within about 0.3 s and disconnecting logs `removed … (0 connected)`; a pad attached before the start logs `gamepads ready, 1 connected` and then one more `added` (apparently SDL sending ADDED for existing devices; the count stays 1). Buttons and axes to bits (temporary log): A bit 1, BACK bit 5, START bit 7; X axis min/max to `LSTICK_LEFT` (22)/`LSTICK_RIGHT` (23), hat X −1 to `DPAD_LEFT` (14), hat Y +1 to `DPAD_DOWN` (13), LT 255 to `LT` (16), RX 20000 to `RSTICK_RIGHT` (27), and X at −8000 (under half) to none — design 444's table and half-deflection rule. In-game input (6th): each of two BACK presses during the attract calls `IDirectSoundBuffer::Play` on the same buffer (`ef601598`) within 1 ms (the coin sound), and the run no longer reads `StreetMix.gds`, which the no-input control run reads at +64 s in the attract cycle; F5 from the uinput keyboard plays the same buffer, so pad BACK → I/O board → game works as the keyboard does. START: neither the pad's START nor the keyboard's `1` drew a visible response (sound or file reads) on 6th's title after two coins, and on 4th neither got past the title; the two agree, so it is not a pad problem, and whether credit settings or timing are the cause is unresolved and outside this task — with no picture, the API log is the only evidence. Focus left and returned several times during runs without any action by the validation scripts (the desktop was in use); since SDL drops pad buttons without focus (design 445, 3-1), the pad comparison runs set `SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1`, while with focus the bits arrived without the hint.*

### 패키지 (작업 442·444) / Package (tasks 442, 444)

- `scripts/package_release.sh linux-x64`: 이 호스트의 빌드는 `GLIBC_2.43`을 요구해 기본 상한 2.36에서 거부된다(의도된 가드, 릴리스는 Debian 12 이미지에서 만든다). `RE2DJ_MAX_GLIBC=2.99`로는 `re2dj-v0.0.64-linux-x64.tar.gz`가 만들어지고 NEEDED는 `libm.so.6 libc.so.6 ld-linux-x86-64.so.2` 그대로다.

  *`scripts/package_release.sh linux-x64`: this host's build needs `GLIBC_2.43` and is refused at the default limit of 2.36 (the intended guard; releases are built in the Debian 12 image); with `RE2DJ_MAX_GLIBC=2.99` it writes `re2dj-v0.0.64-linux-x64.tar.gz`, NEEDED unchanged at `libm.so.6 libc.so.6 ld-linux-x86-64.so.2`.*

### 관찰 / Observations

- **가려진 창은 1 fps**: XWayland에서 창을 최소화(`XIconifyWindow`)하면 약 2초 뒤 창 제목이 `FPS : 1.0`이 된다. 게임 로직이 `Flip`에 묶여 있어 게임 자체가 초당 1프레임으로 느려진다(40초 동안 API 호출 24,058회, 정상 실행은 약 75만 회). Mutter가 보이지 않는 창의 프레임을 1 Hz로 줄이는 동작으로 **추정**한다. 앞서 셰이더 비교 중 `none` 한 번이 이렇게 느려졌는데, 셰이더와 무관하고 창이 가려졌던 탓으로 보인다. 리듬 게임에서는 음악과 어긋날 수 있으므로 처리 여부는 사용자와 정한다.
- **창 생성 83초 지연(미확정)**: 08:29~08:36 사이 네이티브 Wayland 실행 6번이 모두 오디오 초기화 뒤 창이 뜨기까지 약 83초씩 걸렸다(그 전후는 0.4~0.5초). 그 사이 메인 스레드는 `poll`에서 기다렸다. ptrace가 막혀(`ptrace_scope=1`) 백트레이스는 얻지 못했고, strace 아래와 이후 실행, 최소화 실험 재현에서는 다시 나타나지 않았다. 저널에 화면 잠금 같은 세션 사건은 없었다. 다시 보이면 `strace -f -tt`로 기동해 원인을 잡는다.
- 네이티브 Wayland 기동마다 `libdecor-gtk-WARNING: Failed to initialize GTK`가 stderr에 찍힌다. 창 장식과 실행에는 영향이 보이지 않았다.

  *A hidden window runs at 1 fps: minimising the window under XWayland (`XIconifyWindow`) turns the title to `FPS : 1.0` about 2 s later, and since the game logic is tied to `Flip` the game itself slows to one frame a second (24,058 API calls in 40 s against about 750,000 normally); inferred to be Mutter throttling invisible windows' frames to 1 Hz. One `none` run in the shader comparison slowed like this, unrelated to the shader and apparently because the window was covered; in a rhythm game this can drift from the music, so whether to handle it is for the user to decide. An 83-second window creation (unresolved): between 08:29 and 08:36 six native Wayland runs each took about 83 s from audio initialisation to the window (0.4 to 0.5 s before and after), the main thread waiting in `poll`; with ptrace blocked (`ptrace_scope=1`) no backtrace was taken, and it did not recur under strace, in later runs or when repeating the minimise experiment, with no session event such as a screen lock in the journal; if it returns, start under `strace -f -tt` to catch it. Every native Wayland start prints `libdecor-gtk-WARNING: Failed to initialize GTK` to stderr, with no visible effect on decorations or the run.*

### 추가 확인 / Follow-up

- **정정**: 위 "하지 못한 것"의 i386 multilib 부재는 잘못이었다. `dpkg` 출력을 걸러낼 때 패키지명 형식을 잘못 다뤘다. `gcc-multilib`·`g++-multilib`·`libc6-dev-i386`과 i386 아키텍처는 설치돼 있었다. 다시 빌드하니 GCC 15의 링크 오류와 i386 스택 정렬로 인한 실행 중 `SIGSEGV`가 나왔고, [작업 459](20261005-459-linux-x86-gcc15-host-abi.md)에서 고쳤다.
- 사용자가 `clang`(21.1.8)과 amd64 `libudev-dev`를 설치했다. x64 구성에서 `SDL_LIBUDEV`가 ON이 됐고, 실행 중 `libudev`가 dlopen으로 올라온 상태에서 가상 패드 `added`·`removed`를 확인했다. NEEDED는 변하지 않았다. clang 21 빌드는 spdlog 번들 fmt 10.2.1 때문에 실패해 [작업 460](20261005-460-spdlog-fmt-clang21.md)에서 고쳤다.

  *Correction: the missing i386 multilib above was wrong, caused by mishandling package names when filtering `dpkg` output; `gcc-multilib`, `g++-multilib`, `libc6-dev-i386` and the i386 architecture were installed. Building again hit GCC 15's link error and a run-time `SIGSEGV` from i386 stack alignment, fixed in [task 459](20261005-459-linux-x86-gcc15-host-abi.md). The user installed `clang` (21.1.8) and amd64 `libudev-dev`: the x64 configuration now has `SDL_LIBUDEV` ON, and a virtual pad's `added` and `removed` were seen with `libudev` dlopened during the run, NEEDED unchanged. The clang 21 build failed on spdlog's bundled fmt 10.2.1, fixed in [task 460](20261005-460-spdlog-fmt-clang21.md).*
