# 작업 095 — Direct3D 3 indexed vertex-buffer draw 결과

## 분석

작업 094의 제품 실행 `20260829-233725-840`이 남긴 WER dump를 읽기 전용으로 분석했다. exception thread 23292는 `EIP=0`, `ESP=0x001af9d4`, 첫 복귀 주소 `0x004206a3`을 기록했다. 원본 복귀 지점 직전 `0x0042069d`의 `call dword ptr [edx+0x8c]`와 stack 인자를 DirectX 6 ABI에 대조해 호출을 다음과 같이 확정했다.

```text
IDirect3DDevice3::DrawIndexedPrimitiveVB(
    D3DPT_TRIANGLELIST, vertex_buffer, indices, 600, 0)
```

dump의 `EDX`가 가리키는 runtime facade vtable에서 `+0x8c`가 null이어서 EIP 0과 직접 일치했다. 원본 dump, 실행 파일과 raw memory는 저장소에 복사하지 않았다. 분석용으로 `build/`에 만든 임시 minidump probe와 모든 컴파일 산출물도 확인 뒤 삭제했다.

## 구현

- 공용 `PrimitiveTopology`에 triangle list를 추가하고 SDL/OpenGL backend에서 `GL_TRIANGLES`로 변환했다.
- `LegacyVertexBuffer`가 read-only storage와 lock 상태를 노출하고, 공용 `ExpandIndexedVertices`가 16-bit index를 vertex count에 대해 검사해 stride 단위 stream으로 전개한다.
- Win32 `IDirect3DDevice3` vtable의 `DrawIndexedPrimitiveVB` 슬롯을 채웠다. adapter는 guest pointer, facade vtable/magic, 같은 DirectDraw root 소유권, lock 상태와 index 범위를 검사한 뒤 기존 transform·fixed-function·texture draw 경로를 재사용한다.
- runtime probe에 성공하는 indexed triangle list, locked-buffer 실패와 범위 밖 index 실패를 추가했다. 공용 단위 테스트는 index 순서·범위와 triangle-list vertex count를 검증한다.

## 검증

- Windows x86 Debug 전체 build 성공
- Debug CTest 3/3 통과
- Windows x86 Release 전체 build 성공
- Release CTest 3/3 통과
- 제품 실행 `20260830-000841-620`: 약 3분 동안 PID 35872가 응답 상태를 유지하고 render failure와 새 access violation이 없었다. 정상 `WM_CLOSE` 뒤 `runtime_detached_exit` code `0x00000000`, success outcome을 기록하고 child와 loader가 모두 종료했다.

기존 execute-at-zero와 오디오 종료 교착은 재현되지 않았다. 다만 이 제품 재실행의 draw trace에는 primitive 4 marker가 없으므로 동일 호출의 실제 진입 성공은 runtime probe에서 확정됐고, 이전 실행에서 해당 원본 경로를 선택한 최초 상태 조건은 미확정으로 남긴다.

---

# Task 095 — Direct3D 3 indexed vertex-buffer draw result

## Analysis

The WER dump from Task 094 product run `20260829-233725-840` was analyzed read-only. Exception thread 23292 records EIP zero, ESP `0x001af9d4`, and first return `0x004206a3`. Comparing the preceding `call dword ptr [edx+0x8c]` at `0x0042069d` and its stack arguments against the DirectX 6 ABI identifies `IDirect3DDevice3::DrawIndexedPrimitiveVB(D3DPT_TRIANGLELIST, vertex_buffer, indices, 600, 0)`. The runtime-facade vtable addressed by EDX has a null `+0x8c` slot, directly matching EIP zero. No original dump, executable, or raw memory was copied into the repository. The temporary minidump probe and all of its build outputs under `build/` were deleted after analysis.

## Implementation

- Added triangle-list topology to the shared command and mapped it to `GL_TRIANGLES` in the SDL/OpenGL backend.
- Exposed read-only storage and lock state from `LegacyVertexBuffer`; shared `ExpandIndexedVertices` validates 16-bit indices against vertex count and expands a stride-sized stream.
- Filled the Win32 `IDirect3DDevice3::DrawIndexedPrimitiveVB` slot. The adapter validates guest pointers, facade vtable/magic, same-root ownership, lock state, and index range before reusing the existing transform, fixed-function, texture, and draw path.
- Extended the runtime probe with a successful indexed triangle list plus locked-buffer and out-of-range-index failures. Shared unit tests cover index ordering/range and triangle-list vertex counts.

## Verification

- Complete Windows x86 Debug build passed
- Debug CTest passed 3/3
- Complete Windows x86 Release build passed
- Release CTest passed 3/3
- Product run `20260830-000841-620`: PID 35872 remained responsive for roughly three minutes with no render failure or new access violation. A normal `WM_CLOSE` produced `runtime_detached_exit` code `0x00000000` and a success outcome; both child and loader exited.

The former execute-at-zero failure and audio shutdown deadlock did not recur. This product rerun contains no primitive-four draw marker, however, so successful entry into the identical call is confirmed by the runtime probe while the original state condition that selected the path in the earlier run remains unresolved.
