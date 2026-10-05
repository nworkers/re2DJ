# 기술 지식 기반 색인 / Knowledge Base Index

이 디렉터리는 **일반적으로 통용되는** 기술 배경 지식을 주제별로 둔다. 원본 EZ2DJ 바이너리에서 확인한 프로젝트 고유 사실은 [`docs/analysis/`](../analysis/README.md)에 둔다.

*This directory holds **generally applicable** technical background, organized by topic. Project-specific facts confirmed against the original EZ2DJ binaries live in [`docs/analysis/`](../analysis/README.md).*

외부 자료에서 얻은 내용에는 원 사양, Microsoft 공식 문서, CPU 제조사 문서 같은 권위 있는 출처 링크를 가까운 위치에 남긴다.

*Place authoritative links — original specifications, official Microsoft documentation, CPU-vendor manuals — near anything derived from an external source.*

## 문서 / Documents

| 문서 | 내용 |
| --- | --- |
| [pe32-executable-format.md](pe32-executable-format.md) | PE32 실행 형식: 헤더 배치, 섹션, 재배치, import |
| [mame-chd-hunk-decompression.md](mame-chd-hunk-decompression.md) | MAME CHD의 hunk/unit 판독 단위, libchdr에 hunk 캐시가 없다는 점과 그로 인한 판독 비용 |
| [win32-hle-boundary.md](win32-hle-boundary.md) | Win32 API를 HLE 경계로 삼는 방식과 호출 규약 |
| [x86-32-guest-on-64-bit-host.md](x86-32-guest-on-64-bit-host.md) | 32비트 게스트를 64비트·WebAssembly 호스트에서 실행하는 선택지 |
| [i386-host-abi-at-guest-boundary.md](i386-host-abi-at-guest-boundary.md) | Linux i386에서 게스트 경계의 16바이트 스택 정렬, 정렬 어긋남이 SSE `movdqa`의 `SIGSEGV`로 나타나는 모습, 이름 없는 namespace 안 `extern "C"`의 링크 |
| [linux-x86-64-compatibility-mode.md](linux-x86-64-compatibility-mode.md) | Linux x86-64 프로세스 안의 `0x23`/`0x33` far transition, LDT FS와 host FS base 복원, signal 경로 |
| [libretro-glsl-post-shaders.md](libretro-glsl-post-shaders.md) | libretro 단일 pass GLSL 후처리 셰이더 형식, 호스트가 주는 값, `#pragma parameter`, 정수 배율의 주사선 함정, GLSL 1.20 기준 |
| [windows-x64-compatibility-mode.md](windows-x64-compatibility-mode.md) | Windows x64 프로세스 안의 `0x23` 실행, 문맥 전환 때 사라지는 `wrfsbase` TEB와 VEH 지연 복구, CS `0x33`으로 바뀌는 재개와 64비트 stub |
| [web-x86-execution-engines.md](web-x86-execution-engines.md) | Web용 x86 실행 엔진 후보, 라이선스와 제한된 검증 결정 |
| [windows-wow64-process-introspection.md](windows-wow64-process-introspection.md) | suspended WOW64 process의 주 이미지 주소를 검증하는 제한된 방법 |
| [hasp4-parallel-dongle.md](hasp4-parallel-dongle.md) | HASP4 병렬포트 API 형태, Hardlock 구분, Win32 IOCTL 반환 계약 |
| [hardlock-api-functions.md](hardlock-api-functions.md) | Hardlock `HL_API` function 코드와 `API_CRYPT`·`API_CODE` transform 모양 (GPL 소스 사실 대조) |
| [legacy-direct3d-immediate-mode.md](legacy-direct3d-immediate-mode.md) | DirectDraw에서 얻는 구형 Direct3D COM interface, hardware device 검색, proxy HLE 경계 |
| [legacy-directdraw-surface-gdi.md](legacy-directdraw-surface-gdi.md) | DirectDraw surface 생성, GDI GetDC/ReleaseDC interop와 HLE pixel-storage 계약 |
| [legacy-directsound-buffer.md](legacy-directsound-buffer.md) | DirectSound secondary buffer 생성, Lock/Unlock sample upload와 HRESULT 경계 |
| [sdl3-mixer-raw-audio.md](sdl3-mixer-raw-audio.md) | SDL3_mixer raw PCM, track 재생 상태와 zlib 라이선스 경계 |
| [x86-io-port-trapping.md](x86-io-port-trapping.md) | x86 `IN`/`OUT` 권한, Windows exception debug event, 제한된 장치 HLE trap |
| [windows-vectored-io-trap.md](windows-vectored-io-trap.md) | Windows vectored exception 처리 순서와 debugger 분리 실행 경계 |
| [win32-file-deletion-and-case.md](win32-file-deletion-and-case.md) | `DeleteFileA`의 오류 계약, Win32 이름 대소문자 비교, 읽기 전용 이미지 위의 whiteout |
| [win32-dpi-window-frame.md](win32-dpi-window-frame.md) | DPI-aware Win32 client/outer frame 계산과 초기화 순서 |
| [win32-cursor-and-child-window-input.md](win32-cursor-and-child-window-input.md) | 자식 창과 키보드 포커스, `GetAsyncKeyState`, `WM_SETCURSOR`와 커서 표시 카운트, 플랫폼 backend 없는 Dear ImGui |
| [windows-timer-resolution.md](windows-timer-resolution.md) | `Sleep`·`timeGetTime` 해상도, Win9x와 NT 계열 기본값 차이, Windows 10 2004의 프로세스 단위 규칙, `timeGetTime`과 `Sleep`의 분리, SDL3의 기본 요청 |
| [spdlog-runtime-logging.md](spdlog-runtime-logging.md) | spdlog multi-sink, immediate flush, `critical`과 프로젝트 `FATAL` 의미의 분리 |
| [sdl3-gamepad-input.md](sdl3-gamepad-input.md) | SDL3 joystick/gamepad 두 층, 가상 조이스틱으로 하는 테스트, Linux의 evdev·HIDAPI 경로 |

새 문서를 추가하거나 이름을 바꾸면 같은 작업에서 이 표를 갱신한다.

| [direct3d-cull-winding.md](direct3d-cull-winding.md) | Direct3D/OpenGL cull mode와 winding 변환 | Microsoft Direct3D 문서 기반 |

| [directsound-play-state.md](directsound-play-state.md) | DirectSound 재생 중 Play, cursor 보존, 명시적 위치 변경 계약 | Microsoft Learn 기반 |

*Update this table in the same task whenever a document is added or renamed.*

- [WSL Linux 검증 범위 / WSL Linux validation scope](wsl-linux-validation.md): WSL 개발 환경과 x86/x64 사용자 공간 검증의 한계 / WSL development and limits of x86/x64 userspace validation.

| [x86-page-fault-error-code.md](x86-page-fault-error-code.md) | x86 page-fault error code의 read/write, present, privilege bits | Intel SDM 기반 |
