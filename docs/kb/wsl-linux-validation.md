# WSL Linux 검증 범위 / WSL Linux Validation Scope

WSL 2는 실제 Linux kernel을 사용하는 개발 환경이다. re2DJ의 WSL 사용은 개발·검증 환경 선택이며, 원본 게임을 Wine이나 전체 시스템 에뮬레이터에 맡기는 제품 구조를 의미하지 않는다. Microsoft는 Linux 도구의 파일 작업에 같은 Linux filesystem 사용을 권장한다. [WSL 버전 비교](https://learn.microsoft.com/en-us/windows/wsl/compare-versions).

*WSL 2 provides an actual Linux kernel. Using it to develop and validate re2DJ does not make Wine or a full-system emulator part of the product execution architecture. Microsoft recommends keeping Linux tool workloads on the Linux filesystem. [WSL version comparison](https://learn.microsoft.com/en-us/windows/wsl/compare-versions).*

WSLg는 X11·Wayland GUI와 GPU driver를 통한 OpenGL 실행 경로를 제공하지만, 로컬 driver·라이브러리·아키텍처 조합의 성공을 보장하는 것은 아니다. headless unit test, 실제 window/OpenGL, 오디오 출력, 입력·focus를 별도 검증한다. [WSL GUI 공식 문서](https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps).

*WSLg provides X11/Wayland GUI and an OpenGL path through GPU drivers; this does not prove a particular local driver/library/architecture combination works. Verify headless tests, actual window/OpenGL, audio output, and input/focus separately. [Official WSL GUI documentation](https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps).*

이 프로젝트의 검증 정책은 kernel architecture와 ELF userspace architecture를 구분한다. `uname -m`만으로 실행 파일 비트 폭을 판정하지 않고 `file`/`readelf`와 실제 실행 결과를 기록한다. x64 kernel에서 ELF32 프로그램이 성공해도 32비트 kernel의 주소 공간·배포판·driver 동작까지 검증한 것으로 간주하지 않는다.

*Project validation policy distinguishes kernel and ELF userspace architectures. Record file/readelf output and execution results rather than deriving executable bitness from uname alone. ELF32 success on an x64 kernel does not validate a 32-bit kernel's address-space, distribution, or driver behavior.*

적용 계획: [작업 307](../work-orders/20260918-307-linux-x86-x64-wsl.md).

*Application plan: [task 307](../work-orders/20260918-307-linux-x86-x64-wsl.md).*
