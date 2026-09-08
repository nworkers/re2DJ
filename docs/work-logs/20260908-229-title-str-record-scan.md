# 작업 로그: title.str 레코드 구조 조사

## 한국어

### 관련 문서

- 분석: [ez2dj1stse CHD 파일시스템 분석](../analysis/ez2dj1stse-chd-filesystem.md)
- 선행 작업: [ez2dj1stse 스프라이트 적재 경계 관측](20260908-227-ez2dj1stse-sprite-load-boundary.md), [FAT32 항목 타임스탬프](20260908-228-fat32-entry-timestamps.md)

### 목적

[작업 227](20260908-227-ez2dj1stse-sprite-load-boundary.md)에서 세운 세 후보 중 두 번째, "`.str` 레코드가 담은 기대 크기·치수와 실제 값이 어긋나 게스트가 적재를 포기한다"를 검증했습니다. 코드 변경은 없고 원본 자산 조사만 수행했습니다.

### title.str 구조 — 확인됨

`System/Title/title.str`은 4,060 바이트입니다.

- 오프셋 `0x0000`에 16바이트 헤더. `0x00`과 `0x0c`에 값 `8`이 있습니다.
- 이름은 레코드 시작의 16바이트 필드에 널 종단 ASCII로 들어갑니다.
- 이름 오프셋은 `0x0010`, `0x0090`, `0x0110` … `0x0390`으로 `typo0000`–`typo0007` 8개가 **128바이트 간격**입니다.
- 이후 `logo-out`(`0x0760`), `logo`(`0x08f0`), `SE-logo`(`0x0b30`), `amuse`(`0x0cc0`), `version01`(`0x0e50`)은 간격이 976, 400, 576, 400, 400으로 **가변 길이**입니다.
- 각 이름 바로 앞에는 float와 작은 정수가 있습니다. 예를 들어 `logo` 앞에는 `4096.0`, `255.0` 계열 float와 `2`, `2`, `1`이 놓입니다.

가변 길이와 float 배열은 스프라이트마다 프레임이나 좌표 항목 수가 다른 레이아웃 스크립트와 맞습니다. 다만 레코드 필드의 의미는 이 조사에서 확정하지 않았습니다.

### 후보 2 배제 — 확인됨

`title.str`이 이름으로 참조하는 스프라이트의 실제 비트맵 값은 다음과 같습니다.

| 스프라이트 | 파일 크기 | 치수 |
| --- | --- | --- |
| `typo0000`, `logo-out`, `logo` | 196,664 | 256×256 24bpp |
| `SE-logo` | 93,680 | 331×94 24bpp |
| `amuse` | 42,164 | 483×29 24bpp |
| `version01` | 8,060 | 115×23 24bpp |

이 값들을 `title.str` 전체에서 `u32`, `u16`, `f32` 세 형식으로 훑었습니다. **파일 크기는 하나도 나타나지 않고, 치수도 나타나지 않습니다.** `256`이 `u32`로 18번 걸리지만 전부 인접한 `01 00 00 00` 필드를 한 바이트 어긋나게 읽은 것으로, `logo` 이름 앞 `0x08eb`가 그 예입니다.

따라서 `.str` 레코드는 비트맵의 기대 크기나 치수를 담지 않으며, 게스트가 그 값을 실제 파일과 대조해 적재를 포기한다는 설명은 성립하지 않습니다.

### 성급했던 추론 정정

캡처 화면의 대체 사각형이 실제 비트맵 치수와 같아 보여, 게스트가 치수를 안다고 적었습니다. PNG를 해독해 실측하니 `logo` 자리의 사각형은 **256×255 논리 픽셀**로 `logo.bmp`의 256×256과 거의 같습니다.

그러나 이것은 게스트가 파일을 읽었다는 증거가 **아닙니다.** 스프라이트 아트가 1:1로 배치되면 스크립트의 레이아웃 사각형이 원본 치수와 같아지는 것이 자연스럽습니다. 레코드 안의 float가 그 사각형일 가능성이 높습니다. 앞선 보고에서 이를 근거처럼 쓴 부분을 취소합니다.

### 그 밖의 확인

`System/Title`의 31개 항목을 모두 확인했습니다. 스프라이트 픽셀은 `.bmp`에만 있고 `.abm` 같은 패킹 아카이브는 없습니다. `title.wav`(9,438,308)와 `title.str`은 읽히고, `title.ezv`(9,495)는 이번 실행에서 열리지 않았습니다.

또한 이번 실행의 dynamic resolver 기록에서 `CreateFileA`, `ReadFile`, `GetFileSize`, `GetFileType`, `CloseHandle`이 모두 `route=hle`임을 확인했습니다. 게스트의 파일 접근이 후킹을 우회할 가능성은 없습니다.

### 남은 후보

1. ~~열거 결과의 타임스탬프~~ — 작업 228에서 배제
2. ~~`.str` 레코드의 기대 크기·치수 비교~~ — **이번에 배제**
3. 적재를 나중 단계로 미루는 정상 동작이고 그 단계에 도달하지 못하는 것

세 번째만 남았고, 이는 자산 쪽 관측으로는 더 좁힐 수 없습니다. 다음 단계는 존재 확인 직후 구간의 명령어 수준 추적입니다. `CreateFileA` 반환 지점에서 시작해 `CloseHandle` 호출까지의 분기를 따라가면 적재를 건너뛰는 조건을 직접 볼 수 있습니다. launcher에 이미 있는 `--code-window`와 `--field-write-watch`가 그 용도에 가깝습니다.

## English

### Related documents

- Analysis: [ez2dj1stse CHD filesystem analysis](../analysis/ez2dj1stse-chd-filesystem.md)
- Preceding tasks: [ez2dj1stse sprite-load boundary observation](20260908-227-ez2dj1stse-sprite-load-boundary.md), [FAT32 entry timestamps](20260908-228-fat32-entry-timestamps.md)

### Purpose

This task tested the second of the three candidates from [task 227](20260908-227-ez2dj1stse-sprite-load-boundary.md): that the guest abandons loading because an expected size or dimension carried in the `.str` record disagrees with the real file. No code changed; this was an investigation of the original assets.

### title.str structure — confirmed

`System/Title/title.str` is 4,060 bytes. A 16-byte header sits at offset `0x0000` with the value 8 at both `0x00` and `0x0c`. Names are null-terminated ASCII in a 16-byte field at the start of each record. The eight `typo0000`–`typo0007` names sit at `0x0010`, `0x0090`, `0x0110` … `0x0390`, a fixed **128-byte stride**. The later names — `logo-out` at `0x0760`, `logo` at `0x08f0`, `SE-logo` at `0x0b30`, `amuse` at `0x0cc0`, and `version01` at `0x0e50` — are spaced 976, 400, 576, 400, and 400 bytes apart, so those records are **variable length**. Floats and small integers precede each name; `logo` is preceded by floats in the `4096.0` and `255.0` family and the integers 2, 2, 1.

Variable length with float arrays fits a layout script whose per-sprite frame or coordinate count differs. The meaning of the individual fields was not established here.

### Candidate two eliminated — confirmed

The real bitmaps behind the names in `title.str` are 196,664 bytes at 256×256 24bpp for `typo0000`, `logo-out`, and `logo`; 93,680 bytes at 331×94 for `SE-logo`; 42,164 bytes at 483×29 for `amuse`; and 8,060 bytes at 115×23 for `version01`.

Scanning the whole of `title.str` for those values as `u32`, `u16`, and `f32` finds **no file size and no dimension**. The value 256 matches as `u32` eighteen times, but every hit is an adjacent `01 00 00 00` field read one byte early — `0x08eb`, just before the `logo` name, is one such case.

The `.str` record therefore carries neither an expected size nor expected dimensions, so the guest cannot be abandoning the load by comparing them against the real file.

### Correcting a premature inference

An earlier report said the placeholder rectangles matched the real bitmap dimensions and therefore that the guest knew them. Decoding the capture confirms the measurement: the rectangle in the `logo` position is **256×255 logical pixels** against `logo.bmp`'s 256×256.

That is **not** evidence that the guest read the file. Sprite art placed 1:1 naturally gives a script layout rectangle equal to the source dimensions, and the floats inside the record are the likely source of that rectangle. The earlier use of this as supporting evidence is withdrawn.

### Other observations

All 31 entries of `System/Title` were listed: sprite pixels live only in `.bmp` files, with no packed archive such as `.abm`. `title.wav` (9,438,308 bytes) and `title.str` are read; `title.ezv` (9,495 bytes) was not opened in this run.

The dynamic-resolver record for this run also shows `CreateFileA`, `ReadFile`, `GetFileSize`, `GetFileType`, and `CloseHandle` all at `route=hle`, so the guest's file access cannot be bypassing the hooks.

### Remaining candidate

Candidates one and two are eliminated. Only the third remains: that loading is normally deferred to a later stage which the run never reaches. Asset-side observation cannot narrow it further.

The next step is an instruction-level trace of the stretch immediately after the existence check — from the `CreateFileA` return to the `CloseHandle` call — to see the condition under which the load is skipped. The launcher's existing `--code-window` and `--field-write-watch` options are the closest fit.
