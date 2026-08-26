# Direct3D 3 정점 버퍼 HLE 작업 로그

관련 설계: [Direct3D 3 정점 버퍼 HLE](../design/20260826-070-direct3d3-vertex-buffer-hle.md)
관련 작업 지시: [Direct3D 3 정점 버퍼 HLE 작업 지시](../work-orders/20260826-070-direct3d3-vertex-buffer-hle.md)

## 상태

**완료.** nullable `lpdwSize` 계약을 바로잡고 두 canonical 실행에서 정점 buffer Lock/Unlock과 후속 렌더링을 AV 없이 통과했다. 다음 안정 경계는 DirectSound duplicate 실패다.

## 작업 내용

- 플랫폼 중립 `LegacyVertexBuffer`(`include/re2dj/graphics/legacy_vertex_buffer.h`, `src/graphics/legacy_vertex_buffer.cpp`)를 추가했다. descriptor 4필드, 확인된 FVF 비트만 반영한 stride 계산, DX6 전체 buffer lock/unlock을 보존한다.
- `direct3d3_com_facade.cpp`에 `D3dCreateVertexBuffer`와 완전한 `IDirect3DVertexBuffer` vtable(`VertexBufferFacade`)을 구현하고 관찰 marker를 연결했다. aggregation 거절, `IID_IUnknown`/`IID_IDirect3DVertexBuffer`만 수용하는 QI, `ProcessVertices`/`Optimize`의 `E_NOTIMPL` 보수 경계가 포함된다.
- `windows_vfs_runtime_probe`에 DirectDrawCreate → QI(`IDirect3D3`) → `CreateVertexBuffer` → Lock/Unlock/`GetVertexBufferDesc` 흐름 검증을 추가했다.
- 단위 테스트 `legacy_vertex_buffer_test.cpp`를 스위트에 등록했다.

## 수정한 문제

- 초기 stride 계산을 texture 좌표 세트당 4바이트로 잘못 잡았다. FVF 텍스처 좌표 기본값은 2 float(u,v)=8바이트이며, 이는 작업 67에서 확정된 transformed-lit 정점 stride 32(`0x1c4`=16+4+4+8)와도 일치한다. 8바이트로 바로잡고 테스트를 갱신했다.
- `DDERR_LOCKED`는 SDK에 없는 상수여서 컴파일이 실패했다. `d3d.h`의 `D3DERR_VERTEXBUFFERLOCKED`(MAKE_DDHRESULT(2062))로 교체했다.
- `VbLock`은 게스트가 `lpdwSize=nullptr`을 넘기면 marker와 `*data` 기록 전에 `DDERR_INVALIDPARAMS`를 반환했다. `lplpData`만 필수로 검사하고 size는 선택 출력으로 처리했으며, facade 반환 포인터와 Lock 진입 self/vtable/data/size를 기록했다. probe도 원본과 같은 null-size 호출로 바꿨다.

## 검증

- `-DRE2DJ_WARNINGS_AS_ERRORS=ON` windows-x86 build 경고 0 + CTest 2/2 성공
- `-DRE2DJ_WARNINGS_AS_ERRORS=ON` windows-x64 build 경고 0 + CTest 1/1 성공(CI windows-x64 job 동일 조건)
- probe는 정점 버퍼 생성/lock/unlock/descriptor 복원을 통과한다.

## 원본 실행 관찰 (canonical 실행 1회)

trace `logs/windows_x86_launcher_probe/ez2dj1stse/20260826-022620-578.jsonl`:

- **`CreateVertexBuffer`가 처음으로 성공했다.** marker: `caps=0x00000000:fvf=0x00000112:vertices=121:flags=0x00000000`. FVF `0x112` = `XYZ|NORMAL|TEX1` — untransformed 파이프라인 사용이 확정됐다(설계 미확정 1건 해소). 유도 stride는 32다.
- 직후 게스트 코드 `0x00420353`에서 write AV(address `0x001b013c`, index 67, `[ebp-0x20]`=Lock 출력 local이 스택 잔재 `0x001af8dc`를 가리킴)로 첫 실행이 멈췄고, 이어지는 711회 execute@0 AV가 여러 thread(ntdll thread-start return, thread-name magic `0x406d1388` 서명 포함)에서 process 붕괴로 이어졌다.
- `IDirect3DVertexBuffer` marker(Lock/Unlock)는 하나도 기록되지 않았다.

## 정적 해독 (`0x00420230`–`0x00420390`, .text RVA=파일 오프셋)

원본 함수는 descriptor를 stack에 만들고 두 COM 호출과 이중 루프를 수행한다.

```mermaid
sequenceDiagram
    participant G as Guest function
    participant D3 as IDirect3D3 global
    participant VB as Vertex buffer
    G->>G: desc={dwSize=16, caps=0, fvf=0x112, vertices=0x79}
    G->>D3: CreateVertexBuffer(&desc, out=[ebp+8], flags=0, outer=0)
    G->>VB: Lock(vb, 1/DDLOCK_WAIT?, &[ebp-0x20], NULL)
    loop i,j in 0..10 (11×11=121)
        G->>G: idx = i + j*11; x=i/k 등 grid 좌표(fdiv [0x45495c])
        G->>VB: [data + idx*32 + {0,4,8,0xc}] 저장
    end
```

- call site `0x00420273` `call [eax+0x24]` 앞 pushes: `0(outer), 0(flags), [ebp+8](&out 통과), &desc, global(this)` — 계약 일치.
- `0x00420276`–`0x0042028b`: 반환된 객체로 `push vb; push 1; push &[ebp-0x20]; push 0; call [vtbl+0xc]` — `Lock(this, flags=1(DDLOCK_WAIT?), lplpData=&[ebp-0x20], lpdwSize=NULL)` 형태.
- `0x00420353` `mov [ecx+eax],edx`에서 ecx=`[ebp-0x20]`, eax=index×32. 이후 `+4`, `+8(=0)`, `+0xc(=0)` 저장이 이어진다.

## 원인과 최종 원본 실행

미호출 모순은 marker 위치 때문에 생긴 오판이었다. `VbLock`의 첫 조건이 `data == nullptr || size == nullptr`였으므로 원본의 null size 호출은 marker 전에 실패했고, 게스트는 HRESULT를 검사하지 않은 채 미초기화 local을 썼다.

최종 trace `20260826-104802-472.jsonl`, `20260826-104944-099.jsonl`은 각각 다음을 확인했다.

- 반환 facade와 Lock self가 실행별 `0A415510`, `0A558770`으로 일치하고 vtable도 모두 `66090AD8`이다.
- `size=00000000`, `flags=1` Lock이 3,872바이트(121×32) 출력 포인터를 반환하고 Unlock까지 1회씩 성공했다.
- `0x00420353`과 모든 `av_access`, OpenGL failure는 0회다. 후속 DrawPrimitive는 각각 2,961회와 3,855회 성공했다.
- 두 실행 모두 caller `0x00424f68`, `KSnd(ksndDuplicate) : Error on duplicate`에서 동일하게 제어 종료했다. 이는 다음 DirectSound HLE 경계다.

## Verification (English)

- Warnings-as-errors Windows x86 build with zero warnings and CTest 2/2 passing; Windows x64 build and CTest 1/1 passing (same conditions as the CI job).
- The vfs runtime probe passes vertex-buffer creation, lock, unlock, and descriptor restore.

## Original-run observations (English)

Trace 20260826-022620-578.jsonl first exposed the write AV and absent markers. Code review then showed that VbLock rejected the guest's null size output before its marker; the guest ignored the HRESULT and used its uninitialized local. Final traces 20260826-104802-472.jsonl and 20260826-104944-099.jsonl show matching returned/self facade pointers and vtables, one successful null-size Lock of 3,872 bytes and one Unlock each, zero access violations and OpenGL failures, and thousands of later DrawPrimitive calls. Both terminate through the same controlled caller 0x00424f68 with `KSnd(ksndDuplicate) : Error on duplicate`, the next DirectSound HLE boundary.
