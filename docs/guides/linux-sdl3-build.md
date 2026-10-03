# Linux SDL3/OpenGL 빌드 가이드

이 가이드는 Ubuntu 24.04 또는 WSL Ubuntu 24.04에서 re2DJ의 SDL3 X11·Wayland·OpenGL backend를 구성하고 검증하는 반복 절차다. 근거는 [SDL3/OpenGL 공용 backend 설계](../design/20260827-076-sdl3-opengl-shared-backend.md)와 [작업 로그](../work-logs/20260827-076-sdl3-opengl-shared-backend.md)에 둔다.

## 개발 패키지

```bash
sudo apt update
sudo apt install -y \
  ninja-build \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev \
  libxi-dev libxss-dev libxtst-dev \
  libwayland-dev libwayland-egl-backend-dev libxkbcommon-dev \
  libegl1-mesa-dev libgl1-mesa-dev libgles2-mesa-dev \
  libdrm-dev libgbm-dev \
  libasound2-dev libpulse-dev libpipewire-0.3-dev libudev-dev
```

소리는 SDL3_mixer가 ALSA·PulseAudio·PipeWire 중 하나로 내므로 그 개발 패키지가 필요하다. `libudev-dev`는 게임패드 핫플러그 열거용이다(작업 444). 헤더가 없으면 SDL은 udev 없이 빌드되고 `/dev/input`을 inotify로만 본다. 두 라이브러리 모두 실행 시 `dlopen`으로 열리므로 릴리스 실행 파일의 NEEDED에는 들어가지 않는다.

GNOME/Weston에서 client-side window decoration까지 활성화하려면 선택적으로 `libdecor-0-dev`를 추가한다. 이 패키지가 없어도 SDL3의 X11·Wayland·OpenGL backend는 빌드된다.

## 빌드와 테스트

```bash
cmake --preset linux-x64-debug -DRE2DJ_WARNINGS_AS_ERRORS=ON
cmake --build --preset linux-x64-debug
ctest --preset linux-x64-debug --output-on-failure
```

Linux x86 product host는 multilib compiler와 32비트 SDL/X11/Wayland/OpenGL 개발 패키지가 필요하다. WSL Ubuntu에서 x64 패키지만 설치된 경우 동일한 `:i386` 개발 패키지를 추가하고 `g++-multilib libc6-dev-i386`도 준비한다. 현재 preset은 i386 `libxss`와 `libxtst`가 없는 WSL에서도 빌드할 수 있도록 SDL XScreenSaver·XTest 통합을 끈다. 해당 기능이 필요하면 `libxss-dev:i386 libxtst-dev:i386`을 설치하고 `-DSDL_X11_XSCRNSAVER=ON -DSDL_X11_XTEST=ON`으로 별도 구성한다.

```bash
cmake --preset linux-x86-debug -DRE2DJ_WARNINGS_AS_ERRORS=ON
cmake --build --preset linux-x86-debug
ctest --preset linux-x86-debug --output-on-failure
file build/linux-x86-debug/bin/re2dj build/linux-x64-debug/bin/re2dj
```

`file` 결과에서 x86 product는 ELF 32-bit Intel 80386, x64 product는 ELF 64-bit x86-64여야 한다. 두 product 모두 원본 PE32를 같은 프로세스 안에서 실행한다(작업 379에서 별도 i386 helper를 제거했다).

WSL에서 Windows filesystem 아래 build가 느리면 source는 그대로 두고 binary directory만 Linux filesystem의 임시 디렉터리로 지정할 수 있다. 이 경로는 일회성 build 산출물이며 저장소에 넣지 않는다.

---

# Linux SDL3/OpenGL Build Guide

This is the repeatable procedure for configuring and verifying the re2DJ SDL3 X11, Wayland, and OpenGL backend on Ubuntu 24.04 or WSL Ubuntu 24.04. It is based on the [shared SDL3/OpenGL backend design](../design/20260827-076-sdl3-opengl-shared-backend.md) and [work log](../work-logs/20260827-076-sdl3-opengl-shared-backend.md).

Install the packages shown above, then run the configure, build, and CTest commands. The audio development packages serve SDL3_mixer, which plays through ALSA, PulseAudio or PipeWire, and `libudev-dev` serves gamepad hot-plug enumeration (task 444); without its header SDL builds without udev and watches `/dev/input` through inotify alone. Both libraries are `dlopen`ed at run time, so neither enters the release executable's NEEDED. Optionally install `libdecor-0-dev` for client-side window decorations on GNOME/Weston; X11, Wayland, and OpenGL still build without it. Under WSL, an out-of-tree binary directory on the Linux filesystem can avoid slow Windows-filesystem build I/O. Keep that temporary output outside the repository.

For the Linux x86 product host, install multilib support and 32-bit SDL/X11/Wayland/OpenGL development packages alongside `g++-multilib libc6-dev-i386`. The preset disables SDL XScreenSaver and XTest integration when i386 `libxss`/`libxtst` are unavailable; install `libxss-dev:i386 libxtst-dev:i386` and configure with `-DSDL_X11_XSCRNSAVER=ON -DSDL_X11_XTEST=ON` when those integrations are required. Build the product host and PE32 helper in separate trees. The x86 product and helper should be reported as ELF 32-bit Intel 80386 by `file`, while the x64 product is ELF 64-bit x86-64. Run the host probe from each product architecture against the same i386 helper.
