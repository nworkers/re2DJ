# 작업 412 설계 — DirectDrawEnumerateA / Task 412 design — DirectDrawEnumerateA

선행: [작업 411 설계](20260927-411-private-profile-section-names.md)

## 배경 / Background

작업 411 뒤 Linux의 EZ2DJ 1st는 설정을 다 읽은 다음 `ddraw!DirectDrawEnumerateA`에서 멈췄다. 1st는 4th의 DirectX 7과 달리 DirectDraw 1(`DirectDrawCreate`)로 시작하는 DirectX 6 경로를 쓴다. Windows 제품은 `DirectDrawCreate`를 DX6 facade(`direct3d3_com_facade.cpp`)로 가로채고, `DirectDrawEnumerateA`는 진짜 DirectDraw가 답하게 둔다.

*After Task 411, EZ2DJ 1st on Linux read all its settings and stopped at `ddraw!DirectDrawEnumerateA`. Unlike 4th's DirectX 7, 1st takes a DirectX 6 path that starts from DirectDraw 1 (`DirectDrawCreate`); the Windows product routes `DirectDrawCreate` to its DX6 facade (`direct3d3_com_facade.cpp`) and leaves `DirectDrawEnumerateA` to the real DirectDraw.*

### 측정 / Measurements

Windows 11(32비트, 모니터 하나)에서 측정했다.

*Measured on Windows 11 (32-bit, one monitor).*

- 콜백은 한 번만 불린다: GUID NULL, 설명은 CP949 "주 디스플레이 드라이버"(`DirectDrawEnumerateExA`와 같음), 이름 "display". 인자는 4개다. / *The callback is called once: a null GUID, the CP949 "주 디스플레이 드라이버" description (as `DirectDrawEnumerateExA`), the name "display"; four arguments.*
- 콜백이 FALSE를 돌려줘도 결과는 `DD_OK`다. NULL 콜백은 `0x80070057`(`DDERR_INVALIDPARAMS`)이다. / *`DD_OK` whatever the callback answers; a null callback is `0x80070057` (`DDERR_INVALIDPARAMS`).*

## 결정 / Decisions

`DirectDrawEnumerateExA`의 콜백 루프를 `EnumerateDisplays`로 떼어 두 함수가 함께 쓴다. `DirectDrawEnumerateA`는 주 드라이버 하나를 4개 인자로 넘긴다.

*`DirectDrawEnumerateExA`'s callback loop moves into `EnumerateDisplays`, shared by both; `DirectDrawEnumerateA` passes the primary driver alone with four arguments.*

## 다음 / Next

Windows 실행 기록에서 1st의 DX6 facade 호출은 다음 순서다.

*Per the Windows run log, 1st's DX6 facade calls go:*

1. `DirectDrawCreate`
2. `QueryInterface`로 `IDirectDraw4` / *`QueryInterface` to `IDirectDraw4`*
3. `SetCooperativeLevel`, `SetDisplayMode`
4. `CreateSurface`와 텍스처 표면 175개 / *`CreateSurface` and 175 texture surfaces*
5. `IDirect3D3::FindDevice`/`CreateDevice`
6. `IDirect3DDevice3::DrawPrimitive`

Linux에서는 DX7 facade처럼 공용 core를 쓰는 DX6 HLE 모듈로 차례로 옮긴다. TODO의 "DirectX 6 facade 공용 core" 항목과 같은 방향이다.

*On Linux these move one at a time into a DX6 HLE module on shared cores, as the DX7 facade did, in line with the TODO's DirectX 6 shared-core item.*
