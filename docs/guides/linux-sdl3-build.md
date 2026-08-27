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
  libdrm-dev libgbm-dev
```

Linux audio backend까지 활성화하는 작업에서는 `libasound2-dev libpulse-dev`를 추가한다. 현재 공용 graphics build는 Linux에서 SDL audio를 끄므로 필수는 아니다.

GNOME/Weston에서 client-side window decoration까지 활성화하려면 선택적으로 `libdecor-0-dev`를 추가한다. 이 패키지가 없어도 SDL3의 X11·Wayland·OpenGL backend는 빌드된다.

## 빌드와 테스트

```bash
cmake --preset linux-x64-debug -DRE2DJ_WARNINGS_AS_ERRORS=ON
cmake --build --preset linux-x64-debug
ctest --preset linux-x64-debug --output-on-failure
```

WSL에서 Windows filesystem 아래 build가 느리면 source는 그대로 두고 binary directory만 Linux filesystem의 임시 디렉터리로 지정할 수 있다. 이 경로는 일회성 build 산출물이며 저장소에 넣지 않는다.

---

# Linux SDL3/OpenGL Build Guide

This is the repeatable procedure for configuring and verifying the re2DJ SDL3 X11, Wayland, and OpenGL backend on Ubuntu 24.04 or WSL Ubuntu 24.04. It is based on the [shared SDL3/OpenGL backend design](../design/20260827-076-sdl3-opengl-shared-backend.md) and [work log](../work-logs/20260827-076-sdl3-opengl-shared-backend.md).

Install the packages shown above, then run the configure, build, and CTest commands. Add `libasound2-dev libpulse-dev` only when working on the Linux audio backend; the current Linux graphics build disables SDL audio. Optionally install `libdecor-0-dev` for client-side window decorations on GNOME/Weston; X11, Wayland, and OpenGL still build without it. Under WSL, an out-of-tree binary directory on the Linux filesystem can avoid slow Windows-filesystem build I/O. Keep that temporary output outside the repository.
