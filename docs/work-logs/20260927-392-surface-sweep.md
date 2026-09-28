# 작업 392 작업 로그 — 표면 점검: EnumSurfaces와 RestoreAllSurfaces / Task 392 work log — the surface sweep: EnumSurfaces and RestoreAllSurfaces

설계: [20260927-392-surface-sweep.md](../design/20260927-392-surface-sweep.md)
작업 지시서: [20260927-392-surface-sweep.md](../work-orders/20260927-392-surface-sweep.md)

## 진행 / Progress

측정은 MSVC x86으로 빌드한 작은 프로그램(`ddraw.lib`, `DDSCL_NORMAL`)으로 했다. 전체 화면 전용 모드는 사용자 화면의 해상도를 바꾸므로 쓰지 않았다. 그래서 flip chain의 뒤 버퍼 대신 mipmap 하위 단계로 "붙은 표면"을 확인했다. 결과는 설계의 측정 절에 적었다.

*The measurements used a small program built with MSVC x86 (`ddraw.lib`, `DDSCL_NORMAL`). Exclusive full-screen mode would change the user's display resolution, so it was not used; attached surfaces were checked with a mipmap sublevel instead of a flip chain's back buffer. The results are in the design's measurements section.*

Linux에 `EnumSurfaces`를 더하자 4th는 표면 4개를 받았다. 순서는 텍스처, depth, 뒤 버퍼, 주 표면이다. 콜백은 각각 `DDENUMRET_OK`를 돌려주고, API를 하나도 부르지 않았다. 게임은 이어서 다음을 불렀다.

1. `SetTexture(0..3, NULL)`
2. `RestoreAllSurfaces`
3. 텍스처 하나의 `IsLost`와 `Release` 두 번
4. hardlock 요청 하나

그 뒤 `kernel32!GetCurrentDirectoryA`에서 멈췄다.

*With `EnumSurfaces` on Linux the 4th received four surfaces: the texture, the depth surface, the back buffer, then the primary. Its callback returned `DDENUMRET_OK` for each and called no API. The game went on to call the following, then stopped at `kernel32!GetCurrentDirectoryA`:*

1. *`SetTexture(0..3, NULL)`*
2. *`RestoreAllSurfaces`*
3. *`IsLost` and two `Release`s on one texture*
4. *one hardlock request*

`SetTexture(1..3, NULL)`은 `DDERR_UNSUPPORTED`를 받는다. stage 0만 다루는 Windows facade와 같은 답이며, 게임은 결과를 보지 않고 넘어간다. 실제 Direct3D 7이 이 호출에 무엇을 답하는지는 측정하지 않았다.

*`SetTexture(1..3, NULL)` gets `DDERR_UNSUPPORTED`, the same answer as the Windows facade, which handles stage 0 only; the game moves on without looking at it. What real Direct3D 7 answers there was not measured.*

## 변경 / Changes

- **core**:
  - `DDENUMSURFACES_*` 값(Windows에서 SDK와 static_assert).
  - `PlanEnumSurfaces`와 `EnumSurfacesPlan`.

  ***core:***
  - *The `DDENUMSURFACES_*` values (static_assert against the SDK on Windows).*
  - *`PlanEnumSurfaces` and `EnumSurfacesPlan`.*
- **Windows facade**: `Dd7EnumSurfaces`가 core로 플래그를 판정한다. 유효한 열거는 여전히 아무것도 나열하지 않는다. / ***Windows facade:** `Dd7EnumSurfaces` checks its flags through the core; a valid sweep still lists nothing.*
- **HLE**: `GuestComObjects::Addresses()`. / ***HLE:** `GuestComObjects::Addresses()`.*
- **Linux ddraw.dll**:
  - `ExistingSurfaces`(최신 것부터), `DescribeSurface`.
  - `IDirectDraw7::EnumSurfaces`, `RestoreAllSurfaces`.

  ***Linux ddraw.dll:***
  - *`ExistingSurfaces` (newest first) and `DescribeSurface`.*
  - *`IDirectDraw7::EnumSurfaces` and `RestoreAllSurfaces`.*
- **단위 테스트**:
  - core: 측정한 flags 조합과 모델 밖 검색.
  - `ddraw.dll`: 순서(텍스처 → 뒤 버퍼 → 주 표면), 설명(size, width, pitch), context, 참조 증가, CANCEL, 거절 8가지, 모델 밖 검색의 정지, 해제된 표면 제외, `RestoreAllSurfaces`.

  ***Unit tests:***
  - *core: the measured flag combinations and the unmodelled searches.*
  - *`ddraw.dll`: the order (texture → back buffer → primary), the description (size, width, pitch), the context, the raised references, CANCEL, eight refusals, a stop on an unmodelled search, released surfaces left out, and `RestoreAllSurfaces`.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Windows x86 build, CTest | exit 0, 6/6 |
| Windows 실제 4th, 기준 `daf74ef`와 30초씩 / real 4th vs base `daf74ef`, 30 s each | 정규화한 ddraw 기록 1,410줄이 같다 / *the normalized ddraw logs match, 1,410 lines* |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3(unit checks 3,875) / *no warnings or errors, 3/3 each (3,875 unit checks)* |
| 실제 4th, Linux 두 폭 / real 4th, both Linux widths | 호출 22,723번, hardlock 82, IO 읽기 2,949·쓰기 1,964. 주소와 시계 값을 정규화하면 두 폭이 같다. `#22723 kernel32!GetCurrentDirectoryA`에서 멈춘다. / *22,723 calls, hardlock 82, IO reads 2,949 / writes 1,964; identical on both widths after address and clock normalization. The run stops at `#22723 kernel32!GetCurrentDirectoryA`.* |

## 다음 / Next

`kernel32!GetCurrentDirectoryA`와 그 뒤의 경계다.

*Next is `kernel32!GetCurrentDirectoryA` and the boundaries after it.*
