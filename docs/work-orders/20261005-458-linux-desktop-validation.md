# 작업 458 작업 지시서 — v0.0.62~v0.0.64의 Linux 실기 검증 / Task 458 work order — validating v0.0.62 to v0.0.64 on a Linux desktop

선행: [작업 444 로그(게임패드)](../work-logs/20261003-444-gamepad-input.md), [작업 455 로그(후처리 셰이더)](../work-logs/20261005-455-post-process-shaders.md), [작업 457 로그(셰이더 명령행)](../work-logs/20261005-457-post-shader-command-line.md)

## 배경 / Background

최근 변경(작업 444 게임패드, 작업 454 clang 빌드, 작업 455·457 후처리 셰이더와 명령행)은 Linux 쪽을 WSL(WSLg, llvmpipe)에서만 확인했다. 작업 444는 실물 패드로 Linux에서 입력하는 확인을 남겨 두었다. 사용자가 데스크톱 Linux 실기에서 검증을 요청했다. 코드는 바꾸지 않는다.

*The recent changes (task 444's gamepad, task 454's clang build, tasks 455 and 457's post shaders and command line) were checked on Linux only under WSL (WSLg, llvmpipe), and task 444 left input from a pad on Linux unchecked. The user asked for validation on a desktop Linux machine. No code changes.*

## 절차 / Steps

1. `linux-x64-debug`(경고를 오류로)와 `linux-x64-release`를 빌드하고 CTest와 GL probe 3종을 돌린다.
   *Build `linux-x64-debug` (warnings as errors) and `linux-x64-release`, and run CTest and the three GL probes.*
2. 4th를 셰이더별(`none`·`crt`·`scanline`)로 실행하고, 작업 457의 명령행 경우를 다시 돌린다.
   *Run 4th under each shader and rerun task 457's command-line cases.*
3. 6th를 `--post-shader=crt -- ez2dj6th`로 실행해 런처·자식 전달과 종료를 본다.
   *Run 6th with `--post-shader=crt -- ez2dj6th` and check what reaches the child and how both end.*
4. 실물 패드가 없으므로 uinput 가상 Xbox 360 패드로 핫플러그, 버튼·축 매핑, 게임 입력(코인)을 확인한다. 키보드도 uinput으로 넣어 대조한다.
   *With no physical pad, use a uinput virtual Xbox 360 pad for hot-plug, button and axis mapping and in-game input (the coin), compared against a uinput keyboard.*
5. `package_release.sh linux-x64`의 glibc 상한과 NEEDED를 확인한다.
   *Check `package_release.sh linux-x64`'s glibc limit and NEEDED.*

## 완료 조건 / Done when

각 항목의 결과와, 할 수 없었던 항목의 이유가 작업 로그에 남는다.

*The work log records each item's result and why any item could not be done.*
