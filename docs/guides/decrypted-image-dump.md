# 실행 중 주 이미지 덤프 / Dumping the decrypted main image

보호된 빌드의 원본 코드는 디스크 파일에 없다. 이 절차는 실행 중 복호화된 주 이미지를 파일로 남겨 정적 분석이 가능하게 한다.

*A protected build's original code is not in its disk file. This procedure saves the decrypted main image from a running process so it can be analyzed statically.*

설계: [20260916-292-decrypted-image-dump.md](../design/20260916-292-decrypted-image-dump.md)
작업 로그: [20260916-292-decrypted-image-dump.md](../work-logs/20260916-292-decrypted-image-dump.md)
분석 결과: [보호 빌드의 런타임 복호화](../analysis/protected-build-runtime-decryption.md)

> 작업 449부터 덤프는 in-process 러너가 쓴다. "entry"는 이미지를 매핑하고 import를 연결한 직후(진입 전), "resumed"는 지연이 지난 뒤 게스트가 처음 부르는 import에서 쓴다. Linux에서도 같은 옵션으로 동작한다. 이미지가 통째로 매핑되어 있으므로 `gaps`는 늘 비어 있고 sidecar에 `"source": "in-process"`가 붙는다. 근거: [작업 449 설계](../design/20261004-449-windows-cli-in-process.md).
>
> *From task 449 the in-process runner writes the dumps: "entry" right after the image is mapped and its imports bound (before the entry), "resumed" at the guest's first import after the delay. The same options work on Linux. The image is mapped whole, so `gaps` is always empty, and the sidecar carries `"source": "in-process"`. See the [task 449 design](../design/20261004-449-windows-cli-in-process.md).*

## 한국어

### 덤프 뜨기

저장소 root의 PowerShell에서 실행한다. `--image-dump`는 제품 실행에 진단 옵션으로 붙는다.

```powershell
.\build\windows-x86\bin\Debug\re2dj.exe ez2dj4th --image-dump --image-dump-delay 8000
```

게스트 창이 뜨고 나면 잠시 두었다가 창을 닫는다. `--image-dump-delay`는 재개 후 두 번째 덤프까지의 밀리초이며 기본값은 5000이다. 게임이 그 시간 안에 종료하면 두 번째 덤프는 건너뛰고 진단에 그 사실이 남는다.

산출물은 그 실행의 로그 디렉터리에 남는다.

```
logs\image-dumps\<target>\<stamp>-<executable>.entry.image.bin
logs\image-dumps\<target>\<stamp>-<executable>.entry.image.json
logs\image-dumps\<target>\<stamp>-<executable>.resumed.image.bin
logs\image-dumps\<target>\<stamp>-<executable>.resumed.image.json
```

**분석에는 `resumed` 쪽을 쓴다.** `entry`는 복호화 이전이며, 대조용으로만 쓴다. 근거는 분석 문서 2절에 있다.

크기는 빌드의 `SizeOfImage`와 같다. 3rd가 약 6.8 MB, 1st Tracks는 약 27 MB다. `logs/`는 gitignore 대상이므로 저장소에 들어가지 않는다.

### sidecar 읽기

`.image.json`이 그 덤프가 어느 빌드의 것인지 말한다. **이 값 없이는 덤프를 분석 근거로 쓰지 않는다.**

| 항목 | 의미 |
| --- | --- |
| `target`, `executable` | 어느 제품, 어느 파일 |
| `timestamp`, `size_of_image`, `entry_point_rva` | 어느 빌드. 프로파일 fingerprint와 같은 값 |
| `file_size`, `file_digest` | 디스크 파일 동일성. `fnv1a64`이며 암호학적 해시가 아니다 |
| `image_base` | 덤프 오프셋을 실행 중 주소로 환산할 때 더한다 |
| `point`, `delay_ms` | 어느 시점 |
| `gaps` | 읽지 못해 0으로 채운 범위. 비어 있지 않으면 그 구간의 내용은 덤프의 것이 아니다 |
| `layout` | `virtual`. 아래 항을 볼 것 |

### 덤프 안에서 주소 찾기

덤프는 virtual 레이아웃이므로 **파일 오프셋이 곧 RVA다.**

* 진단 로그의 실행 중 주소 → 덤프 오프셋: `주소 - image_base`
* 프로파일의 helper RVA → 덤프 오프셋: 그대로

문자열을 찾는 예다.

```powershell
python -c "d=open(r'logs\image-dumps\ez2dj4th\<stamp>-EZ2DJ.resumed.image.bin','rb').read(); i=d.find(b'TotalCoin'); print(hex(i) if i>=0 else 'not found')"
```

나온 오프셋이 RVA이므로, `image_base`를 더하면 실행 중 주소가 된다.

### 덤프가 제대로 떠졌는지 확인하기

새 타깃에서 처음 뜰 때 확인한다.

1. sidecar의 `gaps`가 비어 있는지.
2. 파일 앞 `0x400` 바이트가 디스크 실행 파일과 일치하는지. PE 헤더는 보호 계층이 건드리지 않으므로 일치해야 한다. 어긋나면 덤프가 잘못된 것이다.
3. `entry`와 `resumed`가 다른지. 같다면 그 빌드는 진입점 시점에 이미 복호화되어 있거나, 복호화가 지연 시간 안에 끝나지 않은 것이다.

### 주의

* 덤프는 **실행 가능한 PE가 아니다.** import가 해석된 주소로 바인딩되어 있고 재배치가 적용되어 있다. 정적 분석용이다.
* 덤프와 그 내용은 원본 자산이다. **저장소에 커밋하지 않는다.** 분석 문서에는 바이트 열이 아니라 구조, 오프셋, 관찰된 동작만 적는다.

## English

### Taking a dump

Run from the repository root in PowerShell; `--image-dump` attaches to a normal product run as a diagnostic.

```powershell
.\build\windows-x86\bin\Debug\re2dj.exe ez2dj4th --image-dump --image-dump-delay 8000
```

Let the guest window come up, leave it a moment, then close it. `--image-dump-delay` is the milliseconds between the resume and the second dump, default 5000. If the game exits within that time the second dump is skipped and the diagnostic says so.

The output lands in that run's log directory as `<stamp>.entry.image.bin` / `.json` and `<stamp>.resumed.image.bin` / `.json`.

**Use the `resumed` pair for analysis.** `entry` precedes decryption and serves only as a control; the evidence is in section 2 of the analysis document.

The size equals the build's `SizeOfImage` — about 6.8 MB for 3rd, about 27 MB for 1st Tracks. `logs/` is gitignored, so nothing enters the repository.

### Reading the sidecar

The `.image.json` says which build the dump came from. **Do not use a dump as evidence without it.** It carries the target and executable; the `timestamp`, `size_of_image` and `entry_point_rva` that identify the build and match the profile fingerprints; `file_size` and `file_digest` for disk-file identity, the latter `fnv1a64` and not a cryptographic hash; the `image_base` to add when converting an offset to a runtime address; the `point` and `delay_ms`; the `gaps` that were zero-filled because they could not be read, whose contents are therefore not the guest's; and `layout`, which is `virtual`.

### Finding an address inside a dump

The dump is in virtual layout, so **a file offset is the RVA**. A runtime address from a diagnostic log becomes a dump offset as `address - image_base`, and a profile's helper RVA is already a dump offset. Adding `image_base` to a found offset gives the runtime address.

### Checking that a dump is sound

On the first dump of a new target:

1. The sidecar's `gaps` should be empty.
2. The first `0x400` bytes should equal the disk executable's. The PE headers are untouched by the protection, so a mismatch means the dump is wrong.
3. `entry` and `resumed` should differ. If they do not, either that build is already decrypted at its entry point or decryption did not finish within the delay.

### Cautions

* A dump is **not a runnable PE**: imports are bound to resolved addresses and relocations are applied. It is for static analysis.
* A dump and its contents are original assets. **Never commit them.** Analysis documents record structure, offsets and observed behavior, never byte dumps.
