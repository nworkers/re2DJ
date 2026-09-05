# Direct3D cull 상태 전달 설계

# Direct3D Cull-State Forwarding Design

## 배경 / Background

**확인됨:** 사용자 실행 `20260905-164819-234`의 Music Select 원판 draw(`texture=279`, `texture=387`)는 `D3DRENDERSTATE_CULLMODE=3` (`D3DCULL_CCW`)을 사용합니다. 현재 `LegacyFixedFunctionState`와 SDL3/OpenGL backend에는 cull mode 전달이 없습니다.

* **Confirmed:** In the user's `20260905-164819-234` Music Select run, the disc draws (`texture=279` and `texture=387`) use `D3DRENDERSTATE_CULLMODE=3` (`D3DCULL_CCW`). The current `LegacyFixedFunctionState` and SDL3/OpenGL backend do not carry cull mode.

## 목표 / Goals

- Direct3D `NONE`, `CW`, `CCW`를 공용 fixed-function state로 전달합니다.
- OpenGL에서 triangle primitive에만 back-face culling을 적용합니다.
- D3D의 화면 좌표와 현재 vertex shader의 Y 변환으로 인해 바뀌는 winding을 반대 front-face 방향으로 보정합니다.
- line primitive와 presentation quad에는 culling이 남지 않도록 명시적으로 끕니다.
- 지원하지 않는 cull enum은 조용히 무시하지 않고 draw를 거부합니다.

* Carry Direct3D `NONE`, `CW`, and `CCW` into the shared fixed-function state.
* Apply OpenGL back-face culling only to triangle primitives.
* Compensate for the winding direction as represented by Direct3D screen coordinates and the current vertex shader's Y conversion by selecting the opposite OpenGL front-face direction.
* Explicitly disable culling for line primitives and the presentation quad.
* Reject unsupported cull enum values instead of silently ignoring them.

## 매핑 / Mapping

Direct3D의 cull state는 제거할 back-face의 winding을 의미하고, OpenGL의 `glFrontFace`는 유지할 front-face의 winding을 의미합니다. 현재 셰이더가 guest top-left Y를 OpenGL bottom-left 방향으로 변환하므로 다음처럼 매핑합니다.

| Direct3D state | 제거되는 winding | OpenGL 설정 |
| --- | --- | --- |
| `D3DCULL_NONE (1)` | 없음 | `GL_CULL_FACE` disabled |
| `D3DCULL_CW (2)` | clockwise | `glFrontFace(GL_CCW)`, cull back |
| `D3DCULL_CCW (3)` | counter-clockwise | `glFrontFace(GL_CW)`, cull back |

*Direct3D cull state names the winding of back faces to remove, while OpenGL `glFrontFace` names the winding to keep as front-facing. Because the current shader converts guest top-left Y to OpenGL's bottom-left direction, the mapping is:* 

| Direct3D state | Back-face winding removed | OpenGL setting |
| --- | --- | --- |
| `D3DCULL_NONE (1)` | none | disable `GL_CULL_FACE` |
| `D3DCULL_CW (2)` | clockwise | `glFrontFace(GL_CCW)`, cull back |
| `D3DCULL_CCW (3)` | counter-clockwise | `glFrontFace(GL_CW)`, cull back |

## 구현 경계 / Implementation Boundary

```mermaid
flowchart LR
    R["Device render state\nCULLMODE"] --> B["BuildFixedFunctionState"]
    B --> S["LegacyFixedFunctionState\ncull_mode"]
    S --> G["OpenGL Draw\nglEnable/glDisable + glFrontFace"]
    G --> P["Triangle rasterization"]
```

Direct3D device 생성 시 기본 cull state는 `D3DCULL_CCW`로 초기화합니다. OpenGL function loader는 `glCullFace`와 `glFrontFace`를 함께 요구하며, `Present` 직전에는 `GL_CULL_FACE`를 끕니다. RGB565 target, blend, alpha test, texture sampling, Z state는 이 작업에서 변경하지 않습니다.

*The Direct3D device initializes its default cull state to `D3DCULL_CCW`. The OpenGL function loader requires both `glCullFace` and `glFrontFace`, and `Present` disables `GL_CULL_FACE` before its copy. RGB565 targeting, blending, alpha test, texture sampling, and Z state are unchanged by this task.*

## 검증 / Verification

1. Win32 build, unit tests, CTest를 실행합니다.
2. 사용자가 같은 환경에서 Music Select에 진입해 culling 적용 후 화면을 비교합니다.
3. 중앙 artwork/ring이 사라지거나 winding이 잘못된 다른 layer가 사라지는지 확인합니다.
4. culling 적용 전후를 비교하여 과노출이 해소되는지 기록합니다.

*Run the Win32 build, unit tests, and CTest. Then compare the same Music Select environment with culling enabled, checking that the center artwork and ring remain visible and that no incorrectly wound layer disappears. Record whether the overexposure changes.*

## 참고 / Reference

- [Microsoft D3DCULL enumeration](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dcull)
