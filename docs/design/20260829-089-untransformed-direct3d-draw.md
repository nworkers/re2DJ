# 변환 전 Direct3D 3 정점 draw 설계

## 상태와 근거

**[구현·자동 검증 완료, 사용자 화면 재검증 대기.]** 작업 088 수정본을 사용한 사용자 재검증에서도 Music Select 중앙 그림은 나타나지 않았다. 최신 `20260829-015640-892.ddraw.log`는 `TextureLoad` 호출 0회를 기록하므로 작업 088의 직접 원인 추정을 기각한다. 같은 장면 진행 중 `DrawPrimitive` 실패는 정확히 두 형식으로 반복된다.

- `FVF 0x112`: `D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1`, 즉 32바이트 `D3DVERTEX`. texture 114/115를 사용하는 triangle strip이다.
- `FVF 0x1e2`: `D3DFVF_XYZ | D3DFVF_RESERVED1 | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1`, 즉 32바이트 `D3DLVERTEX`. 관찰된 호출은 texture가 없는 triangle strip이다.

현재 facade는 `D3DFVF_TLVERTEX`(`0x1c4`, 이미 screen-space인 XYZRHW)만 decode하고 위 두 draw에 `DDERR_UNSUPPORTED`를 반환한다. 반면 world/view/projection matrix와 `D3DVIEWPORT2`는 이미 COM facade에 저장된다. 원본 게임 로직이나 정점 데이터를 바꾸지 않고 이 상태를 정상적인 fixed-function vertex processing에 연결한다.

```mermaid
flowchart LR
    G[Original XYZ vertex] --> D[FVF decoder]
    D --> W[World transform]
    W --> V[View transform]
    V --> P[Projection transform]
    P --> C[Perspective divide]
    C --> VP[D3DVIEWPORT2 mapping]
    VP --> T[Existing XYZRHW draw command]
    T --> O[SDL3/OpenGL backend]
```

## 설계

1. 플랫폼 중립 `legacy_transform` 모듈이 row-vector Direct3D matrix 규약으로 world → view → projection을 순서대로 적용한다.
2. projection 결과의 `w`가 0이거나 입력·결과에 비정상 실수가 있으면 draw를 명시적으로 거절한다.
3. normalized 좌표를 `D3DVIEWPORT2`의 clip 범위와 screen rectangle으로 옮기고 `dvMinZ..dvMaxZ`에 z를 매핑한다. 결과는 기존 `TransformedLitVertex`로 정규화해 OpenGL backend를 변경하지 않는다.
4. `0x112`는 normal을 건너뛰고 texture UV를 보존하며 diffuse를 흰색으로 둔다. 현재 확인된 stage-zero `MODULATE(TEXTURE, DIFFUSE)`에서 이는 texture 색을 보존한다. lighting 계산은 아직 관찰 근거가 없어 추가하지 않는다.
5. `0x1e2`는 `RESERVED1` 4바이트를 stride와 offset에 포함하고 diffuse/specular/UV를 보존한다.
6. 기존 `0x1c4` decode와 line-list 경로는 그대로 유지한다.
7. facade는 현재 matrix와 viewport snapshot을 공용 decoder에 전달하는 ABI adapter만 담당한다.

## 검증

- identity matrix와 표준 viewport에서 XYZ가 예상 screen 좌표와 RHW로 변환되는지 단위 테스트한다.
- world/view/projection 합성, `D3DVERTEX`·`D3DLVERTEX` field offset, non-finite/zero-w, 잘못된 viewport를 검증한다.
- 기존 TL vertex 테스트가 회귀 없이 통과해야 한다.
- Windows x86 warnings-as-errors build와 CTest를 통과한다.
- 실제 실행에서 `FVF 0x112`/`0x1e2` 실패가 사라지고 Music Select 중앙 그림이 표시되는지 사용자가 재검증한다.

---

# Untransformed Direct3D 3 Vertex Draw Design

## Status and evidence

**[Implementation and automated verification complete; user-visible revalidation pending.]** The Music Select center remains absent in the user's Task 088 revalidation. Latest log `20260829-015640-892.ddraw.log` records zero `TextureLoad` calls, rejecting Task 088's direct-cause hypothesis. During the same scene progression, `DrawPrimitive` repeatedly rejects exactly two formats: 32-byte `D3DVERTEX` FVF `0x112` with textures 114/115, and 32-byte untextured `D3DLVERTEX` FVF `0x1e2`.

The facade currently decodes only screen-space `D3DFVF_TLVERTEX` (`0x1c4`) even though it already retains world, view, projection, and `D3DVIEWPORT2` state. This task connects that original fixed-function state without changing game logic or guest vertex data.

## Design

1. A platform-neutral `legacy_transform` module applies world, view, and projection in Direct3D row-vector order.
2. Reject zero projection `w` and non-finite input or output explicitly.
3. Map normalized coordinates through the `D3DVIEWPORT2` clip range, screen rectangle, and depth range, producing the existing `TransformedLitVertex` command so the OpenGL backend remains unchanged.
4. FVF `0x112` skips normals, preserves UV, and supplies white diffuse for the confirmed stage-zero texture/diffuse modulation. Lighting is deferred until observed.
5. FVF `0x1e2` includes the four-byte `RESERVED1` field in stride and offsets and preserves diffuse, specular, and UV.
6. Keep existing `0x1c4` and line-list behavior unchanged.
7. The facade remains an ABI adapter that snapshots current matrices and viewport into the shared decoder.

## Verification

- Unit-test identity and composed transforms, both FVF layouts, invalid floats/zero-w, invalid viewport, and existing TL regression.
- Pass the Windows x86 warnings-as-errors build and CTest.
- Ask the user to verify that FVF `0x112`/`0x1e2` failures disappear and the Music Select center artwork appears.
