# 20260905-182 DirectX 7 facade의 DirectX 6 구현 위임 결과
# 20260905-182 Delegating The DirectX 7 Facade To The DirectX 6 Implementation — Results

## 1. 개요 (Overview)

DirectX 7 facade의 수용 스텁을 걷어내고, DirectX가 스스로 가진 버전 계층 구조를 이용해 DirectX 6 구현에 위임했다.

**결론: EZ2DJ 4th가 처음으로 실제 렌더 경로에 들어갔다. 창 모드가 적용되어 1,296×999 창이 열리고, back buffer가 달린 primary flip chain과 3D 디바이스와 Z 버퍼가 만들어지고, 첫 텍스처가 RGB565로 생성되어 GDI로 업로드된다. 다만 그 직후 게스트 자신의 코드에서 null 객체를 역참조해 종료하므로 화면은 아직 검다.**

**부수 확인: DirectX 6 경로(1st SE)는 회귀 없이 그대로 그린다. 1,296×999 창에서 표본 64점 중 36점이 비검정이다.**

The DirectX 7 acceptance stubs are gone, replaced by delegation to the DirectX 6 implementation through the version layering DirectX itself defines. EZ2DJ 4th now enters the real render path for the first time — the window mode is applied, a primary flip chain with a back buffer, a 3D device, and a depth buffer are created, and the first texture is created as RGB565 and uploaded through GDI — but the guest then dereferences a null object in its own code and exits, so the screen is still black. The DirectX 6 path is unchanged and still draws.

---

## 2. 변경 내용 (Changes Implemented)

`src/platform/windows/directdraw_legacy_interop.h` (신규)

1. **내부 경계.** DirectX 6 vtable 접근자, root 생성, 버전 중립 헬퍼를 선언한다. 공개 ABI가 아니라 두 facade 사이의 경계이며, `DIRECT3D_VERSION 0x0600`에서도 보이는 타입만 쓴다.

`src/platform/windows/direct3d3_com_facade.cpp` (공용 구현 계층)

2. **vtable 매개변수화.** `RootFacade`가 `surface_vtable`, `device_vtable`, `vertex_buffer_vtable`을 들고, 만드는 객체마다 그것을 설치한다. null이면 DirectX 6 기본값이므로 기존 경로는 바뀌지 않는다.
3. **표면 `Lock`/`Unlock` 구현.** 표면이 이미 들고 있는 픽셀과 pitch를 그대로 돌려주고, 부분 사각형 잠금은 그 사각형의 첫 픽셀 주소를 준다. `Unlock`은 텍스처 revision을 올려 백엔드 캐시를 무효화한다.
4. **`AddAttachedSurface` 구현과 `GetAttachedSurface` 확장.** 게스트가 붙인 깊이 버퍼를 기록해 되읽을 수 있게 했다. 이전에는 되읽기가 `DDERR_NOTFOUND`였다.
5. **`DDSCAPS_ZBUFFER` 표면 수용.** 깊이 픽셀은 백엔드가 소유하므로 서술자만 보관한다.
6. **`SetRenderTarget`/`GetRenderTarget`/`DrawPrimitiveVB` 구현.** DirectX 6 vtable에서 비어 있던 슬롯이며 버전 중립이므로 공용 계층에 넣었다.
7. **디바이스 viewport 상태.** DirectX 7은 viewport 객체 없이 디바이스에 설정하므로, 변환 상태 구성이 viewport 객체가 없을 때 이 상태를 읽도록 했다.
8. **디바이스 CLSID 확장.** 열거가 게시하는 RGB emulation과 T&L HAL도 받아들인다. 이전에는 HAL만 받았다.
9. **primary의 3DDEVICE 요청 보존.** 게스트가 primary 자체를 렌더 타깃으로 요구하면 그 cap을 유지한다.
10. **표면 픽셀 형식 기록.** `CreateSurface` 한 줄에 요청된 픽셀 형식을 남긴다.

`src/platform/windows/directdraw7_com_facade.cpp`, `direct3d7_com_facade.cpp`, `direct3d7_vertex_buffer_facade.cpp`

11. **위임 vtable.** 재사용 슬롯은 DirectX 6 함수 포인터를 슬롯마다 명시적으로 대입한다. vtable을 통째로 복사하지 않으므로 컴파일러가 두 멤버의 존재를 검사한다.
12. **어댑터 슬롯.** `CreateDevice`, `CreateVertexBuffer`, `SetTexture`, `GetTexture`, `DrawPrimitiveVB`, `DrawIndexedPrimitiveVB`, `Clear`, `SetViewport`, `GetViewport`가 변환 후 공용 구현을 부른다.
13. **`QueryInterface` 위임.** DirectX 7이 더한 식별자만 직접 답하고 나머지는 DirectX 6 구현으로 넘긴다. 텍스처 인터페이스처럼 같은 객체의 다른 멤버를 돌려주는 응답이 그대로 살아난다.
14. **미구현 슬롯 보고.** 남은 슬롯은 조용히 성공하는 대신 `not-implemented` 한 줄을 남긴다.
15. **정점 버퍼 통합.** `IDirect3DVertexBuffer7`이 DirectX 6 정점 버퍼 객체 위의 vtable이 되어 그리기 경로에 닿는다. 이전의 별도 facade 구현 287줄이 사라졌다.

`src/platform/windows/graphics_trace_log.*`

16. **호출 원장.** 미구현 슬롯이 메서드마다 작은 예산으로 자신을 기록한다. 요약이 아니라 개별 줄이므로 호출 순서가 남는다.

`src/platform/windows/directdraw_com_context.h` 삭제. DirectX 7 경로가 DirectX 6 객체를 그대로 쓰므로 별도 공유 상태가 필요 없어졌다.

---

## 3. 검증 결과 (Verification Results)

- `scripts/build_win32.bat`: 빌드 성공, 경고와 에러 0.
- `re2dj_unit_tests.exe`: 1,265 checks, 0 failures.
- `re2dj_windows_product_loader_probe.exe`: `profile-defaults=ok unsupported-target=ok resolve-iat-slot=ok`.
- 4th 진단 실행 3회: `20260905-005023-585`, `-005324-517`, `-005620-109`.
- 1st SE 제품 실행 2회: `20260905-005743-889` 및 후속 실행.

### 3.1 DirectX 6 경로에 회귀가 없다 (확인됨)

1st SE를 제품 loader로 실행하고 창 픽셀을 표본 조사했다.

```
t=0 size=1296x999 nonblack=36/64
t=1 size=1296x999 nonblack=36/64
t=2 size=1296x999 nonblack=36/64
t=3 size=1296x999 nonblack=36/64
```

- **확인됨 — 1st SE는 여전히 그린다.** 창 크기가 원본 640×480의 2배로 적용되었고 표본 64점 중 36점이 검정도 흰색도 아니다.
- **미확정 — 실행이 몇 초 뒤 스스로 끝나는 이유.** 이번 두 실행 모두 종료 코드 `0`으로 끝났다. 변경 전 같은 명령의 지속 시간을 측정해 두지 않았으므로 이것이 회귀인지 원래 동작인지 판단하지 않는다.

### 3.2 4th가 실제 렌더 초기화를 통과한다 (확인됨)

`20260905-005620-109`의 기록이다.

```
window-caption:event=mode-applied:hwnd=00370CC0:...:style=0x14cf0000
CreateSurface:flags=0x00000021:caps=0x00002218:0x0:back_buffers=1:pf_flags=0x00000000
IDirect3D7::CreateDevice
IDirect3D7::EnumZBufferFormats
CreateSurface:flags=0x00001007:caps=0x00024000:640x480:pf_flags=0x00000400:bpp=16
ddraw-trace:seq=3..12:RenderState:frame=0:...
CreateSurface:flags=0x00001007:caps=0x10005000:1024x512:pf_flags=0x00000040:bpp=16:
  r=0x0000f800:g=0x000007e0:b=0x0000001f
ddraw-trace:seq=14:GetDC:id=4:revision=1:result=0x00000000
ddraw-trace:seq=15:ReleaseDC:id=4:revision=2:result=0x00000000
```

| 지표 | 위임 전 (`20260905-001254-426`) | 위임 후 |
| - | - | - |
| 창 | 640×480, 창 모드 미적용 | **1,296×999, 창 모드 적용** |
| primary | 스텁이 수용, 픽셀 없음 | **flip chain, back buffer 1개, 실제 픽셀** |
| 디바이스 | 스텁 객체 | **공용 `DeviceFacade`** |
| Z 버퍼 | 스텁이 수용 | **수용 후 렌더 타깃에 부착** |
| 렌더 상태 | 무시 | **10건이 공용 상태에 기록** |
| 텍스처 | 733개, 업로드 없음 | 1개, **GDI 업로드 성공** |

- **확인됨 — 4th 텍스처의 픽셀 형식은 RGB565다.** `r=0xf800 g=0x07e0 b=0x001f`이며 공용 표면 backing이 그대로 받는다. 설계에서 미확정으로 남겼던 항목이 해소되었다.
- **확인됨 — primary의 back buffer 개수는 1이다.** 공용 구현의 제약과 일치한다.
- **확인됨 — 게스트는 primary 자체에 `DDSCAPS_3DDEVICE`를 요구한다.** caps `0x2218`에 그 비트가 있다.
- **확인됨 — Z 버퍼 표면이 필요하다.** `caps=0x24000`, `pf_flags=DDPF_ZBUFFER`, 640×480으로 생성된다.
- **확인됨 — 게스트가 부르는 미구현 슬롯은 `IDirect3DDevice7::SetMaterial` 하나뿐이다.** 세 실행 모두 같다.
- **확인됨 — 게스트는 표면과 디바이스에 `QueryInterface`를 부르지 않는다.** 두 곳에 기록을 달았으나 한 줄도 남지 않았다.

### 3.3 다음 차단 지점은 게스트 자신의 null 객체다 (확인됨)

세 실행 모두 같은 지점에서 접근 위반으로 끝난다.

```
av_registers: eip=0x00422b3a eax=0x00aca5b0 ecx=0x00000000
av_access: kind=read address=0x00000000
```

`RVA 0x00022b00`의 함수는 전역 객체 `0x00aca5b0`의 멤버 함수이고, 문제의 세 명령은 다음과 같다.

```
00422b31  8b 45 f8              mov  eax, [ebp-8]        ; this
00422b34  8b 88 10 0a 00 00     mov  ecx, [eax+0xa10]    ; = 0
00422b3a  8b 01                 mov  eax, [ecx]          ; fault
00422b3c  52                    push edx
00422b3d  ff 50 24              call [eax+0x24]
```

- **확인됨 — 첫 텍스처 업로드 직후에 발생한다.** 직전 기록이 `GetDC`와 `ReleaseDC`다.
- **확인됨 — 세 실행에서 재현된다.** 레지스터 값까지 같다.
- **확인됨 — 이 호출은 COM이 아니다.** `this`를 스택에 올리지 않고 인자 하나만 밀어 넣는 thiscall이므로, 대상은 우리 facade 객체가 아니라 게스트 자신의 C++ 객체다.
- **추정 — 위임 이전에는 이 경로에 도달하지 않았다.** 스텁의 `GetDC`가 null 핸들을 돌려주어 게스트가 다른 분기를 탔고, 이제 성공하므로 새 분기에 들어간다. 그 분기가 요구하는 객체를 무엇이 만드는지는 확인하지 않았다.

### 3.4 검증하지 못한 것 (미확정)

- **미확정 — `Flip`과 `Present`.** 게스트가 첫 프레임을 넘기기 전에 종료하므로 이번 실행으로는 확인할 수 없다. 4th 창 픽셀은 검정 그대로다.
- **미확정 — `Clear`의 색.** 백엔드는 프레임마다 검정으로 지우므로 게스트가 다른 색을 요구하면 어긋난다. 요청 값을 기록만 해 두었고 아직 관측되지 않았다.
- **미확정 — `SetMaterial`의 영향.** 유일하게 관측된 미구현 슬롯이지만 조명을 쓰지 않는 그리기에는 영향이 없을 수 있다.

---

## 4. 설계 대비 (Against The Design)

| 설계 항목 | 결과 |
| - | - |
| 접두 확장 관계를 슬롯 대입으로 재사용 | 적용. DirectDraw 7 슬롯 7개, Surface 7 슬롯 17개, Device 7 슬롯 13개, VertexBuffer 7 슬롯 7개 |
| 객체 모델 공유 | 적용. `IDirect3D7`만 예외로 별도 객체이며 근거를 코드 주석에 남겼다 |
| 미구현 슬롯 보고 | 적용. 실제로 `SetMaterial` 하나를 잡아냈다 |
| DirectX 6 동작 불변 | 유지. 새 구현은 비어 있던 슬롯을 채운 것이고 기존 슬롯의 동작은 바꾸지 않았다 |
| DirectX 6 파일 분할 | **하지 않음.** 설계 166이 제안한 `directdraw4_com_facade` 분리는 이번 범위 밖이며, `direct3d3_com_facade.cpp`는 3,285줄에서 3,917줄로 늘었다. 다음 작업의 부담으로 남는다 |

---

## 5. 다음 작업 (Next Task)

`RVA 0x00022b3a`의 null 객체를 추적한다. 전역 `0x00aca5b0`의 `+0xa10` 필드를 채우는 코드를 찾고, 그 초기화가 무엇을 기다리는지 확인한다. Task 164와 Task 178이 같은 종류의 문제를 다뤘으므로 그 절차를 따른다.

Trace the null object at `RVA 0x00022b3a`: find what fills the `+0xa10` field of the global at `0x00aca5b0` and what that initializer is waiting on. Tasks 164 and 178 handled the same class of problem and their procedure applies.

---

## 6. 관련 문서 (Related Documents)

- [Task 182 설계](../design/20260905-182-directx7-legacy-delegation.md)
- [Task 182 작업 지시서](../work-orders/20260905-182-directx7-legacy-delegation.md)
- [Task 166 IDirect3D7 / IDirectDraw7 COM Facade 분리 설계](../design/20260904-166-direct3d7-com-facade.md)
- [Task 181 작업 로그](20260904-181-hardlock-exit-attribution-log.md)
- [Task 179 작업 로그](20260904-179-direct3d7-vertex-buffer-facade.md)
