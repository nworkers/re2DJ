# 작업 로그: 자산 열기 호출 지점과 코드 창

## 한국어

### 관련 문서

- 설계: [자산 열기 호출 지점과 코드 창 설계](../design/20260908-230-asset-open-caller-window.md)
- 작업 지시: [자산 열기 호출 지점과 코드 창](../work-orders/20260908-230-asset-open-caller-window.md)
- 선행 작업: [스프라이트 적재 경계 관측](20260908-227-ez2dj1stse-sprite-load-boundary.md), [FAT32 타임스탬프](20260908-228-fat32-entry-timestamps.md), [title.str 레코드 조사](20260908-229-title-str-record-scan.md)

### 코드 변경

주입 런타임에 자산 열기 호출 지점 진단을 추가했습니다.

| 항목 | 내용 |
| --- | --- |
| `ReportVfsAssetOpen` | 호출 지점 주소와 main image base 기준 RVA를 함께 기록 |
| `Re2djVfsCreateFileA` | 다른 호출보다 먼저 `_ReturnAddress()`를 잡아 게스트 호출 지점을 보존. `OpenChdReadFile`에 그대로 전달 |
| `ReportAssetOpenCallerWindow` | 첫 `.bmp` 열기에서 한 번만, 반환 주소 앞 32바이트·뒤 288바이트의 실행 시점 코드와 게스트 코드 범위에 드는 스택 값을 기록 |

`.protect` 빌드는 실행 시점에 `.text`를 복호화하므로 원본 파일의 바이트는 암호문입니다. 코드를 보려면 프로세스 안에서 읽어야 하고, 주입 런타임이 crash 보고에서 쓰던 `ReadProcessMemory(GetCurrentProcess(), ...)` 패턴을 그대로 사용했습니다.

### 확인된 사실 — 호출 지점이 둘로 갈린다

| 확장자 | 호출 지점 RVA | 횟수 |
| --- | --- | --- |
| `.bmp` | `0x00023f45` | 61 |
| `.str` | `0x00023f45` | 2 |
| `.str` | `0x0001ee3e` | 2 |

`0x00023f45`는 모든 자산이 거치는 지점이고, `0x0001ee3e`는 `.str`만 거칩니다. 뒤이어 `GetFileSize`와 `ReadFile`이 오는 것도 `0x0001ee3e` 쪽뿐입니다. 즉 앞의 것은 존재 확인, 뒤의 것은 실제 적재입니다. 비트맵은 적재 지점에 도달하지 않습니다.

### 확인된 사실 — `FileExists` 헬퍼

호출 지점 `0x00423f45`(VA)를 담는 함수는 다음과 같습니다.

```
push 0 / push 80h / push 3 / push 0 / push 1 / push 80000000h
mov eax,[ebp+8] / push eax / call [CreateFileA]
0x00423f45:  mov [ebp-4],eax
             cmp [ebp-4],-1
             jne +4
             xor eax,eax          ; 없음 -> 0
             jmp epilogue
             mov ecx,[ebp-4] / push ecx / call [CloseHandle]
             mov eax,1            ; 있음 -> 1
```

인자 하나를 받아 열고 닫는 존재 확인 함수입니다. HLE가 유효한 CHD 의사 핸들을 돌려주므로 이 함수는 비트맵에 대해 **1을 반환합니다.**

### 확인된 사실 — 자산 탐색 경로 resolver

스택에서 게스트 코드 범위의 반환 주소만 추려 그 위 프레임이 `0x00423feb`임을 확인했고, 넓힌 코드 창으로 그 함수 전체를 읽었습니다. `0x00423f70`에서 시작합니다.

```
mov [ebp-4],0                       ; i = 0
loop:
  cmp ecx,[0x01C4D0E0] / jge end    ; i < 디렉터리 개수
  imul edx,[ebp-4],0x104            ; 260바이트 레코드
  add  edx,0x01C4C0A0               ; 탐색 경로 테이블
  strcpy(local, prefix)
  strcat(local, [0x004570A8])       ; 구분자 문자열 상수
  strcat(local, [ebp+8])            ; 요청한 이름
  call 0x00423f20                   ; FileExists(local)
  test eax,eax / jz next
  strcpy([ebp+0Ch], local)          ; 찾은 전체 경로를 출력 버퍼에
  xor eax,eax / jmp ret             ; 0 = 성공
next: jmp loop
end:  mov eax,1                     ; 1 = 어디에도 없음
```

260바이트 레코드로 이루어진 디렉터리 테이블을 돌며 `<접두사><구분자><이름>`을 만들어 존재를 확인하고, 찾으면 전체 경로를 출력 버퍼에 복사하고 0을 반환합니다. [자산 적재 경로 분석](../analysis/ez2dj-asset-loading-path.md)이 기술한 검색 경로 테이블의 실제 구현입니다.

**확인됨.** 우리 비트맵은 이 resolver에서 성공합니다. 첫 후보 경로에서 `FileExists`가 1을 반환하고 경로가 출력됩니다. 따라서 "파일을 찾지 못해서 적재하지 않는다"는 설명은 배제됩니다.

### 결론

적재를 건너뛰는 판단은 resolver보다 **한 프레임 더 위**에 있습니다. resolver는 유효한 전체 경로를 돌려주는데, 그 경로로 파일을 여는 호출이 뒤따르지 않습니다.

스택에서 얻은 그 위 프레임 후보는 `0x004389ac`, `0x00419180`, `0x00422be2`입니다. 다음 단계는 이 주소들에 코드 창을 잡아 resolver 반환값을 어떻게 쓰는지 읽는 것입니다.

**미확정.** resolver 성공 이후 무엇이 적재를 막는지. 가설 하나는 게스트가 경로를 얻은 뒤 스프라이트 크기의 DirectDraw surface를 먼저 만들고, 그것이 실패하면 파일을 읽지 않고 대체 표시로 넘어간다는 것입니다. 실행에서 만들어진 surface가 원본 치수와 무관한 `128×128` 42개뿐인 점이 이 가설과 맞습니다. 확인 방법: 위 프레임의 코드 창을 읽어 resolver 반환값 이후의 호출 순서를 확인합니다.

### 검증

- Windows x86 Release build 성공
- `re2dj_unit_tests.exe` → `checks: 1421, failures: 0`
- `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only` → exit 0
- 진단은 첫 `.bmp` 열기에서 한 번만 기록하며, 자산 열기 trace는 기존 예산 안에서 동작합니다

## English

### Related documents

- Design: [Asset-Open Call Site and Code Window Design](../design/20260908-230-asset-open-caller-window.md)
- Work order: [Asset-Open Call Site and Code Window](../work-orders/20260908-230-asset-open-caller-window.md)
- Preceding tasks: [sprite-load boundary](20260908-227-ez2dj1stse-sprite-load-boundary.md), [FAT32 timestamps](20260908-228-fat32-entry-timestamps.md), [title.str record scan](20260908-229-title-str-record-scan.md)

### Code change

`ReportVfsAssetOpen` now records the call-site address and its main-image RVA; `Re2djVfsCreateFileA` takes `_ReturnAddress()` before any other call so the guest's own site is preserved and passes it through `OpenChdReadFile`; and `ReportAssetOpenCallerWindow` records, once on the first `.bmp` open, the run-time code from 32 bytes before to 288 bytes after that return address together with the stack values that fall in the guest image's code range.

A `.protect` build decrypts `.text` at run time, so the original file's bytes are ciphertext and the code has to be read from inside the process. The injected runtime's existing crash-report pattern, `ReadProcessMemory(GetCurrentProcess(), ...)`, was reused.

### Confirmed — two distinct call sites

All 61 `.bmp` opens and two `.str` opens come from RVA `0x00023f45`, while `.str` alone is opened twice more from `0x0001ee3e`, and only that second site is followed by `GetFileSize` and `ReadFile`. The first is an existence check and the second is the actual load; bitmaps never reach the load site.

### Confirmed — the FileExists helper

The function containing `0x00423f45` pushes `OPEN_EXISTING`, `FILE_SHARE_READ`, `GENERIC_READ` and `FILE_ATTRIBUTE_NORMAL`, calls `CreateFileA` with its single argument, returns 0 when the result is `INVALID_HANDLE_VALUE`, and otherwise closes the handle and returns 1. Since the HLE hands back a valid CHD pseudo-handle, it **returns 1** for every bitmap.

### Confirmed — the asset search-path resolver

Filtering the stack for guest-range return addresses put the next frame at `0x00423feb`, and the widened window covers that whole function, which begins at `0x00423f70`. It walks a table of 260-byte directory records at `0x01C4C0A0` whose count sits at `0x01C4D0E0`, builds `<prefix><separator><name>` with a string constant at `0x004570A8`, calls `FileExists` at `0x00423f20`, and on success copies the full path to the caller's output buffer and returns 0; it returns 1 only after exhausting the table. This is the concrete implementation of the search-path table described in the [asset loading path analysis](../analysis/ez2dj-asset-loading-path.md).

**Confirmed.** Our bitmaps succeed in this resolver: `FileExists` returns 1 on the first candidate path and the resolved path is written out. "The file was not found" is therefore eliminated as the reason the load is skipped.

### Conclusion

The decision to skip the load sits **one frame further up** than the resolver, which hands back a valid full path that is then never opened. The stack put the candidates for that frame at `0x004389ac`, `0x00419180`, and `0x00422be2`; the next step is a code window at those addresses to read how the resolver's return value is used.

**Unresolved.** What prevents the load after the resolver succeeds. One hypothesis is that the guest, having the path, first creates a DirectDraw surface of the sprite's size and falls back to the labeled placeholder without reading the file when that fails — consistent with the run creating only 42 fixed `128×128` surfaces unrelated to the source dimensions. Verify by reading the code window of the frame above to see the call order after the resolver returns.

### Verification

The Windows x86 Release build succeeded, `re2dj_unit_tests.exe` reported `checks: 1421, failures: 0`, and `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only` exited 0. The diagnostic records once, on the first `.bmp` open, and the asset-open trace stays within its existing budget.
