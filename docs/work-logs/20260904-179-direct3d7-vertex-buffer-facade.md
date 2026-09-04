# 20260904-179 Direct3D7 정점 버퍼 facade 구현 결과
# 20260904-179 Direct3D7 Vertex Buffer Facade Results

## 1. 개요 (Overview)

Task 178이 확정한 원인을 고쳤다.

**결론: `IDirect3D7::CreateVertexBuffer`가 실제 `IDirect3DVertexBuffer7`을 돌려주게 하자 접근 위반이 사라졌다. 게스트는 121정점, stride 32, 3,872바이트 버퍼를 만들어 `Lock`과 `Unlock`을 마치고 계속 진행한다. 실행은 이제 접근 위반 없이 끝까지 가서 `ExitProcess(1)`로 스스로 종료한다.**

**부수 확인: surface 생성이 236개에서 1,108개로 늘고, 배경 애니메이션(`EZ2DJ/BG/aquaris/`)까지 적재된다. 남은 예외는 우리 진단이 심은 single step(`0x80000004`)뿐이다.**

Returning a real vertex buffer clears the access violation: the guest creates a 121-vertex, 32-stride, 3,872-byte buffer, locks and unlocks it, and continues. Execution now reaches a deliberate `ExitProcess(1)` with no access violation anywhere, after creating 1,108 surfaces and loading background animation data.

---

## 2. 변경 내용 (Changes Implemented)

1. **전용 파일 추가.** `src/platform/windows/direct3d7_vertex_buffer_facade.{h,cpp}`. `IDirect3DVertexBuffer7`의 아홉 슬롯을 모두 구현하고, 저장소는 플랫폼 공용 `re2dj::graphics::LegacyVertexBuffer`를 쓴다. DX6 경로가 이미 쓰는 그 저장소다.
2. **연결.** `D3d7CreateVertexBuffer`가 새 facade 생성 함수로 위임한다. 성공과 함께 null을 돌려주는 조합을 없앴다.
3. **빌드 배선.** `CMakeLists.txt`에 새 소스를 추가했다.

---

## 3. 검증 결과 (Verification Results)

- `scripts/build_win32.bat`: 빌드 성공, 경고와 에러 0.
- `re2dj_unit_tests.exe`: 1,265 checks, 0 failures.
- `re2dj_windows_product_loader_probe.exe`: 전체 통과.
- 진입 추적: `logs/windows_x86_launcher_probe/ez2dj4th/20260904-152525-592`.

### 3.1 정점 버퍼가 동작한다 (확인됨)

```
re2dj:hle:IDirect3D7::CreateVertexBuffer caps=0x00000000 fvf=0x00000112 vertices=121 stride=32 bytes=3872
re2dj:hle:IDirect3DVertexBuffer7::Lock flags=0x00000001 bytes=3872 size_out=none
re2dj:hle:IDirect3DVertexBuffer7::Unlock
re2dj:hle:IDirect3DVertexBuffer7::Release:destroyed
```

- **확인됨 — stride 계산이 설계대로다.** `dwFVF = 0x112`는 `XYZ | NORMAL | TEX1`이고 `12 + 12 + 8 = 32`, `121 × 32 = 3,872`다.
- **확인됨 — 게스트는 `size`를 null로 넘긴다.** `Lock(flags=1, &data, nullptr)`이며 facade가 이를 허용한다.
- **확인됨 — 생성·잠금·해제 주기가 3회 반복된다.** `CreateVertexBuffer` 3회, `Lock` 3회, `Unlock` 3회다.

### 3.2 접근 위반이 사라졌다 (확인됨)

이번 실행의 예외는 모두 `0x80000004`(single step)이며, 우리 진단이 심은 진입 앵커에서 발생한 것이다. `0xc0000005`는 한 건도 없다.

### 3.3 실행이 크게 전진했다 (확인됨)

| 지표 | Task 177 실행 | 이번 실행 |
| - | - | - |
| 디버그 이벤트 | 50,035 | **156,565** |
| `IDirectDraw7::CreateSurface` | 236 | **1,108** |
| `RestoreAllSurfaces` / `EnumSurfaces` | 3 / 3 | 7 / 7 |
| 접근 위반 | 2 | **0** |
| 종료 | idle 경계까지 정지 | `ExitProcess(1)` |

가장 작은 surface가 9x13까지 내려간다. 글리프 단위 자원까지 적재한다는 뜻이다. `.vfs.log`의 마지막 자원은 `EZ2DJ/BG/aquaris/eye01.str`, `zoom01.str`으로 배경 애니메이션 단계다.

### 3.4 새 중단 지점은 스스로 종료하는 것이다 (확인됨)

프로세스는 예외 없이 `exit_process code 0x00000001`로 끝난다. 직전 기록은 Hardlock descriptor IOCTL이다.

```
device-ioctl-entry:code=0x9c40244c:input_size=256:output_size=256
hardlock-device:request=descriptor:outcome=completed:bytes=256:answered=0:status_cleared=1
```

- **추정 — 보호 검사가 종료를 결정한다.** 종료 직전 기록이 Hardlock 요청이고 `answered=0`이다. 다만 종료 코드 1을 쓰는 코드 경로는 아직 특정하지 않았다.
- **미확정 — `ExitProcess(1)`을 부르는 지점.**

---

## 4. 가설 판정 (Hypothesis Verdicts)

| 가설 | 판정 |
| - | - |
| null 정점 버퍼가 접근 위반의 원인이다 | **확인.** 실제 버퍼를 주자 사라졌다 |
| 게스트가 만든 버퍼를 곧바로 잠근다 | **확인.** 생성 직후 `Lock` |
| 공용 `LegacyVertexBuffer`로 충분하다 | **확인.** FVF stride와 lock 상태가 그대로 맞는다 |

---

## 5. 다음 작업 (Next Task)

`ExitProcess(1)`을 부르는 지점을 특정한다. 런처에는 이미 `--probe-exit-process`와 `--break-exit-process`가 있으므로, 종료 지점에서 멈춰 호출자 프레임을 읽는 것이 첫 단계다. Hardlock 응답이 원인인지는 그 호출자가 어느 검사 경로에 있는지로 판단한다.

Identify what calls `ExitProcess(1)`. The launcher already has `--probe-exit-process` and `--break-exit-process`, so stopping there and reading the caller frames comes first; whether the Hardlock answer is the cause follows from which check that caller belongs to.

---

## 6. 관련 문서 (Related Documents)

- [Task 179 설계](../design/20260904-179-direct3d7-vertex-buffer-facade.md)
- [Task 179 작업 지시서](../work-orders/20260904-179-direct3d7-vertex-buffer-facade.md)
- [Task 178 작업 로그](../work-logs/20260904-178-ez2dj4th-panel-null-object.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)
