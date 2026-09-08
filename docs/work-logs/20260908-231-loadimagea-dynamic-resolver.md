# 작업 로그: LoadImageA 동적 해석 연결

## 한국어

### 관련 문서

- 분석: [ez2dj1stse CHD 파일시스템 분석](../analysis/ez2dj1stse-chd-filesystem.md), [자산 적재 경로](../analysis/ez2dj-asset-loading-path.md)
- 선행 작업: [스프라이트 적재 경계 관측](20260908-227-ez2dj1stse-sprite-load-boundary.md), [FAT32 타임스탬프](20260908-228-fat32-entry-timestamps.md), [title.str 레코드 조사](20260908-229-title-str-record-scan.md), [자산 열기 호출 지점과 코드 창](20260908-230-asset-open-caller-window.md)

### 확인된 원인

[작업 230](20260908-230-asset-open-caller-window.md)에서 얻은 프레임 코드 창으로 스프라이트 적재 함수를 해독했습니다. `0x00422be2`가 반환 지점입니다.

```
lea ecx,[ebp-1A0h] / push ecx        ; 출력 버퍼
lea edx,[ebp-9Ch]  / push edx        ; 요청한 이름
call 0x00423f70                      ; 자산 탐색 경로 resolver
0x00422be2: add esp,8
            cmp eax,1 / jne found    ; 0 = 찾음
            mov [eax+8],0            ; 못 찾음 -> 핸들 0
            jmp check
found:      push 0x2010              ; LR_LOADFROMFILE | LR_CREATEDIBSECTION
            push 0 / push 0 / push 0 ; cy, cx, IMAGE_BITMAP
            lea ecx,[ebp-1A0h] / push ecx
            push 0                   ; hInstance
            call [0x01EBA50C]        ; LoadImageA
            mov [edx+8],eax          ; HBITMAP 저장
check:      cmp [eax+8],0 / jne ok
            call 0x00422a20          ; 실패 시 대체 표시
ok:         ... GetObjectA(hbm, 24, &BITMAP) ...
```

**확인됨.** 스프라이트 픽셀은 `ReadFile`이 아니라 **`LoadImageA`**로 들어옵니다. 그래서 비트맵에 `ReadFile`이 한 건도 없었던 것이고, 그것은 결함의 증상이 아니라 이 게임의 적재 방식이었습니다.

**확인됨.** `[0x01EBA50C]`는 image base `0x00400000` 기준 RVA `0x01aba50c`이고, 원본 `.idata`의 `USER32.dll` IAT(`0x01aba4dc`, 21개 항목) 안 13번째 슬롯입니다. launcher는 이 슬롯을 정적으로 패치하지만, `.protect` packer가 unpack 시 원본 import를 스스로 해석하며 그 값을 덮어씁니다. 그래서 패치가 무효였고 `Re2djVfsLoadImageA`는 한 번도 실행되지 않았습니다.

`LoadImageA`는 주입 런타임의 **동적 resolver 표에 없었습니다.** `CreateFileA`, `ReadFile`, `DeviceIoControl`처럼 packer가 `GetProcAddress`로 물어볼 때 HLE thunk를 돌려주는 항목이 아니었던 것입니다.

resolver trace가 정확히 예산 128건에서 잘려 `LoadImageA`가 관측 목록에 보이지 않았던 것도 이 지점을 늦게 찾은 이유입니다.

### 코드 변경

주입 런타임의 동적 resolver에 `LoadImageA` 항목을 추가해 `Re2djVfsLoadImageA`를 돌려줍니다. 파일 API와 같은 취급이며, packed build에서 정적 슬롯 패치가 무효가 되는 상황에서 게스트의 호출이 VFS에 도달하는 유일한 경로입니다.

### 검증 — 결과

`re2dj ez2dj1stse` 실행 화면을 캡처했습니다.

**타이틀 화면이 원본대로 렌더링됩니다.** EZ2DJ 로고, `THE 1ST TRACKS / SPECIAL EDITION` 문자판, 좌상단 `VERSION 1.0`, 하단 `(C)1999 AmuseWorld All Rights Reserved.`, 그리고 회전하는 배경까지 모두 보입니다. 이름표 대체 표시는 사라졌습니다.

trace에서 `asset-open:api=LoadImageA`가 **42건** 기록됩니다. 이전 실행에서는 0건이었습니다. 42는 [작업 227](20260908-227-ez2dj1stse-sprite-load-boundary.md)에서 관측한 off-screen surface 42개와 일치하며, 그 surface들이 대체 표시용이라는 당시 추정과 맞습니다.

### 검증 — 시험과 회귀

- `re2dj_unit_tests.exe` → `checks: 1421, failures: 0`
- `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only` → exit 0
- 3rd·4th를 40초 제한으로 실행. 두 경우 모두 정상 실행 중이고 VFS trace 앞 120줄이 이전 실행과 동일합니다. 두 제품 모두 `LoadImageA` 자산 열기가 변경 전후 0건으로, 이 경로를 쓰지 않으므로 영향이 없습니다.

### 되짚어보기

작업 227에서 "게스트가 비트맵을 한 번도 읽지 않는다"를 확인했을 때, 그 다음 질문을 "왜 읽지 않기로 결정하는가"로 세웠습니다. 실제 답은 "읽지 않는 것이 정상이고 다른 API로 적재한다"였습니다. 후보 1과 2를 배제한 것은 낭비가 아니었지만, 질문의 틀 자체가 한쪽으로 좁혀져 있었습니다.

원본 import 목록을 확인할 때 `USER32.dll` 21개 항목의 이름을 펼쳐 봤다면 `LoadImageA`를 더 일찍 봤을 것입니다. 작업 227에서는 파일 관련 이름만 필터링해 출력했고 `LoadImageA`는 그 필터에 걸리지 않았습니다.

### 남은 과제

- 타이틀 이후 화면(모드 선택, 선곡, 게임플레이)의 렌더링 확인
- 정적 IAT 패치가 packed build에서 무효가 되는 다른 경계 점검. 같은 이유로 동작하지 않는 항목이 더 있을 수 있음
- resolver trace 예산 128이 관측을 자르는 문제. 자산 적재처럼 늦게 해석되는 이름이 목록에서 사라짐
- legacy I/O helper RVA가 이 빌드에서도 유효한지 실행으로 확인

## English

### Related documents

- Analysis: [ez2dj1stse CHD filesystem analysis](../analysis/ez2dj1stse-chd-filesystem.md), [asset loading path](../analysis/ez2dj-asset-loading-path.md)
- Preceding tasks: [sprite-load boundary](20260908-227-ez2dj1stse-sprite-load-boundary.md), [FAT32 timestamps](20260908-228-fat32-entry-timestamps.md), [title.str record scan](20260908-229-title-str-record-scan.md), [asset-open call site and code window](20260908-230-asset-open-caller-window.md)

### Confirmed cause

The frame code windows from [task 230](20260908-230-asset-open-caller-window.md) decoded the sprite-loading function, whose return site is `0x00422be2`. It calls the asset search-path resolver at `0x00423f70` with the requested name and an output buffer, and when that returns 0 it pushes `0x2010` — `LR_LOADFROMFILE | LR_CREATEDIBSECTION` — with a null instance and the resolved path and calls **`LoadImageA`** through `[0x01EBA50C]`, storing the `HBITMAP` and falling back to a labeled placeholder at `0x00422a20` only when that returns null.

**Confirmed.** Sprite pixels arrive through `LoadImageA`, not `ReadFile`. The complete absence of `ReadFile` on bitmaps was therefore not a symptom of a defect but how this game loads them.

**Confirmed.** `[0x01EBA50C]` is RVA `0x01aba50c` against image base `0x00400000`, the thirteenth slot of the original `.idata`'s `USER32.dll` IAT at `0x01aba4dc`. The launcher patches that slot statically, but the `.protect` packer resolves the original imports itself at unpack time and overwrites the value, so the patch was inert and `Re2djVfsLoadImageA` never ran.

`LoadImageA` was **absent from the injected runtime's dynamic resolver table**, unlike `CreateFileA`, `ReadFile`, and `DeviceIoControl`, so the packer's `GetProcAddress` request was answered with the real entry point. The resolver trace also truncates at its 128-entry budget, which is why `LoadImageA` never appeared in the observed name list.

### Code change

The dynamic resolver now answers `LoadImageA` with `Re2djVfsLoadImageA`, the same treatment the file APIs get. On a packed build where the static slot patch is overwritten, this is the only path by which the guest's own call reaches the VFS.

### Verification — result

**The title screen renders as the original.** The captured `re2dj ez2dj1stse` window shows the EZ2DJ logo, the `THE 1ST TRACKS / SPECIAL EDITION` plate, `VERSION 1.0` at the top left, `(C)1999 AmuseWorld All Rights Reserved.` along the bottom, and the animated background. The labeled placeholders are gone.

The trace records **42** `asset-open:api=LoadImageA` events where previous runs recorded none. That count matches the 42 off-screen surfaces observed in [task 227](20260908-227-ez2dj1stse-sprite-load-boundary.md), consistent with the inference there that those surfaces were the fallback path.

### Verification — tests and regression

`re2dj_unit_tests.exe` reported `checks: 1421, failures: 0` and `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only` exited 0. 3rd and 4th were each run under a 40-second bound, both still running, with their first 120 VFS trace lines identical to the previous runs. Both record zero `LoadImageA` asset opens before and after the change, so neither uses this path and neither is affected.

### Retrospective

When task 227 established that the guest never reads its bitmaps, the next question was framed as "why does it decide not to read them". The real answer was that not reading is normal and the load happens through a different API. Eliminating candidates one and two was not wasted, but the framing had narrowed the search prematurely.

Expanding the `USER32.dll` import names while listing the original `.idata` would have surfaced `LoadImageA` much earlier; task 227 filtered that listing to file-related names only, and `LoadImageA` did not match that filter.

### Remaining work

- Confirm rendering of the screens after the title: mode select, song select, and gameplay.
- Audit other boundaries whose static IAT patch is overwritten on a packed build; more may be inert for the same reason.
- The 128-entry resolver trace budget truncates observation and hides names resolved late, such as this one.
- Confirm by execution whether the legacy-I/O helper RVAs hold for this build.
