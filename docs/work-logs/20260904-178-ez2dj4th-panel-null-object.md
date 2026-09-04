# 20260904-178 EZ2DJ 4th 패널 단계 null 객체 관측 결과
# 20260904-178 EZ2DJ 4th Panel-Stage Null Object Observation Results

## 1. 개요 (Overview)

Task 177이 남긴 중단 지점의 원인을 확정했다.

**결론: null인 것은 정점 버퍼다. 게스트는 `IDirect3D7::CreateVertexBuffer`로 버퍼를 만들어 자기 객체의 `+0x08`에 받고 곧바로 그 인터페이스의 vtable slot 3을 부른다. re2DJ의 `D3d7CreateVertexBuffer`는 `*vb = nullptr`을 넣고 `D3D_OK`를 돌려주므로, 게스트는 성공으로 받아들이고 null을 역참조한다.**

The null object is a vertex buffer. The guest calls `IDirect3D7::CreateVertexBuffer`, stores the result in its own object's `+0x08`, and immediately calls vtable slot 3 on it. re2DJ's `D3d7CreateVertexBuffer` writes `*vb = nullptr` and returns `D3D_OK`, so the guest treats the failure as success and dereferences null.

---

## 2. 변경 내용 (Changes Implemented)

`src/tools/windows_x86_launcher_probe/main.cpp`만 변경했다.

1. **코드 범위 교체.** `av2_callee`(`0x00012880`), `av2_caller`(`0x00038200`), `av2_outer`(`0x0003f780`), `link_thunks`.
2. **앵커 추가.** `panel_null_object`(`0x0001290e`).

---

## 3. 검증 결과 (Verification Results)

- `scripts/build_win32.bat`: 빌드 성공, 경고와 에러 0.
- `re2dj_unit_tests.exe`: 1,265 checks, 0 failures.
- 참조 스캔: `logs/windows_x86_launcher_probe/ez2dj4th/20260904-151411-117.jsonl`.
- 접근 위반 실행: `logs/windows_x86_launcher_probe/ez2dj4th/20260904-150114-649`.

### 3.1 faulting 함수의 본문 (확인됨)

앵커의 prologue 탐색이 함수 시작을 `RVA 0x00012875`로 보고했다. 복원한 본문이다.

```
0x00012875  push ebp; mov ebp, esp; sub esp, 0x40
0x0001288e  mov  [ebp-0x38], ecx            ; this
0x00012891  push 0x4bc / push 0 / push arg1
0x0001289c  call memset                     ; memset(arg1, 0, 0x4bc)
0x000128a4  push 0x10 / push 0 / lea ecx,[ebp-0x10]
0x000128ac  call memset                     ; 지역 서술자 초기화
0x000128b4  mov  dword [ebp-0x10], 0x00000010
0x000128bb  mov  dword [ebp-0x0c], 0x00000000
0x000128c2  mov  dword [ebp-0x08], 0x00000112
0x000128c9  mov  dword [ebp-0x04], 0x00000079
0x000128d2  push 0                           ; flags
0x000128d4  mov  edx, [ebp+0x08]
0x000128d7  add  edx, 8
0x000128da  push edx                         ; &arg1->[0x08]  (out)
0x000128db  lea  eax, [ebp-0x10]
0x000128de  push eax                         ; &서술자
0x000128e8  mov  ecx, [eax+0x2c]             ; this->[0x2c] = IDirect3D7
0x000128eb  mov  eax, [ecx]
0x000128ed  push edx
0x000128ee  call [eax+0x14]                  ; vtable slot 5
...
0x00012905  mov  eax, [edx+0x08]             ; arg1->[0x08]
0x0001290b  mov  edx, [ecx+0x08]
0x0001290e  mov  ecx, [edx]                  ; ***** 접근 위반 *****
0x00012911  call [ecx+0x0c]                  ; 새 인터페이스의 vtable slot 3
```

- **확인됨 — 지역 서술자는 `D3DVERTEXBUFFERDESC`다.** `dwSize = 0x10`(구조체 크기), `dwCaps = 0`, `dwFVF = 0x112`, `dwNumVertices = 0x79`(121)로 필드 배치가 정확히 맞는다.
- **확인됨 — 호출 규약이 `CreateVertexBuffer`와 맞는다.** `IDirect3D7`의 vtable slot 5가 `CreateVertexBuffer(desc, out, flags)`이고, 역순으로 `flags`, `out`, `desc`가 밀린다.
- **확인됨 — out은 `arg1->[0x08]`이다.** `add edx, 8` 뒤 그 주소를 넘긴다. 실패 후 그대로 읽는 필드와 같은 자리다.
- **확인됨 — 게스트는 반환값을 검사하지 않는다.** 호출 직후 곧바로 `arg1->[0x08]`을 vtable 있는 객체로 다룬다.

### 3.2 그 호출이 실제로 우리 facade로 들어왔다 (확인됨)

같은 실행의 `.ddraw.log` 마지막 줄이다.

```
re2dj:hle:IDirectDraw7::CreateSurface flags=0x00001007 caps=0x10005000 256x6
re2dj:hle:IDirect3D7::CreateVertexBuffer
```

`CreateVertexBuffer`는 실행 전체에서 한 번 기록되고, 그것이 마지막 그래픽 호출이다.

### 3.3 re2DJ의 구현이 null을 돌려준다 (확인됨)

`src/platform/windows/direct3d7_com_facade.cpp`의 `D3d7CreateVertexBuffer`는 서술자와 flags를 버리고 `*vb = nullptr`을 쓴 뒤 `D3D_OK`를 돌려준다. 게스트에게 이것은 "성공했고 버퍼는 null"이라는 모순된 응답이다.

### 3.4 어디까지 진행했는지 (확인됨)

`.vfs.log`의 마지막 자원은 `System/StreetMix/Panel/`의 `Judgment_Kool.str`부터 `combo0000.str`까지다. 호스트는 `wdmaud.drv`, `ksuser.dll`, `msacm32.drv`, `midimap.dll`을 적재했다. 게임 플레이 화면 구성 단계다.

- **확인됨 — 이 예외는 실행을 멈춘다.** 진단 로그 50,116줄 중 접근 위반이 50,099줄에 있고 그 뒤로 디버그 이벤트가 없다. 두 번 기록된 뒤 idle 경계에 도달했다.
- **미확정 — 다른 두 호출 지점의 영향.** 이 함수는 `0x000383f8`, `0x0007296e`, `0x0008ff0e` 세 곳에서 불린다. 이번에 걸린 것은 첫 번째다.

---

## 4. 가설 판정 (Hypothesis Verdicts)

| 가설 | 판정 |
| - | - |
| null이 자원 적재 실패에서 온다 | **반증.** 이번 실행의 자원 적재는 모두 성공했다 |
| null이 우리 facade 응답에서 온다 | **확인.** `CreateVertexBuffer`가 null을 쓴다 |
| 게스트가 반환값을 검사한다 | **반증.** 검사 없이 역참조한다 |
| 게스트가 이 예외를 흡수한다 | **반증.** 이후 디버그 이벤트가 없다 |

---

## 5. 다음 작업 (Next Task)

`IDirect3D7::CreateVertexBuffer`가 실제 `IDirect3DVertexBuffer7` facade 객체를 돌려주게 한다. vtable은 `QueryInterface`, `AddRef`, `Release`, `Lock`, `Unlock`, `ProcessVertices`, `GetVertexBufferDesc`, `Optimize`, `ProcessVerticesStrided` 순이며, 게스트가 곧바로 부르는 slot 3은 `Lock`이다. 서술자의 `dwNumVertices`와 `dwFVF`로 정점 하나의 크기를 정해 호스트 메모리를 잡고, `Lock`이 그 버퍼를 돌려주게 한다.

Make `CreateVertexBuffer` return a real `IDirect3DVertexBuffer7` facade. Slot 3, the one the guest calls immediately, is `Lock`; size the host allocation from the descriptor's `dwNumVertices` and `dwFVF` and hand that memory back from `Lock`.

---

## 6. 관련 문서 (Related Documents)

- [Task 178 설계](../design/20260904-178-ez2dj4th-panel-null-object.md)
- [Task 178 작업 지시서](../work-orders/20260904-178-ez2dj4th-panel-null-object.md)
- [Task 177 작업 로그](../work-logs/20260904-177-vfs-guest-working-directory.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)
