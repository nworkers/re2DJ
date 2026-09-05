# Direct3D와 OpenGL의 cull winding

# Direct3D and OpenGL Cull Winding

Direct3D `D3DCULL_NONE`은 면을 제거하지 않고, `D3DCULL_CW`는 clockwise back face를, `D3DCULL_CCW`는 counter-clockwise back face를 제거합니다. OpenGL은 `glFrontFace`로 front-face winding을 지정하고 `GL_CULL_FACE`/`glCullFace(GL_BACK)`로 back face를 제거하므로, 두 API 사이에서는 “제거할 winding”과 “유지할 winding”을 변환해야 합니다.

*Direct3D `D3DCULL_NONE` removes no faces, `D3DCULL_CW` removes clockwise back faces, and `D3DCULL_CCW` removes counter-clockwise back faces. OpenGL selects the front-face winding with `glFrontFace` and removes back faces with `GL_CULL_FACE`/`glCullFace(GL_BACK)`, so an API bridge must convert the winding that is removed into the winding that is kept.*

현재 re2DJ vertex shader는 guest의 top-left 화면 Y를 OpenGL clip 좌표의 반대 Y 방향으로 변환합니다. 그러므로 D3D `CCW`/`CW`의 보존 방향은 OpenGL `GL_CW`/`GL_CCW`로 각각 반대 매핑해야 합니다. 이는 이 프로젝트의 shader 경로에 대한 구현 규칙이며, 모든 Direct3D vertex pipeline에 일반화하지 않습니다.

*The current re2DJ vertex shader converts the guest top-left screen Y into OpenGL clip coordinates with the opposite Y direction. Therefore the preserved direction for D3D `CCW`/`CW` maps to OpenGL `GL_CW`/`GL_CCW`, respectively. This is an implementation rule for this project's shader path and is not generalized to every Direct3D vertex pipeline.*

참고: [Microsoft D3DCULL enumeration](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dcull), [Microsoft Culling State](https://learn.microsoft.com/en-us/windows/win32/direct3d9/culling-state)

*References: [Microsoft D3DCULL enumeration](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dcull) and [Microsoft Culling State](https://learn.microsoft.com/en-us/windows/win32/direct3d9/culling-state).*
