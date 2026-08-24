# Direct3D 3 OpenGL HLE 작업 지시

관련 설계: [Direct3D 3 OpenGL HLE](../design/20260825-061-direct3d3-opengl-hle.md)

## 상태

**첫 구현 단위 완료, OpenGL backend 대기.** COM 초기화 facade와 canonical 검증을 완료했다. surface storage, OpenGL resource, draw와 present는 후속 구현 단위다.

## 목표

원본 EZ2DJ의 DirectDraw/Direct3D 3 COM ABI와 호출 흐름을 보존하면서, modern Windows의 hardware-only `FindDevice` 결과에 의존하지 않는 OpenGL graphics HLE를 단계적으로 구현한다.

## 작업 범위

1. guest 32비트 COM facade와 `DirectDrawCreate` import gate를 별도 subsystem으로 만든다.
2. platform-neutral legacy surface/state/device model과 `RenderBackend` interface를 만든다.
3. 보수적인 virtual hardware device, Z-buffer 열거와 `IDirect3DDevice3` 생성을 제공한다.
4. RGB565 계열 surface, lock/unlock, depth, texture와 실제 관찰된 fixed-function draw를 구현한다.
5. desktop OpenGL backend와 platform context/present adapter를 분리한다.
6. Windows x86 unit/build test와 canonical 원본 실행으로 각 단계를 검증한다.
7. 새로 확인한 원본 호출·형식·상태를 analysis, architecture, KB와 작업 로그에 반영한다.

## 첫 구현 단위 완료 조건

COM identity/reference-count 단위 테스트를 통과하고, canonical 실행에서 hardware-only `FindDevice`, Z-buffer 열거와 `CreateDevice`를 HLE가 처리하여 `0x00422f39` 2차 null AV를 제거한다. 이후 최초 실패 지점을 두 번 이상 같은 결과로 기록한다. 화면 렌더링 완료는 후속 구현 단위로 분리한다.

## 실행 결과

`--hle-d3d3`와 Windows x86 COM facade를 구현했다. runtime probe와 CTest 2/2가 통과했다. 두 canonical 실행은 DirectDraw, Direct3D, surface, device, graphics-state stage를 모두 성공시키고 기존 null AV 없이 `0x00438987`의 port-I/O privileged instruction까지 동일하게 진행했다. 첫 구현 단위 완료 조건을 충족했다.

---

# Direct3D 3 OpenGL HLE Work Order

Related design: [Direct3D 3 OpenGL HLE](../design/20260825-061-direct3d3-opengl-hle.md)

## Status

**First implementation unit complete; OpenGL backend pending.** The initialization facade and canonical verification are complete. Surface storage, OpenGL resources, drawing, and presentation remain later units.

## Goal and scope

Preserve the original DirectDraw/Direct3D 3 COM ABI and call flow while implementing an OpenGL graphics HLE that does not depend on modern Windows hardware-only `FindDevice` behavior. Build guest-owned COM facades, a platform-neutral legacy graphics model and backend interface, a conservative virtual device, legacy surfaces and observed fixed-function drawing, and a separate desktop OpenGL platform backend. Verify each increment with asset-free tests and canonical execution against a user-supplied HDD directory.

## First implementation-unit completion criteria

Pass COM identity and reference-count tests; handle hardware-only `FindDevice`, Z-buffer enumeration, and `CreateDevice`; remove the secondary null-device AV at `0x00422f39`; and record the next first failure consistently in at least two canonical runs. Visible rendering remains a later implementation unit.

## Execution result

`--hle-d3d3` and the Windows x86 COM facade are implemented. The runtime probe and CTest pass 2/2. Two canonical runs complete every graphics initialization stage, eliminate the old null AV, and consistently reach the privileged port-I/O instruction at `0x00438987`. The first-unit criteria are satisfied.
