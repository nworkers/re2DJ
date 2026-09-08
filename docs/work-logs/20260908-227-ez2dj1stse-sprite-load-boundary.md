# 작업 로그: ez2dj1stse 스프라이트 적재 경계 관측

## 한국어

### 관련 문서

- 분석: [ez2dj1stse CHD 파일시스템 분석](../analysis/ez2dj1stse-chd-filesystem.md)
- 선행 작업: [ez2dj1stse DirectDraw HLE 활성화](20260908-226-ez2dj1stse-directdraw-hle.md)

### 목적

[작업 226](20260908-226-ez2dj1stse-directdraw-hle.md)에서 렌더 루프까지 도달했지만 화면에 원본 아트워크가 나오지 않았습니다. 화면을 직접 캡처해 원본 자산과 비교하고, 끊기는 지점을 좁혔습니다.

### 캡처

`SetProcessDPIAware` 후 창의 client rect를 `CopyFromScreen`으로 잡는 스크립트로 t=8·16·20·25·45초 시점을 캡처했습니다. DPI 배율을 반영하지 않으면 좌표가 어긋나 창 밖 영역이 찍힙니다.

화면에는 아트워크 대신 **자산 이름이 적힌 사각형**이 배치됩니다. 가운데 `logo`, 그 아래 `SE-logo`, 좌상단 `VERSION`, 하단 `AMUSE`, 배경에 `typo` 계열 글자입니다. 배경 숫자가 프레임마다 바뀌므로 화면 자체는 살아 있습니다.

### 확인된 사실

**자산은 정상입니다.** CHD에서 `ez2dj/System/Title/logo.bmp`를 꺼내 PNG로 변환해 확인했습니다. 256×256 24bpp의 실제 EZ2DJ 로고입니다.

**이름표는 게스트가 그립니다.** re2DJ 소스 전체에 텍스트·glyph·placeholder 렌더링 코드가 없습니다(`TextOut`, `DrawText`, `ExtTextOut`, `CreateFont` 모두 없음). 따라서 저 이름표는 원본 코드의 자체 대체 표시입니다.

**이름표 문자열의 출처가 확인됩니다.** `System/Title/title.str`(4,060 바이트)은 스프라이트 표이고, 안에 든 문자열은 확장자 없는 이름 `typo0000`–`typo0007`, `logo-out`, `logo`, `SE-logo`, `amuse`, `version01`입니다. 화면의 이름표와 정확히 일치합니다.

**게스트는 비트맵을 한 번도 읽지 않습니다.** 이것이 이번 작업의 핵심 관측입니다.

| 확장자 | 열기 | 이후 호출 |
| --- | --- | --- |
| `.str` | `flags=0x00000080` 탐색 후 `0x20000080`(`FILE_FLAG_NO_BUFFERING`) 재개방 | `GetFileSize` → `ReadFile` |
| `.wav` | `flags=0x00000080` 한 번 | `ReadFile` 반복 |
| `.bmp` | `flags=0x00000080` 한 번 | **없음** |

61개 `.bmp` 전부가 열린 직후 닫히고 다음 파일이 열립니다. CHD 의사 핸들이 매번 `0xfccd0001`로 재사용되는 것이 닫힘의 증거입니다. 진단을 위해 `GetFileSize`와 `GetFileType`에 trace를 추가했는데, 두 API는 `.str`에만 호출되고 `.bmp`에는 한 번도 호출되지 않습니다. 즉 순수한 존재 확인입니다.

읽기가 향한 곳은 `.wav`(약 6 MB)와 `.str` 두 종류뿐입니다.

**후킹 누락이 아닙니다.** 원본 `.idata`의 파일 관련 import는 `CreateFileA`, `ReadFile`, `WriteFile`, `GetFileSize`, `GetFileType`, `CloseHandle`, `SetFilePointer`, `SetEndOfFile`, `FlushFileBuffers`, `FindFirstFileA`, `FindNextFileA`, `FindClose`, `GetCurrentDirectoryA`, `SetCurrentDirectoryA`가 전부이고 모두 HLE가 후킹합니다. `GetFileAttributesA`, `CreateFileMapping`, `MapViewOfFile`, `LoadImageA`, `_lopen`은 어느 표에도 없습니다. 따라서 게스트가 후킹되지 않은 경로로 비트맵을 읽고 있을 가능성은 배제됩니다.

**graphics 경계는 원인이 아닙니다.** DirectDraw HLE는 미구현 메서드를 한 건도 보고하지 않습니다. 게스트가 요청한 것은 primary surface 1개와 `128×128` off-screen surface 42개 생성, `GetDC`/`ReleaseDC` 42쌍, back buffer에 대한 color-fill `Blt` 8회, `Flip` 8회, `RenderState` 28회뿐입니다. **surface 간 blit도 texture 업로드도 요청되지 않습니다.** 스프라이트 수와 42라는 surface 수가 가깝고 크기가 원본과 무관한 고정 `128×128`인 점은, 게스트가 실제 크기를 모른 채(헤더를 읽지 않았으므로) 대체 surface를 만든다는 설명과 맞습니다.

### 결론

끊기는 지점은 graphics가 아니라 **게스트의 스프라이트 적재 결정**입니다. 게스트는 `.str`에서 이름을 읽고, 각 `<이름>.bmp`의 존재만 확인한 뒤, 픽셀을 읽지 않고 대체 표시로 넘어갑니다.

**미확정.** 그 결정을 만드는 입력이 무엇인지는 확인되지 않았습니다. 후보는 세 가지입니다.

1. `FindFirstFileA`/`FindNextFileA`가 채우는 `WIN32_FIND_DATAA`. 현재 `PopulateFindData`는 속성·크기·이름만 채우고 타임스탬프 세 필드를 0으로 둔다. 게스트가 타임스탬프나 그 밖의 필드로 캐시 유효성을 판단한다면 전부 거부될 수 있다.
2. `.str` 레코드가 담은 기대값(크기·치수)과 실제 값의 비교.
3. 적재를 나중 단계로 미루는 정상 동작이고, 그 단계에 도달하지 못하는 것.

확인 방법: 존재 확인 직후 구간을 명령어 수준으로 추적해 분기 입력을 관찰하거나, `FindFirstFileA`가 채우는 필드를 원본 FAT32 값으로 완전히 채운 뒤 같은 실행을 반복합니다.

### 코드 변경

| 파일 | 변경 |
| --- | --- |
| `injected_runtime.cpp` | `ReportVfsFileQuery` 추가. `GetFileSize`와 `GetFileType`이 결과를 file-event 예산 안에서 기록한다 |
| `injected_runtime.cpp` | VFS open trace 예산을 128에서 1024로. 128은 스프라이트 탐색 한 번에 소진되어 로그가 중간에 끊겼고, 그 때문에 "읽기 없음"이 "기록 없음"과 구분되지 않았다 |

### 검증

- Windows x86 Release build 성공
- `re2dj_unit_tests.exe` → `checks: 1421, failures: 0`
- `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only` → exit 0

### 남은 과제

- 스프라이트 적재 결정의 입력 확인 (위 세 후보)
- `FindFirstFileA`의 `WIN32_FIND_DATAA` 타임스탬프 필드를 FAT32 항목으로 채우는 것 검토
- 렌더 결과의 시각적 일치 확인은 위 문제 해결 이후로 미룸

## English

### Related documents

- Analysis: [ez2dj1stse CHD filesystem analysis](../analysis/ez2dj1stse-chd-filesystem.md)
- Preceding task: [ez2dj1stse DirectDraw HLE enablement](20260908-226-ez2dj1stse-directdraw-hle.md)

### Purpose

[Task 226](20260908-226-ez2dj1stse-directdraw-hle.md) reached a render loop, but the original artwork did not appear. This task captured the screen, compared it against the original assets, and narrowed where the pipeline breaks.

### Capture

A script that calls `SetProcessDPIAware` and then grabs the window's client rect with `CopyFromScreen` captured t = 8, 16, 20, 25, and 45 seconds. Without DPI awareness the coordinates are logical while the copy is physical, so the grab lands outside the window.

The screen places **rectangles labeled with asset names** where artwork belongs: `logo` centered, `SE-logo` beneath it, `VERSION` at the top left, `AMUSE` along the bottom, and `typo`-family glyphs in the background. A background counter changes between frames, so the screen is live.

### Confirmed facts

**The assets are fine.** Extracting `ez2dj/System/Title/logo.bmp` from the CHD and converting it to PNG shows the real 256×256 24bpp EZ2DJ logo.

**The guest draws the labels.** re2DJ contains no text, glyph, or placeholder rendering anywhere — no `TextOut`, `DrawText`, `ExtTextOut`, or `CreateFont`. The labels are therefore the original code's own fallback.

**The label strings are accounted for.** `System/Title/title.str` (4,060 bytes) is a sprite table whose strings are extension-less names: `typo0000` through `typo0007`, `logo-out`, `logo`, `SE-logo`, `amuse`, and `version01` — exactly the labels on screen.

**The guest never reads a bitmap.** This is the key observation. `.str` files are opened as a probe with `flags=0x00000080`, reopened with `0x20000080` (`FILE_FLAG_NO_BUFFERING`), then queried with `GetFileSize` and read. `.wav` files are opened once and read repeatedly. All 61 `.bmp` files are opened once and then nothing follows — the CHD pseudo-handle returns to `0xfccd0001` on every open, which shows each is closed immediately. Traces added to `GetFileSize` and `GetFileType` for this task record calls only for `.str`, never for a `.bmp`. Reads reach only `.wav` files, about 6 MB, and the `.str` scripts.

**Nothing escapes through an unhooked API.** The original `.idata`'s file-related imports are `CreateFileA`, `ReadFile`, `WriteFile`, `GetFileSize`, `GetFileType`, `CloseHandle`, `SetFilePointer`, `SetEndOfFile`, `FlushFileBuffers`, `FindFirstFileA`, `FindNextFileA`, `FindClose`, `GetCurrentDirectoryA`, and `SetCurrentDirectoryA` — all hooked. `GetFileAttributesA`, `CreateFileMapping`, `MapViewOfFile`, `LoadImageA`, and `_lopen` appear in neither import table.

**The graphics boundary is not the cause.** The DirectDraw HLE reports no unimplemented method. The guest asks only for one primary surface plus 42 off-screen `128×128` surfaces, 42 `GetDC`/`ReleaseDC` pairs, eight color-fill `Blt` calls to the back buffer, eight `Flip` calls, and 28 `RenderState` calls — **no surface-to-surface blit and no texture upload**. That the surface count is close to the sprite count, and that their size is a fixed `128×128` unrelated to the originals, fits a guest building fallback surfaces without knowing the real dimensions, which it never read.

### Conclusion

The break is not in graphics but in **the guest's decision to load a sprite**. It reads the names from `.str`, checks that each `<name>.bmp` exists, and then moves on to a fallback without reading any pixels.

**Unresolved.** What input drives that decision is unconfirmed. Three candidates: the `WIN32_FIND_DATAA` that `FindFirstFileA`/`FindNextFileA` fill, where `PopulateFindData` currently sets attributes, size, and name but leaves all three timestamps zero; a comparison against expected size or dimensions carried in the `.str` record; or a normal deferral of loading to a later stage the run never reaches. Verify by tracing the instructions right after the existence check to observe the branch inputs, or by filling every `WIN32_FIND_DATAA` field from the FAT32 entry and repeating the run.

### Code change

`ReportVfsFileQuery` was added so `GetFileSize` and `GetFileType` record their results under the existing file-event budget, and the VFS open-trace budget rose from 128 to 1024 because a single sprite-probe pass exhausted 128 and truncated the log, which made an absent read indistinguishable from an absent trace.

### Verification

The Windows x86 Release build succeeded, `re2dj_unit_tests.exe` reported `checks: 1421, failures: 0`, and `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only` exited 0.

### Remaining work

- Establish which input drives the sprite-load decision among the three candidates above.
- Consider filling the `WIN32_FIND_DATAA` timestamp fields from the FAT32 entry.
- Defer the visual comparison of rendered output until the above is resolved.
