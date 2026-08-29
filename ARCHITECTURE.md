# 아키텍처 / Architecture

이 문서는 현재 저장소에 구현된 구조와, 그 구조가 향후 확장될 방향을 기술한다. 구현이 바뀌면 같은 작업에서 이 문서를 갱신한다.

*This document describes the structure currently implemented in the repository and how it is intended to grow. Update it in the same task whenever the implementation changes.*

> [!NOTE]
> 아래 표기 규칙을 따른다. **[구현됨]** 은 저장소에 코드가 있고 빌드·테스트로 확인된 것, **[설계됨]** 은 인터페이스만 정해진 것, **[계획]** 은 아직 설계 문서만 있는 것이다.
>
> *Sections are marked **[Implemented]** when code exists and is verified by a build or test, **[Designed]** when only the interface is fixed, and **[Planned]** when only a design note exists.*

---

## 1. 실행 모델 / Execution model

게스트는 32비트 x86 Win32 PE 실행 파일이다. Windows 1차 host는 Win32 제품 CLI, shared `WindowsOriginalProcessBackend`와 original-child process로 구성된다. 진단 launcher도 같은 backend engine을 사용한다. Linux는 x86-64 제품 CLI와 별도 i386 helper를 `ExecutionBackend` 뒤에 연결해 원본 entry의 첫 통제 경계까지 실행하며 WebAssembly 실행 엔진은 후속 대상이다.

*The guest is a 32-bit x86 Win32 PE executable. The primary Windows host consists of the Win32 product CLI, a shared `WindowsOriginalProcessBackend`, and the original child process; the diagnostic launcher uses the same backend engine. Linux connects the x86-64 product CLI to a separate i386 helper behind `ExecutionBackend` and runs the original entry to its first controlled boundary. The WebAssembly execution engine remains later work.*

HLE 경계는 **Win32 import thunk**다. 로더가 게스트의 import table을 해석할 때, 각 API를 실제 DLL이 아니라 합성 gate 주소로 바인딩한다. 실행 backend가 gate 주소로 제어를 넘기면 C++ 구현이 호출된다.

*The HLE boundary is the **Win32 import thunk**. When the loader parses the guest import table, it binds each API to a synthetic gate address instead of a real DLL. Control transferred to a gate address dispatches into the C++ implementation.*

```mermaid
flowchart LR
    HDD["HDD directory<br/>(user-supplied path)"] --> SCAN["HDD scan<br/>+ target profile"]
    SCAN --> PE["PE32 image reader"]
    PE --> LOAD["Loader:<br/>sections, relocs, imports"]
    LOAD --> MEM["Guest address space<br/>(flat 32-bit)"]
    MEM --> EXEC["x86-32 execution backend"]
    EXEC -->|import gate| HLE["Win32 / DirectX HLE"]
    HLE --> VFS["Virtual file system"]
    HLE --> GFX["Graphics service"]
    HLE --> SND["Audio service"]
    HLE --> INP["Input service"]
    VFS --> HDD
    GFX --> PLAT["Platform backend<br/>windows / linux / web"]
    SND --> PLAT
    INP --> PLAT
    PLAT --> EXEC
```

---

## 2. 계층과 디렉터리 / Layers and directories

| 계층 / Layer | 경로 / Path | 책임 / Responsibility | 상태 |
| --- | --- | --- | --- |
| HDD 입력 | `include/re2dj/hdd/`, `src/hdd/` | 사용자가 준 디렉터리 검증, 대소문자 무시 경로 해석, 실행 파일 스캔 | **[구현됨]** |
| 게스트 경로 | `include/re2dj/storage/`, `src/storage/` | Win32 경로 파싱·정규화, 드라이브 문자 매핑, overlay 정책과 파일 테이블 | **[구현됨]** |
| 실행 파일 분석 | `include/re2dj/exe/`, `src/exe/` | PE32 헤더·섹션·디렉터리 판독 | **[구현됨]** (헤더·섹션), **[계획]** (import/reloc) |
| 타깃 프로파일 | `include/re2dj/target/`, `src/target/` | 버전별 실행 파일 경로, 작업 디렉터리, HLE 프로파일 ID | **[구현됨]** (자료구조·감지), **[계획]** (버전별 항목) |
| 런타임 | `include/re2dj/runtime/`, `src/runtime/` | 게스트 주소 공간, 레지스터 컨텍스트, 실행 backend 인터페이스 | **[계획]** |
| HLE | `include/re2dj/hle/`, `src/hle/` | kernel32/user32/gdi32/ddraw/dsound/dinput 모듈 테이블과 구현 | **[계획]** |
| 설정 | `include/re2dj/config/`, `src/config/` | INI 파싱, 키 바인딩, 실행 옵션 | **[계획]** |
| 플랫폼 | `src/platform/{windows,linux,web}/` | 창·렌더·오디오·입력·시간의 호스트 구현 | **[계획]** |
| 호스트 | `src/host/cli/` | 명령행 진입점 | **[구현됨]** |
| 도구 | `src/tools/{hdd_probe,pe_analyzer}/` | 비실행 분석 도구 | **[구현됨]** |

*The table above maps each layer to its directory, responsibility, and current status.*

---

## 3. HDD 디렉터리 입력 / HDD directory input **[구현됨]**

원본 HDD 내용은 **디렉터리 경로**로 입력받는다. 이미지 파일(`.img`, `.vhd`)을 직접 마운트하지 않는다. 사용자가 이미지를 풀어 놓은 디렉터리를 그대로 가리키면 된다.

*Original HDD contents arrive as a **directory path**. Image files are not mounted directly; the user points at a directory into which the image was already extracted.*

```
re2dj --hdd /path/to/ez2dj_hdd
```

`re2dj::hdd::HddRoot`가 그 경로를 소유하며 다음을 책임진다.

* 경로 존재와 디렉터리 여부 검증
* 게스트 상대 경로를 호스트 실제 경로로 해석. **대소문자를 무시한다.**
* 해석 결과 캐시

*`re2dj::hdd::HddRoot` owns the path and is responsible for validating it, resolving guest-relative paths to real host paths **case-insensitively**, and caching the result.*

### 왜 대소문자 무시 해석이 필요한가 / Why case-insensitive resolution is required

원본은 Windows에서 동작했으므로 게임 코드가 `DATA\Song01.EZ`처럼 실제 파일명과 대소문자가 다른 문자열로 파일을 열어도 동작했다. Linux와 Web 호스트의 파일 시스템은 대소문자를 구분하므로, 그대로 넘기면 열리지 않는다. `HddRoot`는 각 경로 구성 요소를 디렉터리 항목과 대소문자 무시로 대조해 실제 이름을 찾는다.

*The original ran on Windows, so game code could open `DATA\Song01.EZ` with a case that does not match the real file name. Linux and Web host file systems are case-sensitive, so the open would fail. `HddRoot` matches each path component against directory entries case-insensitively to find the real name.*

대조는 항상 디렉터리 나열 결과를 기준으로 한다. 정확히 일치하는 항목이 있으면 그것을 쓰고, 없으면 대소문자 무시로 처음 일치한 항목을 쓴다. 요청된 철자를 먼저 시도하면 Windows에서는 성공하고 Linux에서는 실패해, 같은 덤프가 호스트마다 다른 경로를 내놓는다. 나열 결과는 디렉터리 단위로 캐시한다.

*Matching always goes through the directory listing: an exact match wins, otherwise the first case-insensitive match does. Probing the requested spelling first would succeed on Windows and fail on Linux, so one dump would yield different paths per host. Listings are cached per directory.*

### 쓰기 정책 / Write policy **[구현됨]**

게스트의 파일 쓰기는 원본 디렉터리를 변경하지 않는다. 쓰기는 별도 overlay 디렉터리로 향하며, 읽기는 overlay를 먼저 조회한 뒤 원본으로 내려간다. overlay 우선 조회도 Win32 의미를 보존하기 위해 구성요소별 정확한 이름을 우선하고 없으면 ASCII 대소문자를 무시해 찾는다. `--hdd`는 전체 dump root이고 Windows x86 launcher는 선택된 target profile의 `working_directory_relative_path`를 안전하게 해석해 guest `D:\ez2dj`와 상대 경로의 source mount로 주입한다. 빈 working directory만 dump root 자체를 뜻한다.

*Guest file writes never modify the original directory. Writes go to a separate overlay directory, and reads consult the overlay before falling through to the original. Overlay lookup preserves Win32 semantics by preferring an exact component match and otherwise using an ASCII case-insensitive fallback. `--hdd` denotes the complete dump root; the Windows x86 launcher safely resolves the selected target profile's `working_directory_relative_path` and injects it as the source mount for guest `D:\ez2dj` and relative paths. Only an empty working directory maps directly to the dump root. The runtime clones an existing original file to the overlay before an `OPEN_EXISTING` write, so the original stays unchanged.*

```mermaid
flowchart TD
    R["Guest read: DATA\\SONG.EZ"] --> O{"overlay hit?"}
    O -->|yes| OV["overlay/DATA/SONG.EZ<br/>(case-insensitive)"]
    O -->|no| HD["hdd/DATA/SONG.EZ<br/>(case-insensitive)"]
    W["Guest write: SAVE\\SCORE.DAT"] --> OW["overlay/SAVE/SCORE.DAT"]
```

### Windows x86 이미지 로더 경계 / Windows x86 image-loader boundary **[구현됨]**

원본은 자산 이름을 검색 경로 테이블로 해석한 뒤 `CreateFileA`로 존재를 확인하고, 실제 비트맵은 `USER32!LoadImageA(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION)`로 읽는다. 따라서 VFS는 `KERNEL32!CreateFileA`만으로는 완결되지 않는다. `Re2djVfsLoadImageA`는 문자열 상대경로 + `IMAGE_BITMAP` + `LR_LOADFROMFILE` 조합에만 읽기 전용 HDD/overlay 매핑을 적용하고, resource ID·다른 image type·다른 flag는 원래 `LoadImageA`로 그대로 넘긴다. launcher는 이 패치 준비 상태를 다른 VFS 패치와 분리해 추적하므로, 실패해도 뒤따르는 device 패치를 조용히 건너뛰지 않고 `vfs_image_loader` 진단 이벤트로 드러난다.

원본 `.str` 장면 스크립트 로더는 `FILE_FLAG_NO_BUFFERING`으로 열고 sector 배수가 아닌 파일 크기 전체를 정렬되지 않은 pool 버퍼로 읽는다. Windows 9x는 이 정렬 요구를 강제하지 않았지만 NT 커널은 강제하므로 `ReadFile`이 `ERROR_INVALID_PARAMETER`로 실패한다. VFS `CreateFileA` 경계는 게스트가 기대한 OS 의미를 복원하기 위해 `FILE_FLAG_NO_BUFFERING`만 제거하고 나머지 flag는 그대로 전달한다. 캐싱 정책만 달라지고 게스트가 받는 바이트는 동일하다.

*The original `.str` scene-script loader opens with `FILE_FLAG_NO_BUFFERING` and then reads a whole non-sector-multiple file into an unaligned pool buffer. Windows 9x did not enforce those alignment rules but the NT kernel does, so `ReadFile` fails with `ERROR_INVALID_PARAMETER`. To restore the OS semantics the guest expects, the VFS `CreateFileA` boundary strips `FILE_FLAG_NO_BUFFERING` alone and forwards every other flag; only the caching policy changes while the bytes the guest receives stay identical.*

자산 경계 진단은 `.bmp`와 `.str` 요청에 대해 호출 API, 게스트 요청 경로, 매핑된 호스트 경로, 성공 여부와 Win32 오류를 별도 bounded 로그에 남긴다. 확장자마다 상한을 따로 두어 대량 비트맵 스윕이 드문 script 요청을 가리지 못하게 한다. 파일 내용은 기록하지 않는다. 경로 해석은 게스트 안에서 일어나므로 존재하지 않는 후보에 `ERROR_FILE_NOT_FOUND`를 그대로 돌려주는 것이 계약이며, 진단 자체의 파일 I/O가 게스트가 읽을 last error를 덮지 않도록 보고 뒤에 오류를 확정한다. 확인된 호출 지점과 인자는 [자산 로딩 경로 분석](docs/analysis/ez2dj-asset-loading-path.md)에 있다.

*The original resolves an asset name through a search-path table, probes existence with `CreateFileA`, and then reads the actual bitmap with `USER32!LoadImageA(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION)`, so a `KERNEL32!CreateFileA`-only VFS is incomplete. `Re2djVfsLoadImageA` applies the read-only HDD/overlay mapping solely to the string-relative-path + `IMAGE_BITMAP` + `LR_LOADFROMFILE` combination and forwards resource IDs, other image types, and other flags unchanged. The launcher tracks this patch's readiness separately from the other VFS patches, so a failure surfaces as a `vfs_image_loader` diagnostic event instead of silently skipping the device patches that follow.*

*The asset-boundary diagnostic records the calling API, guest request path, mapped host path, success status, and Win32 error for `.bmp` and `.str` requests in a separate bounded log, with a per-extension bound so a large bitmap sweep cannot hide the rarer script requests. It never records file contents. Because path resolution happens inside the guest, returning `ERROR_FILE_NOT_FOUND` for absent candidates is the contract, and the error is committed after reporting so the diagnostic's own file I/O cannot overwrite what the guest reads back. Confirmed call sites and arguments are in the [asset loading path analysis](docs/analysis/ez2dj-asset-loading-path.md).*

### Windows x86 가상 디바이스 경계 / Windows x86 virtual-device boundary **[구현됨]**

주입 runtime은 launcher의 `--device-mock-lptdi` 정책이 켜졌을 때 `CreateFileA("\\.\LPTDI*")`를 import thunk에서 가로채 예약 범위 `0xFEED0001..0xFEED00FF`의 synthetic handle을 반환한다. 파일 wrapper는 이 핸들에 EOF형 read, 쓰기 거부, seek/size 미지원, `FILE_TYPE_CHAR`, 성공 close 계약을 제공한다. 정책이 꺼져 있거나 다른 경로이면 기존 VFS/host 경로로 내려간다. 기본 `--device-mock-lptdi`는 `DeviceIoControl`을 host에 전달해 실패 baseline을 보존한다. 실험 옵션은 canonical IAT를 runtime wrapper로 바꾼다. zero/full-success mode 외에 `--device-mock-lptdi-response-profile`은 공용 parser가 검증한 code별 exact-size bytes를 주입한다. `--device-mock-lptdi-target-state`는 공용 `lptdi_challenge_response` 변환으로 실행별 두 번째 input seed에 적응하는 response offset 4~11을 만들며, guest XOR 뒤 선택한 8바이트 상태가 남게 한다. 각 IOCTL 정책은 상호 배타적이고 synthetic handle·확인된 code에만 적용된다. `--lptdi-post-ioctl-trace`는 합성 래퍼도 API watch에 등록하고 guest 복귀 후 같은 allocation의 명령을 bounded single-step으로 기록하며, 외부 call은 one-shot return breakpoint로 건너뛴다. syscall resume breakpoint와 trace breakpoint가 같은 주소를 공유하면 원래 바이트 소유권을 trace에 넘기고, trace가 이미 복귀 대기 중일 때 외부 single-step을 중복 처리하지 않는다. launcher는 원본 entry의 첫 `GetVersion` caller에서 `.data` initializer 8 DWORD를 한 번 기록해 복원 여부를 검증한다. API trace는 USER32도 해석해 창·display 초기화의 파일 접근 전 실패를 귀속한다. 다른 handle은 항상 host로 전달한다.

*When launcher policy `--device-mock-lptdi` is enabled, the injected runtime intercepts `CreateFileA("\\.\LPTDI*")` at the import thunk and returns a synthetic handle in reserved range `0xFEED0001..0xFEED00FF`. File wrappers give that handle EOF-style reads, denied writes, unsupported seek/size, `FILE_TYPE_CHAR`, and successful close semantics. Disabled policy and all other paths fall through to the existing VFS/host path. The base option forwards DeviceIoControl to preserve the host-failure baseline, while experimental options replace the canonical IAT slot. Besides zero/full-success modes, `--device-mock-lptdi-response-profile` injects code-specific exact-size bytes validated by the shared parser. `--device-mock-lptdi-target-state` uses the shared `lptdi_challenge_response` transform to adapt response offsets 4 through 11 to each second-input seed, leaving the selected eight-byte state after the guest XOR. IOCTL policies are mutually exclusive and apply only to synthetic handles and confirmed codes. `--lptdi-post-ioctl-trace` also registers the synthetic wrapper as an API watch and records bounded same-allocation guest instructions after return, skipping external calls with one-shot return breakpoints. When a syscall-resume and trace breakpoint share an address, original-byte ownership transfers to the trace, and an external single-step is not reprocessed while that trace already awaits resume. The launcher records the eight-DWORD `.data` initializer once at the original entry's first `GetVersion` caller to verify restoration. API tracing also resolves USER32 and observes window/display startup calls so pre-file initialization failures remain attributable. Other handles always forward to the host.*

### Windows x86 논리 display mode 경계 / Windows x86 logical display-mode boundary **[구현됨]**

launcher의 `--hle-display-mode`는 원본 USER32 import thunk의 `ChangeDisplaySettingsExA`만 주입 runtime wrapper로 바꾼다. wrapper는 관찰된 null-device, 640×480×16, `CDS_UPDATEREGISTRY`, null-reserved 요청만 `DISP_CHANGE_SUCCESSFUL`로 처리하고 host desktop은 변경하지 않는다. 다른 mode 변경과 restore 요청은 host USER32로 전달한다. 이 정책은 guest가 기대하는 표시 mode 계약만 보존하며 실제 surface 출력과 scaling은 후속 그래픽 backend의 책임으로 남긴다.

*Launcher option `--hle-display-mode` replaces only the original USER32 `ChangeDisplaySettingsExA` import thunk with an injected-runtime wrapper. The wrapper returns `DISP_CHANGE_SUCCESSFUL` without changing the host desktop only for the observed null-device 640×480×16 `CDS_UPDATEREGISTRY` request with a null reserved argument. Other mode changes and restore requests forward to host USER32. This preserves the guest-visible mode contract while leaving real surface output and scaling to a later graphics backend.*

`--d3d-init-trace`는 1st SE 원본 초기화 coordinator의 다섯 return site에 일회성 breakpoint를 두고 반환값, DirectDraw/Direct3D COM 전역, phase marker를 기록한 뒤 원본 바이트를 복원한다. 이 진단은 guest 결과를 바꾸지 않는다. 현재 증거는 import-thunk display HLE 다음 경계가 `IDirect3D3::FindDevice` 같은 COM method임을 보여 준다. 후속 graphics HLE는 host COM vtable을 직접 수정하지 않고 `DirectDrawCreate` import에서 시작하는 교체 가능한 proxy interface 계층으로 설계해야 한다.

*`--d3d-init-trace` places one-shot breakpoints at the five return sites of the 1st SE initialization coordinator, records results, DirectDraw/Direct3D COM globals, and phase markers, then restores the original bytes. It does not change guest results. Current evidence places the next boundary after import-thunk display HLE at a COM method such as `IDirect3D3::FindDevice`; the later graphics HLE should therefore use replaceable proxy interfaces rooted at the `DirectDrawCreate` import rather than patching a host COM vtable in place.*

### Direct3D 3 OpenGL HLE / Direct3D 3 OpenGL HLE **[부분 구현 / Partially implemented]**

`IDirect3DTexture2::Load`는 같은 DirectDraw root에 속한 동일 크기 RGB565 texture surface 사이에서 pixel row와 `DDCKEY_SRCBLT`를 destination으로 복사하고 content revision을 증가시킨다. 따라서 GDI로 채운 source texture와 draw에 사용하는 destination texture가 분리된 원본 경로도 surface identity/cache 계약을 유지한다. 크기나 format 변환은 지원하지 않으며 명시적 실패를 반환한다. 상세 계약은 [Direct3D 3 texture Load 복사 설계](docs/design/20260829-088-direct3d-texture-load.md)에 둔다.

*`IDirect3DTexture2::Load` copies pixel rows and `DDCKEY_SRCBLT` between equal-sized RGB565 texture surfaces owned by the same DirectDraw root, then increments the destination content revision. This preserves the surface-identity/cache contract when the original populates one source texture through GDI but draws a separate destination texture. Format conversion and scaling are unsupported and return an explicit failure. See the [Direct3D 3 texture Load copy design](docs/design/20260829-088-direct3d-texture-load.md).*

`DrawPrimitive`의 플랫폼 중립 정점 경로는 기존 screen-space `D3DFVF_TLVERTEX(0x1c4)`뿐 아니라 원본에서 관찰된 32바이트 `D3DVERTEX(0x112)`와 `D3DLVERTEX(0x1e2)`를 지원한다. 변환 전 XYZ는 저장된 Direct3D row-vector world → view → projection matrix와 `D3DVIEWPORT2`를 거쳐 기존 `TransformedLitVertex` 명령으로 정규화된다. `0x112`의 normal은 lighting 증거가 확보될 때까지 건너뛰고 diffuse white를 사용하며, `0x1e2`의 `RESERVED1` 4바이트를 실제 stride와 field offset에 포함한다. 상세 계약은 [변환 전 Direct3D 3 정점 draw 설계](docs/design/20260829-089-untransformed-direct3d-draw.md)에 둔다.

*The platform-neutral `DrawPrimitive` vertex path supports not only the existing screen-space `D3DFVF_TLVERTEX(0x1c4)` but also the observed 32-byte `D3DVERTEX(0x112)` and `D3DLVERTEX(0x1e2)` layouts. Pre-transform XYZ passes through retained Direct3D row-vector world, view, and projection matrices plus `D3DVIEWPORT2`, producing the existing `TransformedLitVertex` command. The `0x112` normal is skipped with white diffuse until lighting evidence is available, while the four-byte `RESERVED1` field in `0x1e2` participates in the actual stride and offsets. See the [untransformed Direct3D 3 vertex draw design](docs/design/20260829-089-untransformed-direct3d-draw.md).*

그래픽 HLE는 `DirectDrawCreate` import gate, guest 소유 32비트 COM facade, 플랫폼 중립 legacy graphics core, 교체 가능한 `RenderBackend`의 네 계층으로 구성한다. COM facade는 `IDirectDraw`/`IDirectDraw4`/`IDirect3D3`/`IDirect3DDevice3` identity와 vtable, reference count를 보존한다. legacy core는 guest surface의 16비트 layout·pitch·lock 상태와 정규화된 fixed-function state를 소유하며 OpenGL type이나 platform context API를 포함하지 않는다.

첫 backend는 desktop OpenGL이고 Web은 같은 `RenderBackend` 계약을 WebGL 2로 구현한다. deprecated OpenGL fixed-function state에 직접 의존하지 않고 내부 shader로 관찰된 Direct3D 3 상태를 재현한다. `FindDevice`는 host HAL 열거를 전달하지 않고 구현된 capability만 선언하는 가상 hardware device를 노출한다. primary/back/depth/texture surface는 논리 객체로 유지하며, 기존 640×480×16 display-mode HLE와 결합해 host desktop mode를 바꾸지 않고 present한다. 상세 단계와 미확정 항목은 [Direct3D 3 OpenGL HLE 설계](docs/design/20260825-061-direct3d3-opengl-hle.md)에 둔다.

Windows x86 COM 연결은 `src/platform/windows/direct3d3_com_facade.*`에만 남고, 실제 렌더러는 `include/re2dj/graphics/sdl3_opengl_backend.h`와 `src/graphics/sdl3_opengl_backend.cpp`의 공용 SDL3/OpenGL backend다. Windows에서는 SDL3가 원본이 만든 HWND를 external window로 감싸고, Linux와 Web에서는 같은 backend가 SDL window/canvas를 소유할 수 있다. SDL3가 video subsystem, OpenGL context, 함수 해석, drawable 크기와 swap을 제공하므로 WGL과 `opengl32` 직접 의존성은 없다. desktop은 OpenGL 2.1/GLSL 1.20, Web은 OpenGL ES 2.0/WebGL 호환 GLSL ES 1.00 분기를 사용한다. `LegacyDrawCommand`와 `LegacyFixedFunctionState`는 확인된 XYZRHW, stage-zero modulate, linear filter, alpha test와 blend factor를 전달한다. `LegacyTextureView`는 RGB565 backing, stable identity/revision과 inclusive source color-key 범위를 보존하고 backend는 surface별 texture cache에서 변경 revision만 upload한다. `LegacyVertexBuffer`와 Windows `IDirect3DVertexBuffer` facade는 XYZ/NORMAL/TEX1 121개 정점을 3,872바이트 storage로 보존한다. 상세 변경은 [SDL3/OpenGL 공용 backend 설계](docs/design/20260827-076-sdl3-opengl-shared-backend.md)에 둔다.

Win32 창 정책은 `src/platform/windows/window_mode.*`에 분리한다. 제품은 기본적으로 원본 HWND에 `re2DJ v<version> - Build <date> - SDL3 OpenGL - FPS : <value>` 상태 제목, 시스템 메뉴·최소화·최대화 버튼과 resize frame, 640×480 논리 표시를 정확히 2배 확대하는 1280×960 client 영역을 적용하고 monitor work area 중앙에 둔다. 성공한 DirectDraw `Flip`은 약 1초마다 소수점 한 자리 FPS를 갱신한다. 원본 popup WndProc가 표준 caption 처리를 보장하지 않으므로 adapter가 non-client 계산·그리기·hit-test와 close 이외 system command를 `DefWindowProcA`에 직접 맡긴다. host 제목과 icon 갱신은 현재 WndProc chain을 우회해 `DefWindowProcA`로 직접 저장하며, 이후 adapter로 들어오는 일반 `WM_SETTEXT`/`WM_SETICON` replacement는 적용하지 않는다. 실제 실행에서 저장 상태가 정상인데 DWM이 caption content를 소거하는 것이 확인되어 windowed는 `DWMNCRP_DISABLED`로 USER32 기본 title/icon을 표시하고 fullscreen은 `DWMNCRP_ENABLED`로 복원한다. 외부 `--fullscreen`은 `OriginalProcessOptions`와 launcher를 거쳐 injected runtime export로 전달되며, 원본 EXE·INI를 바꾸지 않고 같은 HWND를 monitor bounds의 `WS_POPUP`으로 전환한다. 어느 모드도 host display mode를 변경하지 않는다. `window_mode`는 원본 WndProc를 window property에 보존하고 adapter를 한 번 설치한다. adapter는 `WM_CLOSE`와 제목 표시줄의 최초 `WM_SYSCOMMAND/SC_CLOSE`를 모두 host close로 판정하고 원본 WndProc에 정리 기회를 준다. 실제 창에서 adapter가 우회되거나 close 뒤 guest가 다시 `Flip`하지 않는 경우까지 처리하도록 window mode 적용 완료 시 process당 하나의 worker를 시작해 50 ms 간격으로 최신 표시 HWND의 파괴·숨김을 검사한다. 최소화는 visible 상태이므로 종료로 오인하지 않는다. 제품 trace `20260829-112237-831`은 close message와 `visible=0` watcher exit 뒤 `ExitProcess(0)`이 process를 종료 중 상태에 남기고 Debug DLL을 잠근 사실을 확인했다. 따라서 확인된 host-close 경계는 원본 정리 뒤 현재 process 자신에게만 `TerminateProcess(..., 0)`을 사용해 detach 교착을 우회한다. launcher의 외부 PID 강제 종료에는 사용하지 않는다. 기존 graphics trace에는 watcher target/start, close/exit와 최초 10분간 초당 하나의 valid/visible 표본을 최대 600개 기록한다. 상세 창 정책은 [Win32 창 모드와 메시지 pump 설계](docs/design/20260828-084-window-mode-message-pump.md), 종료 전달은 [Win32 창 닫기와 원본 프로세스 종료 설계](docs/design/20260829-090-window-close-process-exit.md), 제목과 기본 확대는 [Win32 실행 창 제목과 기본 2배 확대 설계](docs/design/20260829-091-window-title-default-scale.md)에 둔다.

컬러키는 alpha 값이 아니라 **discard 조건**으로 구현한다. Direct3D는 `D3DRENDERSTATE_COLORKEYENABLE`이 켜진 동안 키에 일치하는 texel을 blend factor와 무관하게 버린다. texel alpha만으로 표현하면 `srcblend=ONE`, `dstblend=ZERO`인 복사 blend에서 keyed texel이 키 색 그대로 기록된다. backend는 upload 때 일치 texel의 alpha를 0으로 두고, 게스트 `COLORKEYENABLE` 상태로만 gate되는 shader 분기에서 그 texel을 버린다. 게스트 alpha test 분기는 원래 의미대로 별도로 남으며, 컬러키가 꺼진 draw는 영향을 받지 않는다. 특히 `srcblend=ZERO`, `dstblend=SRCCOLOR`인 곱셈 mask pass는 keyed texel을 버리면 mask가 무의미해지므로 반드시 게스트 상태로만 판단한다.

*Color keying is implemented as a **discard condition**, not as an alpha value. While `D3DRENDERSTATE_COLORKEYENABLE` is set, Direct3D drops key-matching texels regardless of the blend factors; expressing that through texel alpha alone writes them out in the key color under a copy blend of `ONE` and `ZERO`. The backend leaves matching texels at zero alpha during upload and drops them in a shader branch gated solely on the guest's `COLORKEYENABLE`. The guest alpha-test branch keeps its own meaning separately, and draws with keying disabled are unaffected — in particular the multiplicative mask passes using `srcblend=ZERO` and `dstblend=SRCCOLOR`, which would become meaningless if their keyed texels were dropped.*

DirectDraw 2D 경로는 texture뿐 아니라 `DDSCAPS_OFFSCREENPLAIN` surface에도 같은 RGB565 GDI backing과 stable identity/revision을 부여한다. 공용 `CopyRgb565Rectangle` 계약이 동일 크기 사각형과 inclusive source color key를 처리하며, facade의 `Blt`/`BltFast`는 CPU backing을 갱신하고 primary/back destination이면 같은 source 사각형을 OpenGL frame에 합성한다. stretch, ROP과 destination color key처럼 아직 관찰되지 않은 조합은 명시적으로 거절한다.

직접 x86 `IN`/`OUT`에는 교체 가능한 Win32 import가 없으므로 `Ez2DjIoBoard`의 플랫폼 중립 button/turntable/coin/light 상태를 `LegacyIoPortBus`가 raw byte 계약으로 변환하여 `ExecutionBackend` 경계에 둔다. Windows x86 진단 모드는 1st SE에서 확인된 helper RVA와 opcode, port만 debugger의 `EXCEPTION_PRIV_INSTRUCTION`으로 처리한다. 실제 실행용 `--run-detached`는 원본 entry 복원, runtime 주입과 IAT 검증 뒤 debugger를 분리하고, injected runtime의 vectored handler가 같은 제한 계약을 process 안에서 처리한다. 원본 instruction byte는 수정하지 않는다. 기본 idle bytes는 `ff ff 80 80 00 ff`다. port `0x105`는 keyboard coin false→true마다 1 증가하고 read로 소비되지 않는 modulo-256 counter다. 제품의 선택적 `--io-config`는 host INI의 absolute path를 runtime에 주입해 Windows keyboard adapter를 활성화한다. 외부 공개 구현에서 교차 확인한 물리 의미는 [I/O 분석](docs/analysis/ez2dj-io-map.md)처럼 원본 확인 사실과 구분한다.

*Direct x86 `IN`/`OUT` has no replaceable Win32 import. `LegacyIoPortBus` therefore adapts platform-neutral button, turntable, coin, and light state owned by `Ez2DjIoBoard` to the raw-byte contract at the `ExecutionBackend` boundary. The Windows x86 diagnostic path accepts only the helper RVAs, opcodes, and ports confirmed for 1st SE; detached product execution uses the equivalent injected vectored handler without modifying original instruction bytes. Idle bytes are `ff ff 80 80 00 ff`. Port `0x105` is a modulo-256 counter incremented on each keyboard coin false-to-true edge and is not consumed by reads. Optional product `--io-config` injects an absolute host INI path for the Windows keyboard adapter. Meanings cross-checked against a public implementation remain distinct from facts confirmed in the original executable.*

*The graphics HLE is split into a `DirectDrawCreate` import gate, guest-owned 32-bit COM facades, a platform-neutral legacy graphics core, and a replaceable `RenderBackend`. The COM layer preserves interface identity, vtables, and reference counts; the common core owns guest-visible surface layouts and normalized fixed-function state without OpenGL or platform context types. Desktop OpenGL is the first backend and WebGL 2 implements the same contract for Web. Internal shaders reproduce observed Direct3D 3 behavior, while a conservative virtual hardware device replaces host HAL enumeration. Logical primary, back, depth, and texture surfaces present through the existing 640×480×16 display contract without changing the host desktop mode.*

*Only the Windows x86 COM bridge remains platform-specific. The shared SDL3/OpenGL backend wraps the original HWND as an external SDL window on Windows and can own an SDL window or canvas on Linux and the Web. SDL3 supplies video initialization, context management, GL symbol resolution, drawable sizing, and swapping, removing direct WGL and `opengl32` dependencies. Desktop builds use OpenGL 2.1 with GLSL 1.20; Web uses an OpenGL ES 2.0/WebGL-compatible GLSL ES 1.00 branch. Stable texture identity/revision, inclusive RGB565 source keys, and the observed fixed-function state feed the same per-surface texture cache on all three builds. See the [shared SDL3/OpenGL backend design](docs/design/20260827-076-sdl3-opengl-shared-backend.md).*

*Win32 window policy is isolated in `src/platform/windows/window_mode.*`. By default, the product applies a `re2DJ v<version> - Build <date> - SDL3 OpenGL - FPS : <value>` status title, a normal system menu with minimize/maximize buttons and resize frame, a 1280x960 client area that scales the unchanged 640x480 logical display by exactly 2x, and centered work-area placement to the original HWND. Successful DirectDraw `Flip` calls update the one-decimal FPS value about once per second. Because the original popup WndProc does not guarantee standard-caption behavior, the adapter routes non-client calculation/painting/hit testing and non-close system commands directly through `DefWindowProcA`. Host title/icon updates bypass the active WndProc chain and store state directly through `DefWindowProcA`; ordinary later `WM_SETTEXT`/`WM_SETICON` replacements entering the adapter are not applied. Actual execution confirmed that DWM removed caption content despite valid stored state, so windowed mode uses `DWMNCRP_DISABLED` to expose the USER32 default title/icon and fullscreen restores `DWMNCRP_ENABLED`. External `--fullscreen` travels through `OriginalProcessOptions`, the launcher, and an injected-runtime export, switching that same HWND to monitor-bounds `WS_POPUP` without changing the original EXE, INI, or host display mode. `window_mode` preserves the original WndProc in a window property and installs one adapter. The adapter treats both `WM_CLOSE` and the title bar's initial `WM_SYSCOMMAND/SC_CLOSE` as host close and gives the original WndProc a cleanup opportunity. To handle adapter bypass and cases where the guest never calls `Flip` again, successful window-mode application starts one worker per process that checks the latest display HWND for destruction or hiding every 50 ms. A minimized window remains visible and is not mistaken for close. Product trace `20260829-112237-831` confirms that `ExitProcess(0)` after close messages and `visible=0` watcher exit leaves the process terminating and locks the Debug DLL. The confirmed host-close boundary therefore uses `TerminateProcess(..., 0)` only on the current process after original cleanup, bypassing detach deadlock; launcher-side arbitrary-PID termination is not used. The existing graphics trace records watcher target/start, close/exit, and at most 600 one-per-second valid/visible samples during the first ten minutes. See the [Win32 window-mode and message-pump design](docs/design/20260828-084-window-mode-message-pump.md), [Win32 window-close process-exit design](docs/design/20260829-090-window-close-process-exit.md), and [Win32 title/default-scale design](docs/design/20260829-091-window-title-default-scale.md).*

*The DirectDraw 2D path also gives `DDSCAPS_OFFSCREENPLAIN` surfaces RGB565 GDI backing and stable identity/revision. A common `CopyRgb565Rectangle` contract handles equal-sized rectangles and inclusive source color keys; facade `Blt`/`BltFast` update CPU backing and composite the same source rectangle into the OpenGL frame for primary/back destinations. Unobserved stretch, ROP, and destination-key combinations remain explicit failures.*

*Direct x86 `IN`/`OUT` has no Win32 import to replace, so a platform-neutral raw byte bus under `include/re2dj/input/` forms an execution-backend boundary. Debugger mode handles only confirmed 1st SE helper RVAs, opcodes, and ports. For real execution, `--run-detached` restores and verifies the guest first, detaches the debugger, and lets an injected vectored handler enforce the same narrow contract in-process without changing original instruction bytes.*

---

## 4. 게스트 경로 변환 / Guest path translation **[구현됨]**

게스트가 넘기는 경로는 Win32 문법이다. `re2dj::storage::GuestPath`가 이를 파싱해 다음을 구분한다.

* 드라이브 문자 (`C:\...`)
* 드라이브 상대 (`C:FOO`)
* 루트 상대 (`\FOO`)
* 순수 상대 (`FOO\BAR`)
* UNC (`\\server\share`) — 현재는 거부한다

*Guest paths use Win32 syntax. `re2dj::storage::GuestPath` parses them and distinguishes drive-absolute, drive-relative, root-relative, plain relative, and UNC forms. UNC is rejected for now.*

정규화 단계에서 `.`을 제거하고 `..`을 접으며, 구분자로 `/`와 `\`를 모두 받아들인다. 루트를 벗어나는 `..`은 실패로 처리해 HDD 디렉터리 바깥을 참조하지 못하게 한다.

*Normalization drops `.`, folds `..`, and accepts both `/` and `\` as separators. A `..` that would escape the root fails, so nothing outside the HDD directory is reachable.*

---

## 5. PE32 이미지 판독 / PE32 image reading **[구현됨]**

`re2dj::exe::PeImageInfo`는 원본 실행 파일에서 다음을 읽는다.

* DOS 헤더와 `e_lfanew`
* COFF 파일 헤더: machine, section 수, characteristics
* Optional header: magic(PE32 / PE32+), image base, entry point RVA, section/file alignment, subsystem, DLL characteristics
* Section 테이블: 이름, VA, 가상/원시 크기, 원시 오프셋, 특성
* Data directory: import, relocation, resource, TLS 위치

*`re2dj::exe::PeImageInfo` reads the DOS header, COFF file header, optional header, section table, and data directories from an original executable.*

이 판독기 자체는 **적재하지 않는다.** 파일을 읽어 구조만 보고한다. 실제 적재(섹션 매핑, 재배치, import 바인딩)는 구현된 런타임 계층의 `LoadPe32Image()`가 수행한다.

*This reader does **not** load anything; it reports structure only. The implemented runtime `LoadPe32Image()` performs section mapping, relocation, and import binding.*

HDD 스캔은 이 판독기를 사용해 각 실행 파일을 분류한다. `machine`이 x86(0x014C)이고 magic이 PE32(0x10B)이며 subsystem이 GUI인 항목이 게임 실행 파일 후보다.

*The HDD scan uses this reader to classify each executable. An entry with x86 machine (0x014C), PE32 magic (0x10B), and the GUI subsystem is a game-executable candidate.*

---

## 6. 타깃 프로파일 / Target profiles **[구현됨: 자료구조]**

버전별 차이는 `re2dj::target::TargetProfile`로 분리한다.

| 필드 | 의미 |
| --- | --- |
| `id` | 명령행에서 고르는 짧은 식별자 |
| `display_name` | 사람이 읽는 이름 |
| `executable_relative_path` | HDD 루트 기준 실행 파일 경로. 지문이 맞을 때 채워진다 |
| `working_directory_relative_path` | 호스트 쪽 작업 디렉터리 |
| `guest_drive_letter` | 게스트가 자신이 실행된다고 믿는 드라이브. 근거가 없으면 `\0` |
| `guest_directory` | 같은 근거의 Win32 디렉터리. 근거가 없으면 빈 문자열 |
| `hle_profile_id` | 적용할 HLE 서비스 집합 |
| `detected` | 내장 표가 아니라 스캔에서 나온 것인지 |
| `bring_up_target` | 캐비닛이 실행한 것이 아니라 개발용인지 |
| `note` | 사람이 읽는 단서와 한계 |

*Version-specific differences live in `re2dj::target::TargetProfile`.*

### 지문으로 덤프를 식별한다 / Dumps are identified by fingerprint

내장 프로파일은 **실행 파일 이름 + 그 옆에 반드시 있어야 하는 항목 목록**으로 덤프를 식별한다. 파일 크기나 해시는 리비전마다 달라져 정상 덤프를 거부하므로 쓰지 않는다.

*A built-in profile identifies a dump by an **executable name plus the entries that must sit beside it**. File size and hashes are rejected as keys because they vary per revision and would reject a legitimate dump.*

| 프로파일 | 실행 파일 | 필수 형제 항목 |
| --- | --- | --- |
| `ez2dj1stse` | `ez2dj.exe` | `ez2dj1.exe`, `ez2dj.ini`, `System.ini`, `Songs`, `System` |
| `ez2dj1stse_unpacked` | `ez2dj1.exe` | `ez2dj.exe`, `ez2dj.ini`, `Songs`, `System` |
| `ez2dj3rd` | `EZ2DJ.EXE` | `EZ2DJ.INI`, `FONTKR.DAT`, `BG`, `Sound`, `system` |

형제 항목이 필요한 이유는 경로 해석이 대소문자를 무시하기 때문이다. 3rd의 `EZ2DJ.EXE`라는 이름만으로는 1st SE의 `ez2dj.exe`와 구별되지 않는다. 두 지문은 서로소라서 오인이 일어나지 않는다.

*The siblings are required because path resolution is case-insensitive, so the name `EZ2DJ.EXE` alone does not distinguish 3rd from 1st SE. The two fingerprints are disjoint, so neither dump matches the other.*

실행 파일을 스캔 결과에서 찾으므로 사용자가 상위 디렉터리를 지정해도 걸린다. 형제 항목은 그 실행 파일의 디렉터리를 기준으로 확인한다.

*Matching searches the scan, so pointing at a parent directory still works; siblings are checked relative to the matched executable's own directory.*

### 순서와 감지 / Ordering and detection

`BuildTargetProfiles()`는 내장 프로파일 중 지문이 맞는 것을 먼저 놓고, 내장이 가져가지 않은 실행 파일에 대해서만 `DetectTargetProfiles()`를 돌린다. 목록의 첫 항목이 기본 타깃이다.

내장 프로파일이 없는 버전의 덤프도 감지만으로 계속 동작한다. 내장 항목은 **실제로 확인한 덤프에 대해서만** 추가한다.

*`BuildTargetProfiles()` puts fingerprint matches first and runs detection only over the executables no built-in claimed; the first entry is the default target. A dump of a version with no built-in profile still works through detection alone. Built-in entries are added **only for dumps that were actually inspected**.*

---

## 7. 런타임 계층 / Runtime layer **[부분 구현됨]**

```mermaid
flowchart TD
    subgraph runtime["re2dj::runtime"]
        GA["GuestAddress<br/>32-bit value type"]
        AS["AddressSpace<br/>page-granular, host-backed"]
        CTX["GuestContext<br/>GPR / EFLAGS / x87 / SSE"]
        BE["ExecutionBackend<br/>interface"]
    end
    BE --> NAT["NativeHelperBackend<br/>Windows adapter implemented"]
    BE -.candidate.-> ORIG["Windows original-process loader<br/>suspended-image probe verified"]
    BE --> LNX["Linux i386 helper<br/>gate probe implemented"]
    BE --> WEB["Web execution engine<br/>v86 spike rejected"]
    WEB -.fallback.-> INT["Custom interpreter<br/>(deferred)"]
    AS --> GATE["Import gate region"]
    GATE --> DISP["HLE dispatcher"]
```

* `GuestAddress`는 host pointer로 변환되지 않는 32비트 값 타입이다.
* `AddressSpace`는 게스트의 평탄한 4 GiB 주소 공간 중 실제로 커밋된 페이지만 호스트 메모리에 둔다. 게스트 주소를 호스트 포인터로 노출하지 않고 `Read8/16/32`, `Write8/16/32` 접근자를 통해서만 다룬다.
* `LoadPe32Image()`는 헤더와 섹션을 매핑하고 zero-fill한 뒤, `IMAGE_REL_BASED_HIGHLOW` 재배치를 적용하고 이름/ordinal import를 합성 gate에 바인딩한다. 실패 시 호출자가 제공한 주소 공간과 gate 표는 바뀌지 않는다.
* `ImportGateTable`은 기본적으로 `0xF0000000`부터 16바이트 간격의 주소를 배정한다. 이 범위에는 실제 명령어가 없으며 Stage 3 backend가 HLE dispatcher로 전달한다.
* `ExecutionBackend` event/reply 인터페이스는 이미지 준비, 실행 시작, event 대기, import 완료 응답, 중단 요청을 분리한다. event에는 backend-local thread ID, guest EIP/ESP와 gate 주소만 담고 host pointer를 넣지 않는다. import 응답은 EAX/EDX, stack 정리 byte 수, 계속/중단 action을 전달한다.
* Windows `NativeHelperBackend`는 x86 helper process와 anonymous pipe protocol v3를 `ExecutionBackend` 뒤에 캡슐화한다. PImpl 공개 header에는 Windows type이 없고, adapter가 packet 순서, 상태 검증과 child 종료를 소유한다. `native_pe_image`는 requested base mapping, `HIGHLOW` relocation, section protection과 process-attach TLS callback을 담당한다. helper는 PE32의 이름/ordinal import를 순회해 `ImportGateTable`의 synthetic gate마다 실행 가능한 x86 thunk를 만들고 실제 thunk 주소를 IAT에 쓴다. load 뒤 module/name/ordinal/gate metadata를 adapter에 보내 `LoadedPeImage.imports`를 채운다. gate가 멈춘 동안 adapter는 guest memory를 읽고 쓴 뒤 EDX:EAX와 동적 stack 정리 크기를 응답한다. protocol은 event 하나를 직렬 처리하며 TLS storage/index와 병렬 guest thread는 후속 확장이다.
* Win32 original-process engine은 `re2dj_windows_original_process_backend` static library에 있으며 제품 `re2dj --run`과 진단 `re2dj_windows_x86_launcher_probe`가 공유한다. 제품 facade는 현재 검증된 `ez2dj1stse`만 받아 command line·Windows directory·VFS/overlay·Direct3D 3·DirectSound·legacy I/O·LPTDI target-state와 detached runtime policy를 구성한다. 미검증 target은 process 생성 전에 거절한다. 진단 executable은 기존 세부 option parser와 trace 기능을 그대로 노출한다.
* `re2dj_windows_x86_launcher_probe`는 기본 Win32 host에서 원본 `ez2dj1.exe`를 `DEBUG_ONLY_THIS_PROCESS`로 만들고 entry `0x0043a640` 직전에 멈춘다. 이 입력에서는 DR0 hardware stop이 전달되지 않아 child memory의 entry 첫 바이트를 일시적으로 `INT3`로 바꾸고 즉시 원복하는 diagnostic fallback으로 정지했다. 이때 Windows loader가 main image를 `0x00400000`에 배치하고 7 DLL·144 IAT slot을 해석했음을 실제 HDD로 확인했다. 정지 상태에서 primary thread를 suspend하고 minimal x86 runtime DLL을 remote `LoadLibraryW` thread로 적재해 module base도 확인했다. `--probe-handoff`는 PE table에서 `GetCommandLineA` IAT slot을 찾아 runtime log-and-forward thunk로 교체하고, `--hle-command-line`은 runtime의 process-lifetime buffer에 original basename을 기록해 실제 HLE thunk가 반환하도록 한다. 두 경로 모두 entry 재개 후 debugger output event로 확인했다. x64 `re2dj_windows_original_process_probe`는 보류된 비교 근거로 남긴다.
* `re2dj_windows_x86_launcher_probe`는 target과 원본 EXE를 해석한 뒤 실행별 JSONL 진단 로그를 `logs/windows_x86_launcher_probe/<target-id>/`에 만든다. debug event와 예외 관찰은 `--trace` 여부와 관계없이 즉시 flush되며, `--trace`는 stderr 실시간 표시만 제어한다. 최종 성공 또는 실패 JSON은 생성된 로그 경로를 포함한다. 이 생성 디렉터리는 HDD 및 guest overlay 밖이고 Git ignore 대상이다.
* `re2dj_windows_x86_launcher_probe`의 `--instruction-trace <max-steps>`는 software entry stop에서 EIP와 TF를 설정하고 primary-thread debugger event 뒤 TF를 다시 설정한다. 최대 32개 instruction address와 바이트를 ring buffer에 유지한 뒤 illegal instruction 또는 step limit에서만 JSONL에 기록한다. 이는 protected post-entry control flow의 관찰 도구이며, branch operand나 보호 실패 원인을 자체적으로 해석하지 않는다.
* `re2dj_windows_x86_launcher_probe --scan-fault-references`는 first-chance illegal-instruction event에서 child를 계속하기 전에 committed private/image memory를 bounded scan한다. fault address와 page base의 32-bit reference, 해당 region 속성 및 match summary를 JSONL에 기록한다. 이 결과는 target storage 후보일 뿐 indirect branch caller의 증명은 아니다.
* `re2dj_windows_x86_launcher_probe --api-trace`는 child memory에서 `kernel32.dll`/`kernelbase.dll` export를 해석해 watched API에 software breakpoint를 설치하고, hit마다 caller·args·ANSI 문자열을 JSONL에 남긴 뒤 원래 byte 복원과 TF 1 step으로 삼키고 재무장한다. `DeviceIoControl`은 guest-origin 호출에 한해 8개 인자와 bounded input/output snapshot을 기록하고 thread별 one-shot return breakpoint에서 EAX, bytes-returned, buffer 변화 여부를 기록한다. first-chance illegal-instruction에서는 full register와 segment, 64 dword stack과 main image section 분류, fault page dump, allocation region walk, entry VA 참조 탐색을 기록한다. first-chance access violation에서는 접근 종류·주소와 full register, main-image register pointer 주변 dword, stack return 주소 주변 code bytes를 기록한다. entry 이후 동적으로 적재된 모듈의 unload event에서는 종반 구간을 single-step 수집하며 매 샘플에 GP register trail과 nearest-export 심볼 주석을 붙인다. 수집 중 표준 syscall 스탠자(`mov edx,&thunk; call edx`) 꼬리마다 복귀 주소에 1회용 software breakpoint를 심어(예산 상한 내 재무장) WOW64 게이트 너머의 32비트 복귀를 포착하고 TF를 재무장하며, 복귀 시점 stack 64 word에 심볼 주석을 남긴다. 이 도구는 관찰이며 보호 실패 원인을 자체적으로 판정하지 않는다.
* OS type을 포함하지 않는 desktop helper protocol v3 header는 `src/platform/native_helper_protocol.h`에 공유한다. Linux x86-64 `NativeHelperBackend`와 i386 helper는 requested-base PE32 mapping, `HIGHLOW` relocation, TLS process-attach callback, named/ordinal import thunk와 event/memory/completion packet을 합성 image로 검증했고 제품 CLI에 연결했다. `native_process_bootstrap`은 guard page가 있는 1 MiB guest stack, 최소 x86 TEB/PEB와 FS descriptor를 만들며 TLS callback과 entry를 이 환경에서 실행한다. alternate signal stack의 handler는 guest `SIGSEGV`·`SIGBUS`·`SIGILL`·`SIGFPE`·`SIGTRAP` EIP/ESP를 저장한 뒤 control stack에서 `kFault` event를 보낸다. 현재 protocol은 단일 pending event와 4 KiB import-stack window만 지원하며 Win32 HLE dispatcher는 후속 단계다.
* Linux 제품 실행 경로는 i386 helper가 PE32 mapping, guest ABI process/thread state와 native x86 실행을 소유하고, x86-64 host가 공용 Win32 HLE dispatcher, HDD/VFS/overlay와 SDL graphics/audio/input service를 소유하는 구조로 확장한다. guest address는 `ExecutionBackend` memory API로만 전달한다. USER32와 DirectX guest object는 32비트 token/vtable로 표현하고 host-side state에 연결한다. unprotected `ez2dj1.exe`로 entry·import·callback·thread·DirectX 경계를 먼저 완성한 뒤 protected `ez2dj.exe`의 self-modifying/LPTDI/raw-I/O 경계를 추가한다. 상세 단계는 [Linux 원본 실행 경로 설계](docs/design/20260827-077-linux-original-execution.md)에 둔다.
* BSD-2-Clause v86 CPU 분리성 spike는 부적합으로 끝났다. 공식 소스에는 CPU-only build 경계가 없고 CPU memory·run loop가 PC 장치, MMIO, browser timer/IRQ에 결합되어 있다. 기본 synthetic gate `0xF0000000`도 v86에서 실행 불가능한 mapped/MMIO 범위다. interpreter와 JIT 양쪽에 gate stop/resume을 새로 넣고 대규모 fork를 유지해야 하므로 채택하지 않는다. TinyEMU 계열은 Web x86 소스 공개 범위를 확인할 때만 재검토하며, 직접 인터프리터는 계속 후순위 fallback이다.

*`GuestAddress` is a 32-bit value type that cannot become a host pointer. `AddressSpace` keeps only committed pages of the flat 4 GiB guest space in host memory and exposes accessors rather than pointers. `LoadPe32Image()` maps and zero-fills headers and sections, applies `IMAGE_REL_BASED_HIGHLOW` relocations, and binds named or ordinal imports to synthetic gates transactionally. `ImportGateTable` assigns addresses at 16-byte intervals from `0xF0000000`. The implemented `ExecutionBackend` interface separates image preparation, start, event waiting, guest-memory reads/writes, import completion, and stop requests using backend-local thread IDs and guest values only. Windows `NativeHelperBackend` encapsulates the x86 helper process and anonymous-pipe protocol v3 behind that interface; `native_pe_image` owns requested-base mapping, relocations, protection, and process-attach TLS callbacks, while import thunks restore EDX:EAX and dynamic stack cleanup. The primary `re2dj_windows_x86_launcher_probe` creates the original as a loader-owned child main image, confirms a pre-entry fully resolved IAT stop through a temporary child-memory `INT3` fallback, and loads a minimal same-bitness runtime DLL while the primary thread remains suspended; x64 observation remains deferred evidence. The OS-independent protocol-v3 header is shared under `src/platform/`. The Linux x86-64 backend and production i386 helper validate requested-base PE32 mapping, relocation, process-attach TLS, named and ordinal native import thunks, and event/memory/completion IPC and are connected to the product CLI. Linux guest TLS callbacks and entry run with a guarded 1 MiB stack, minimal x86 TEB/PEB, and FS selector. Fault handlers capture guest EIP/ESP on an alternate signal stack and send a structured event from the control stack. The x86-64 host owns shared Win32 HLE, HDD/VFS/overlay, and SDL services. Guest USER32 and DirectX objects use 32-bit tokens and vtables backed by host state. The unprotected build brings up imports, callbacks, threads, and DirectX before the protected executable adds self-modification, LPTDI, and raw I/O. Protocol v3 still serializes one event; TLS raw storage/index and parallel guest threads remain later extensions. See the [Linux original-executable design](docs/design/20260827-077-linux-original-execution.md) and [guest process bootstrap design](docs/design/20260827-078-linux-guest-process-bootstrap.md). The v86 separability spike rejected adoption: its published source lacks a CPU-only build boundary, couples CPU memory/run control to PC devices and browser services, and treats the default synthetic-gate range as non-executable MMIO. TinyEMU remains conditional on confirming published Web-x86 source scope, while a custom interpreter remains deferred.*

*After resolving a target and original executable, `re2dj_windows_x86_launcher_probe` creates a per-run JSONL diagnostic log under `logs/windows_x86_launcher_probe/<target-id>/`. Debug events and exception observations flush to that file whether or not `--trace` is set; `--trace` controls only live stderr output. Final success and error JSON identify the log path. The generated directory is outside the HDD and guest overlay and is Git-ignored.*

Win32 `--audio-volume-trace`는 injected runtime의 별도 bounded writer를 사용한다. DirectSound COM facade는 buffer 형식, `SetVolume`, 최초 재생과 재생 중 `Unlock`의 PCM 통계만 기록하고, WINMM import thunk는 호스트 mixer API로 pass-through하면서 control 구조와 scalar 값을 기록한다. 원본 샘플 바이트와 HDD 자산은 로그에 포함하지 않는다.

DirectSound secondary voice는 descriptor에 따라 정적 `MIX_Audio` snapshot과 streaming `SDL_AudioStream`으로 분리된다. 현재 확인된 `DSBCAPS_LOCHARDWARE` streaming buffer는 Play 시 current cursor부터 ring 한 바퀴를 큐잉하고 committed PCM snapshot을 남긴다. 원본이 `DSBLOCK_ENTIREBUFFER`로 전체를 Lock하더라도 Unlock 뒤 frame별 변경을 비교해 가장 큰 unchanged circular gap을 제외한 최소 dirty 구간만 추가한다. SDL stream이 PCM을 복사하므로 guest write와 mixer read가 같은 storage를 동시에 사용하지 않는다. voice별 cursor, control, Stop/restart와 duplicate의 독립 재생 상태는 기존 neutral buffer 계약을 유지한다.

Win32 제품의 `src/platform/windows/ini_profile_hle.*`는 원본 main image의 `GetPrivateProfileIntA` import thunk 경계에서 `GAMEASSIGNMENTS/DemoVolume`만 외부 `--demo-volume 0..3` 값으로 재정의한다. 다른 section/key는 호스트 API로 그대로 전달하며 원본 EXE와 HDD INI는 수정하지 않는다. 확인된 원본 profile table은 `[-10000, -2222, -1111, 0]`이고 제품 기본은 profile 3, SDL master gain 기본은 0 dB다. 상세 계약은 [DirectSound 데모 음량 설정 HLE 설계](docs/design/20260829-086-directsound-volume-transition.md)에 둔다.

*Win32 `--audio-volume-trace` uses a separate bounded writer in the injected runtime. The DirectSound COM facade records only buffer formats, `SetVolume`, and PCM statistics at first playback and streaming `Unlock`; WINMM import thunks pass through to host mixer APIs while recording control structures and scalar values. Original sample bytes and HDD assets are never included in the log.*

*DirectSound secondary voices select either a static `MIX_Audio` snapshot or a streaming `SDL_AudioStream` from the observed descriptor. A confirmed `DSBCAPS_LOCHARDWARE` stream queues one ring revolution from the current cursor at Play and retains a committed PCM snapshot. Even when the original locks the complete buffer with `DSBLOCK_ENTIREBUFFER`, frame comparison after Unlock appends only the smallest dirty circular interval outside the largest unchanged gap. The SDL stream copies PCM, so guest writes and mixer reads do not concurrently use the same storage. Per-voice cursors, controls, Stop/restart, and independent duplicate playback state retain the neutral-buffer contract.*

*The Win32 product's `src/platform/windows/ini_profile_hle.*` overrides only `GAMEASSIGNMENTS/DemoVolume` at the original main image's `GetPrivateProfileIntA` import-thunk boundary with external `--demo-volume 0..3`. Other section/key requests pass through to the host API, and neither the original EXE nor HDD INI is modified. The confirmed original profile table is `[-10000, -2222, -1111, 0]`; product defaults are profile 3 and 0 dB SDL master gain. See the [DirectSound demo-volume HLE design](docs/design/20260829-086-directsound-volume-transition.md).*

*`re2dj_windows_x86_launcher_probe --instruction-trace <max-steps>` sets EIP and TF at the software-entry stop and rearms TF after primary-thread debugger events. It retains up to 32 instruction addresses and bytes in a ring buffer, writing them to JSONL only on an illegal instruction or step limit. This observes protected post-entry control flow; it does not independently decode branch operands or determine a protection-failure cause.*

*`re2dj_windows_x86_launcher_probe --scan-fault-references` bounded-scans committed private/image memory before continuing a first-chance illegal-instruction event. It records 32-bit references to the fault address and page base, their region properties, and a match summary in JSONL. The result is a target-storage candidate only, not proof of an indirect-branch caller.*

*`re2dj_windows_x86_launcher_probe --api-trace` resolves `kernel32.dll`/`kernelbase.dll` exports from child memory, arms software breakpoints on watched APIs, records caller, arguments, and ANSI strings per hit to JSONL, then swallows and rearms each hit by restoring the original byte and trap-flagging one instruction. For guest-origin DeviceIoControl calls it captures all eight arguments and bounded input/output snapshots, then uses a per-thread one-shot return breakpoint to record EAX, bytes-returned, and buffer changes. On a first-chance illegal instruction it records full registers with segments, a 64-dword stack with main-image section classification, the fault page bytes, an allocation region walk, and an entry-VA reference scan. On a first-chance access violation it records access kind/address, full registers, bounded dword windows around main-image register pointers, and code bytes around stack return addresses. On an unload event of a module loaded after entry it single-steps the unload tail, attaching a GP-register trail and nearest-export symbol annotation to every sample. Each standard syscall stanza tail (`mov edx,&thunk; call edx`) plants a one-shot software breakpoint on its return address — rearmable within a fire budget — catching the 32-bit resume past WOW64 gates and re-arming tracing, with a symbol-annotated 64-word stack dump at each resume. The tool observes only and does not itself adjudicate the protection-failure cause.*

---

## 8. 계획된 HLE 계층 / Planned HLE layer **[계획]**

이 표는 추측이 아니라 `ez2dj1.exe`의 import 테이블에서 나왔다. 근거와 전체 목록은 [EZ2DJ import 표면](docs/analysis/ez2dj-import-surface.md)에 있다.

*This table comes from the import table of `ez2dj1.exe` rather than from guesswork. The evidence and full list are in [EZ2DJ Import Surface](docs/analysis/ez2dj-import-surface.md).*

| 모듈 | 대체 대상 | 함수 수 | 우선순위 |
| --- | --- | --- | --- |
| `kernel32` | 파일 I/O, 메모리, INI, 모듈 | 약 40 | 1 |
| `user32` | 창, 메시지 루프, 디스플레이 모드 | 약 12 | 1 |
| `ddraw` | DirectDraw 1~6 표면과 블릿 | COM | 2 |
| `gdi32` | DIB 섹션과 블릿 | 14 | 2 |
| `dsound` | ordinal `#1` = `DirectSoundCreate` | COM | 3 |
| `winmm` | 믹서 볼륨, `timeGetTime` | 8 | 3 |
| `user32` | `GetAsyncKeyState` — 입력 전부 | 1 | 4 |
| `advapi32` | `RegFlushKey` stub | 1 | 4 |
| `kernel32` | 스레드, 이벤트, 임계 구역 | 약 20 | 5 |

`ez2dj1.exe`의 import는 **7개 DLL, 144개 함수**가 전부다. **Direct3D도 DirectInput도 없다.** 3rd는 DirectInput과 AVI 재생을 추가로 쓰므로 버전별 HLE 프로파일이 필요하다.

*`ez2dj1.exe` imports **144 functions from 7 DLLs** in total, with **no Direct3D and no DirectInput**. The 3rd build adds DirectInput and AVI playback, which is why per-version HLE profiles are needed.*

가장 큰 작업은 함수 개수가 아니라 DirectDraw와 DirectSound의 **COM 인터페이스**다. 게스트 메모리 안에 vtable을 만들어 각 슬롯에 gate 주소를 채워야 한다.

*The largest piece is not the function count but the **COM interfaces** of DirectDraw and DirectSound, which need vtables built inside guest memory with a gate address in each slot.*

각 모듈은 `{이름, ordinal, 인자 개수, 호출 규약, 구현 함수}` 항목의 테이블로 표현한다. 로더는 import 이름을 이 테이블에서 찾아 gate 주소를 배정한다. 구현되지 않은 항목은 gate에 남되 호출되면 이름과 함께 실패를 기록한다. 이렇게 하면 **실제로 필요한 API만** 점진적으로 구현할 수 있다.

*Each module is a table of `{name, ordinal, argument count, calling convention, implementation}` entries. The loader looks up an import name and assigns a gate address. Unimplemented entries still get a gate, and a call logs the name and fails, so only APIs the game actually calls need implementing.*

---

## 9. rePIU와의 구조적 차이 / Structural differences from rePIU

| 항목 | rePIU | re2DJ |
| --- | --- | --- |
| 게스트 실행 형식 | DOS/4GW LE | Win32 PE32 |
| 환경 경계 | DOS/DPMI interrupt, port I/O | Win32 import thunk |
| 실행 방식 | 32비트 Win32 호스트에서 네이티브 실행 + VEH 트랩 | 교체 가능한 backend: 데스크톱 native helper 우선, Web 실행 엔진 별도 |
| 호스트 | Win32 x86 전용 | Windows x64 / Linux x64 / Web |
| 그래픽 경계 | Glide (`glide2x`) | DirectDraw / Direct3D |
| 자산 입력 | MAME ROM ZIP + CHD | HDD 디렉터리 경로 |
| 플랫폼 디렉터리 | `src/platform/win32/` | `src/platform/{windows,linux,web}/` |

공통으로 유지하는 것: 설계 우선 워크플로, 한국어 우선 이중 언어 문서, 영어 전용 소스 주석, `VERSION` 기반 버전 관리, BSD 3-Clause 라이선스 정책, 원본 자산 비포함 원칙.

*Shared with rePIU: design-first workflow, Korean-first bilingual documents, English-only source comments, `VERSION`-based versioning, the BSD 3-Clause license policy, and the rule that original assets never enter the repository.*

---

## 10. 빌드 구성 / Build configuration **[구현됨]**

`CMakeLists.txt`는 플랫폼 중립 legacy graphics, 공용 코어, SDL3/OpenGL backend와 host·분석·검증 실행 파일을 만든다. Windows 제품 build는 Win32 runtime만 구성하며 64비트 Windows host에서는 WOW64로 실행한다. 별도 Windows x64 preset·CI target은 제거했다. Linux와 Web 기본 구성은 같은 SDL3/OpenGL backend source를 항상 컴파일하고, Linux i386 helper 전용 구성만 SDL3를 제외한다.

| 타깃 | 내용 |
| --- | --- |
| `re2dj_legacy_graphics` | draw command, texture와 vertex-buffer 공용 정적 라이브러리 |
| `re2dj_core` | 공용 코어 정적 라이브러리 |
| `re2dj_windows_original_process_backend` | Win32 제품 CLI와 진단 launcher가 공유하는 원본-process 실행 engine |
| `re2dj_sdl3_opengl_backend` | Win32·Linux·Web 공용 SDL3/OpenGL 렌더 backend |
| `re2dj` | 명령행 호스트 |
| `re2dj_hdd_probe` | HDD 디렉터리 스캔 도구 |
| `re2dj_pe_analyzer` | PE32 헤더 분석 도구 |
| `re2dj_pe_loader` | PE32 매핑·재배치·import gate 보고 도구 |
| `re2dj_unit_tests` | CTest에 등록된 단위 테스트 |
| `re2dj_native_helper_probe` | Win32 x86 / WOW64 네이티브 gate 호출 probe, 선택 target |
| `re2dj_native_ipc_helper` | Win32 x86 mapper·gate·IPC helper, 선택 target |
| `re2dj_windows_x86_launcher_probe` | Win32 x86 원본 EXE entry·IAT 정지점 검증 도구 |
| `re2dj_windows_product_loader_probe` | Win32 canonical 제품 policy와 미지원 target asset-free 검증 도구 |
| `re2dj_linux_native_ipc_helper` | Linux i386 PE32 mapper·native import gate production helper, 선택 target |
| `re2dj_linux_native_ipc_host_probe` | Linux x86-64/i386 helper synthetic integration 검증 도구 |

외부 의존성은 graphics build에서 FetchContent로 고정하는 zlib 라이선스 SDL 3.4.14와 Windows x86 audio build의 SDL_mixer 3.2.4다. SDL3 video/OpenGL은 Win32·Linux·Web 공용 backend를 제공한다. 추가 mixer codec dependency는 활성화하지 않는다.

*The graphics build fetches pinned zlib-licensed SDL 3.4.14 for the shared Win32/Linux/Web video and OpenGL backend. The Windows x86 audio build additionally fetches SDL_mixer 3.2.4. No optional mixer codec dependencies are enabled. Windows product builds target Win32 and run on 64-bit Windows through WOW64; separate Windows x64 presets and CI targets are removed. Linux uses separate x86-64 product/host-probe and i386 helper build presets.*
