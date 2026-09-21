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

Linux x86 product host는 multilib compiler와 32비트 SDL/X11/Wayland/OpenGL 개발 패키지가 필요하다. WSL Ubuntu에서 x64 패키지만 설치된 경우 동일한 `:i386` 개발 패키지를 추가하고 `g++-multilib libc6-dev-i386`도 준비한다. 현재 preset은 i386 `libxss`와 `libxtst`가 없는 WSL에서도 빌드할 수 있도록 SDL XScreenSaver·XTest 통합을 끈다. 해당 기능이 필요하면 `libxss-dev:i386 libxtst-dev:i386`을 설치하고 `-DSDL_X11_XSCRNSAVER=ON -DSDL_X11_XTEST=ON`으로 별도 구성한다. 제품 host와 PE32 helper는 별도 build tree를 사용한다.

```bash
cmake --preset linux-x86-debug -DRE2DJ_WARNINGS_AS_ERRORS=ON
cmake --build --preset linux-x86-debug
ctest --preset linux-x86-debug --output-on-failure
cmake --preset linux-x86-helper -DRE2DJ_WARNINGS_AS_ERRORS=ON
cmake --build --preset linux-x86-helper
file build/linux-x86-debug/bin/re2dj build/linux-x86-helper/bin/re2dj_linux_native_ipc_helper
```

`file` 결과에서 x86 product와 helper는 ELF 32-bit Intel 80386, x64 product는 ELF 64-bit x86-64여야 한다. SDL3/OpenGL host probe는 x86 product와 x64 product 각각 같은 i386 helper를 실행한다.

WSL에서 Windows filesystem 아래 build가 느리면 source는 그대로 두고 binary directory만 Linux filesystem의 임시 디렉터리로 지정할 수 있다. 이 경로는 일회성 build 산출물이며 저장소에 넣지 않는다.

---

# Linux SDL3/OpenGL Build Guide

This is the repeatable procedure for configuring and verifying the re2DJ SDL3 X11, Wayland, and OpenGL backend on Ubuntu 24.04 or WSL Ubuntu 24.04. It is based on the [shared SDL3/OpenGL backend design](../design/20260827-076-sdl3-opengl-shared-backend.md) and [work log](../work-logs/20260827-076-sdl3-opengl-shared-backend.md).

Install the packages shown above, then run the configure, build, and CTest commands. Add `libasound2-dev libpulse-dev` only when working on the Linux audio backend; the current Linux graphics build disables SDL audio. Optionally install `libdecor-0-dev` for client-side window decorations on GNOME/Weston; X11, Wayland, and OpenGL still build without it. Under WSL, an out-of-tree binary directory on the Linux filesystem can avoid slow Windows-filesystem build I/O. Keep that temporary output outside the repository.

For the Linux x86 product host, install multilib support and 32-bit SDL/X11/Wayland/OpenGL development packages alongside `g++-multilib libc6-dev-i386`. The preset disables SDL XScreenSaver and XTest integration when i386 `libxss`/`libxtst` are unavailable; install `libxss-dev:i386 libxtst-dev:i386` and configure with `-DSDL_X11_XSCRNSAVER=ON -DSDL_X11_XTEST=ON` when those integrations are required. Build the product host and PE32 helper in separate trees. The x86 product and helper should be reported as ELF 32-bit Intel 80386 by `file`, while the x64 product is ELF 64-bit x86-64. Run the host probe from each product architecture against the same i386 helper.
