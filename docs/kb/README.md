# 기술 지식 기반 색인 / Knowledge Base Index

이 디렉터리는 **일반적으로 통용되는** 기술 배경 지식을 주제별로 둔다. 원본 EZ2DJ 바이너리에서 확인한 프로젝트 고유 사실은 [`docs/analysis/`](../analysis/README.md)에 둔다.

*This directory holds **generally applicable** technical background, organized by topic. Project-specific facts confirmed against the original EZ2DJ binaries live in [`docs/analysis/`](../analysis/README.md).*

외부 자료에서 얻은 내용에는 원 사양, Microsoft 공식 문서, CPU 제조사 문서 같은 권위 있는 출처 링크를 가까운 위치에 남긴다.

*Place authoritative links — original specifications, official Microsoft documentation, CPU-vendor manuals — near anything derived from an external source.*

## 문서 / Documents

| 문서 | 내용 |
| --- | --- |
| [pe32-executable-format.md](pe32-executable-format.md) | PE32 실행 형식: 헤더 배치, 섹션, 재배치, import |
| [win32-hle-boundary.md](win32-hle-boundary.md) | Win32 API를 HLE 경계로 삼는 방식과 호출 규약 |
| [x86-32-guest-on-64-bit-host.md](x86-32-guest-on-64-bit-host.md) | 32비트 게스트를 64비트·WebAssembly 호스트에서 실행하는 선택지 |
| [web-x86-execution-engines.md](web-x86-execution-engines.md) | Web용 x86 실행 엔진 후보, 라이선스와 제한된 검증 결정 |
| [windows-wow64-process-introspection.md](windows-wow64-process-introspection.md) | suspended WOW64 process의 주 이미지 주소를 검증하는 제한된 방법 |
| [hasp4-parallel-dongle.md](hasp4-parallel-dongle.md) | HASP4 병렬포트 API 형태, Hardlock 구분, Win32 IOCTL 반환 계약 |
| [legacy-direct3d-immediate-mode.md](legacy-direct3d-immediate-mode.md) | DirectDraw에서 얻는 구형 Direct3D COM interface, hardware device 검색, proxy HLE 경계 |
| [legacy-directdraw-surface-gdi.md](legacy-directdraw-surface-gdi.md) | DirectDraw surface 생성, GDI GetDC/ReleaseDC interop와 HLE pixel-storage 계약 |
| [legacy-directsound-buffer.md](legacy-directsound-buffer.md) | DirectSound secondary buffer 생성, Lock/Unlock sample upload와 HRESULT 경계 |
| [sdl3-mixer-raw-audio.md](sdl3-mixer-raw-audio.md) | SDL3_mixer raw PCM, track 재생 상태와 zlib 라이선스 경계 |
| [x86-io-port-trapping.md](x86-io-port-trapping.md) | x86 `IN`/`OUT` 권한, Windows exception debug event, 제한된 장치 HLE trap |
| [windows-vectored-io-trap.md](windows-vectored-io-trap.md) | Windows vectored exception 처리 순서와 debugger 분리 실행 경계 |

새 문서를 추가하거나 이름을 바꾸면 같은 작업에서 이 표를 갱신한다.

*Update this table in the same task whenever a document is added or renamed.*
