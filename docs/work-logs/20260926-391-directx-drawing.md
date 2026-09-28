# 작업 391 작업 로그 — DirectX 5단계: 그리기와 Linux 창 표시 / Task 391 work log — DirectX phase 5: drawing and the Linux window

설계: [20260926-391-directx-drawing.md](../design/20260926-391-directx-drawing.md)
작업 지시서: [20260926-391-directx-drawing.md](../work-orders/20260926-391-directx-drawing.md)

## 진행 / Progress

Windows facade의 그리기 규칙을 core로 옮긴 뒤 Windows build가 그대로 통과했다. Linux에 `Clear`, `SetTexture`, `DrawPrimitive`, `Flip`을 더하자 4th는 약 490프레임을 그렸다. 프레임 구성은 다음과 같다.

| 프레임 수 / Frames | 내용 / Contents |
| --- | --- |
| 300 | clear + draw 1개(WARNING 화면) / *clear + one draw (the WARNING screen)* |
| 129 | clear + draw 2개 / *clear + two draws* |
| 61 | clear만 / *clear only* |

그 뒤 `IDirectDraw7::EnumSurfaces`(flags `0x11`)에서 멈췄다.

*Once the Windows facade's drawing rules moved to the core, the Windows build passed unchanged. With `Clear`, `SetTexture`, `DrawPrimitive`, and `Flip` on Linux, the 4th drew about 490 frames (above), then stopped at `IDirectDraw7::EnumSurfaces` (flags `0x11`).*

### 보이지 않던 그리기 / Draws that seemed invisible

처음에는 draw가 모두 `D3D_OK`인데 `--hold-window` 창이 clear 색만 보였다. 그래서 다음을 차례로 확인했다.

- **backend 단독 재현.** 같은 vertex, 상태, 1024×512 텍스처와 color key로 backend만 따로 실행하면 정상으로 그려졌다.
- **renderer.** 두 경우 모두 llvmpipe(LLVM 20.1.2, Mesa 25.2.8)였다.
- **부동소수점 상태.** x87 control word는 `0x027f`, MXCSR은 `0x1fa0`이었다. FPU 환경을 초기화해도 같았다.
- **스레드.** llvmpipe worker 12개는 정상적으로 futex에서 대기했고 신호 차단도 정상이었다.
- **되읽기.** re2dj 안에서 `Draw` 직후 `glReadPixels`로 읽으면 pixel이 칠해져 있었다.

결론적으로 그리기는 처음부터 정상이었다. 실행이 `EnumSurfaces`에서 멈춘 뒤 창에 남는 것은 마지막 프레임이다. 그 프레임은 원래 clear만 하는 61프레임 중 하나였다. `LP_NUM_THREADS=0`에서 화면이 보인 것은 렌더링이 느려져 WARNING 구간에 캡처했기 때문이다. 실행 도중에 캡처하면 두 폭 모두 WARNING 화면이 Windows와 같은 배치로 보인다. 원본 화면 캡처는 저장소 밖(scratchpad)에만 두었다.

*At first every draw returned `D3D_OK` but the `--hold-window` window showed only the clear color, so the following were checked in turn:*

- ***A standalone backend repro.** The same vertices, state, and a 1024×512 texture with a color key drew correctly.*
- ***The renderer.** Both cases used llvmpipe (LLVM 20.1.2, Mesa 25.2.8).*
- ***Floating-point state.** The x87 control word was `0x027f` and MXCSR `0x1fa0`; resetting the FPU environment changed nothing.*
- ***Threads.** The 12 llvmpipe workers waited normally on futexes, with normal signal masks.*
- ***Read-back.** A `glReadPixels` straight after `Draw` inside re2dj found the pixels drawn.*

*So drawing had worked all along. Once the run stops at `EnumSurfaces`, the window keeps the last frame, which was one of the 61 clear-only frames. The screen seemed to appear under `LP_NUM_THREADS=0` only because slower rendering put the capture inside the WARNING frames. Captured mid-run, both widths show the WARNING screen laid out as on Windows. Captures of the original screen were kept outside the repository (scratchpad) only.*

### API 기록 크기 / API log size

clear의 채우기가 행마다(1,280바이트씩 480번) guest에 쓰면서 쓰기 기록이 프레임마다 480줄 남았다. 그래서 실행 한 번의 API 기록이 51.8MB였다. 행이 연속이므로 한 번에 쓰도록 바꾸자 5.1MB가 되었다.

*The clear's fill wrote the guest surface row by row (480 writes of 1,280 bytes), leaving 480 write records per frame, so one run's API log reached 51.8 MB. The rows are contiguous, so the fill now writes once, and the log is 5.1 MB.*

## 변경 / Changes

- **core**:
  - `re2dj/directx/direct3d_draw.h`(.cpp): `PlanDrawPrimitive`, `BuildFixedFunctionState`, `BuildTransformState`, `ApplyFadeCompatibility`, `Rgb565FromD3dColor`.
  - `abi.h`: render state index, `D3DCMP`, cull, texture address, filter, primitive type, FVF, `D3DCLEAR`, `DDERR_NOTFLIPPABLE`. Windows에서 SDK 값과 static_assert로 맞춘다.
  - `re2dj_directx`가 `re2dj_legacy_graphics`를 link한다.

  ***core:***
  - *`re2dj/directx/direct3d_draw.h` (.cpp): `PlanDrawPrimitive`, `BuildFixedFunctionState`, `BuildTransformState`, `ApplyFadeCompatibility`, `Rgb565FromD3dColor`.*
  - *`abi.h`: render state indices, `D3DCMP`, cull, texture address, filter, primitive types, FVFs, `D3DCLEAR`, and `DDERR_NOTFLIPPABLE`, checked against the SDK by static_assert on Windows.*
  - *`re2dj_directx` links `re2dj_legacy_graphics`.*
- **Windows facade**: 고정 기능 상태, fade, draw 계획, clear 색, DX7 변환이 core를 부른다. / ***Windows facade:** the fixed-function state, fade, draw plan, clear color, and DX7 transform call the core.*
- **backend**: `Sdl3OpenGlBackend::SetRetainBetweenFrames`. / ***Backend:** `Sdl3OpenGlBackend::SetRetainBetweenFrames`.*
- **host 표시 / host presentation**: `HostPresentation`의 그리기 계약과 Linux 구현. / *The drawing contract on `HostPresentation`, and its Linux implementation.*
- **Linux ddraw.dll**:
  - 표면: identity·revision·host 복사본, `SurfaceTextureView`, `FillSurface`(한 번의 쓰기), `Flip`, 해제 때 `DiscardTexture`.
  - 장치: `Clear`, `DrawPrimitive`, `SetTexture`(참조 보유).
  - 표시되는 표면(`presentation_surface`), 프레임 보존 알림.

  ***Linux ddraw.dll:***
  - *Surfaces: identity, revision, and the host copy; `SurfaceTextureView`, `FillSurface` (one write), `Flip`, and `DiscardTexture` on release.*
  - *The device: `Clear`, `DrawPrimitive`, and `SetTexture` (holding a reference).*
  - *The presented surface (`presentation_surface`), and telling the host to retain frames.*
- **단위 테스트**:
  - core: draw 계획, 고정 기능 상태와 거절, 변환, fade, 5-6-5.
  - `ddraw.dll`: 부분·전체 clear와 채워진 pixel, `SetTexture` 규칙과 참조, textured draw, 계획 밖 draw, `Flip`/`DDERR_NOTFLIPPABLE`, 해제 때 텍스처 폐기.

  ***Unit tests:***
  - *core: the draw plan, the fixed-function state and its refusals, the transform, the fade, and 5-6-5.*
  - *`ddraw.dll`: partial and whole clears with the filled pixels, `SetTexture`'s rules and reference, a textured draw, draws outside the plan, `Flip`/`DDERR_NOTFLIPPABLE`, and discarding the texture on release.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Windows x86 build, CTest | exit 0, 6/6 |
| Windows 실제 4th, 기준 `61a33ba`와 30초씩 / real 4th vs base `61a33ba`, 30 s each | 정규화한 ddraw 기록 1,410줄이 같다 / *the normalized ddraw logs match, 1,410 lines* |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3(unit checks 3,814) / *no warnings or errors, 3/3 each (3,814 unit checks)* |
| 실제 4th, Linux 두 폭 / real 4th, both Linux widths | 호출 22,672번, `#22672 IDirectDraw7::EnumSurfaces`에서 정지. hardlock 81, IO 읽기 2,943·쓰기 1,960으로 두 폭이 같다. 이제 프레임 속도가 실제 시계를 따르므로 마지막 256줄은 `timeGetTime` 폴링 한 번만큼 어긋날 수 있다. / *22,672 calls, stopping at `#22672 IDirectDraw7::EnumSurfaces`; hardlock 81 and IO reads 2,943 / writes 1,960 on both widths. Frame pacing now follows the real clock, so the last 256 lines can be one `timeGetTime` poll apart.* |
| Linux 창 / Linux window | 두 폭 모두 실행 중 WARNING 화면이 보인다 / *both widths show the WARNING screen mid-run* |

## 다음 / Next

`IDirectDraw7::EnumSurfaces`와 그 뒤의 경계다. 이어서 Linux host 입력(키보드·마우스 → `InputSnapshot`, IO 보드, 커서)과 소리 출력을 연결한다.

*Next is `IDirectDraw7::EnumSurfaces` and the boundaries after it, then connecting Linux host input (keyboard and mouse → `InputSnapshot`, the IO board, the cursor) and sound output.*
