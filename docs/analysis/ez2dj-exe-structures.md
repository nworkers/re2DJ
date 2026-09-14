# EZ2DJ 실행 파일 구조 / EZ2DJ Executable Structures

주제: 원본 EZ2DJ 실행 파일 각각의 PE 구조, 보호 계층 해부, 데이터 인벤토리, 관찰된 런타임 흐름. 새 실행 파일이 확인될 때마다 이 문서에 섹션 하나를 추가해 누적한다.

*Topic: the PE structure, protection anatomy, data inventory, and observed runtime flow of each original EZ2DJ executable. Whenever a new executable is identified, add one section here.*

측정 도구: `re2dj_pe_analyzer <file>` (헤더·섹션·데이터 디렉터리), `re2dj_hdd_probe <dir>` (덤프 식별), `re2dj_windows_x86_launcher_probe` (런타임 관찰, `--api-trace` 등). import 함수 목록은 [import 표면 분석](ez2dj-import-surface.md)이, 덤프 디렉터리 구조와 실행 파일 식별 근거는 [HDD 레이아웃 분석](ez2dj-hdd-layout.md)이 담당한다. 이 문서는 실행 파일 단위 구조만 담는다.

*Measurement tools: `re2dj_pe_analyzer <file>` (headers, sections, data directories), `re2dj_hdd_probe <dir>` (dump identification), `re2dj_windows_x86_launcher_probe` (runtime observation via `--api-trace` and friends). The import function lists live in the [import surface analysis](ez2dj-import-surface.md); dump directory structure and executable identification live in the [HDD layout analysis](ez2dj-hdd-layout.md). This document carries per-executable structure only.*

## 표기 규칙 / Notation

모든 서술은 확인됨 / 추정 / 미확정 중 하나로 표기한다. 확인됨에는 측정 방법을, 추정에는 근거를, 미확정에는 확인 방법을 함께 적는다.

*Every statement is marked confirmed, inferred, or unresolved, with the measurement method, evidence, or the way to find out.*

---

## 공통 특성 / Common traits

**확인됨.** 확인한 **게임·도구 실행 파일** 전부 `PE32 / i386 / Windows GUI (subsystem 2, 버전 4.0)`, image base `0x00400000`, section alignment `0x00001000`, file alignment `0x00001000`, size of headers `0x00001000`, `dll flags 0x0000`이다. `dll flags 0`은 `DYNAMIC_BASE`(ASLR)와 `NX_COMPAT`(DEP) 어느 쪽도 선호하지 않는다는 뜻이다. 예외는 9.4절의 `AllowIo.exe`(콘솔 subsystem, image base `0x01000000`)와 `PortTalk.sys`(native 커널 드라이버, image base `0x00010000`, alignment `0x20`) 둘뿐이며, 둘 다 게임 코드가 아니라 legacy I/O 접근 도구다.

*Confirmed. Every **game and tool executable** inspected is PE32 / i386 / Windows GUI (subsystem 2, version 4.0) at image base 0x00400000 with 0x1000 section/file alignment, 0x1000 header size, and dll flags 0x0000 — meaning no ASLR (`DYNAMIC_BASE`) and no DEP opt-in (`NX_COMPAT`). The only exceptions are the two legacy-I/O access tools in section 9.4 — `AllowIo.exe` (console subsystem, image base 0x01000000) and `PortTalk.sys` (native kernel driver, image base 0x00010000, 0x20 alignment) — neither of which is game code.*

**확인됨 — 2026-09-15.** COFF characteristics의 `RELOCS_STRIPPED` 비트는 base relocation 데이터 디렉터리의 유무와 거의 일치한다. `0x010e`(비트 없음)인 파일은 실제 relocation 디렉터리를 갖고, `0x010f`(비트 있음)인 파일은 갖지 않는다. **예외가 정확히 둘 있고, 둘 다 packer가 헤더를 다시 쓴 빌드다.**

*Confirmed — 2026-09-15. The `RELOCS_STRIPPED` bit of the COFF characteristics almost always agrees with whether a base-relocation data directory exists: files at `0x010e` (bit clear) carry a real relocation directory and files at `0x010f` (bit set) do not. **There are exactly two exceptions, and both are builds whose header a packer rewrote.***

| 파일 / file | characteristics | base relocation directory | 보호 / protection |
| --- | --- | --- | --- |
| 1st Tracks `Ez2DJ.exe` | `0x010e` | 있음 / present | `.protect` |
| 1st SE `ez2dj.exe` (`.gtide`) | `0x010e` | **없음 / absent** | `.gtide` |
| 1st SE `Ez2DJ.exe` (`.protect`) | `0x010e` | 있음 / present | `.protect` |
| 2nd `EZ2DJ.exe` | `0x010e` | 있음 / present | 미확정 / unresolved |
| 3rd `EZ2DJ.EXE` (두 빌드 / both) | `0x010e` | 있음 / present | `.protect` |
| 4th `EZ2DJ.exe` | `0x010e` | 있음 / present | `.protect` |
| 5th `EZ2DJ.exe` | `0x010e` | 있음 / present | `.protect` |
| 6th `EZ2DJ.EXE`·`EZ2DJ6th.EXE`·동봉 1st | `0x010f` | 없음 / absent | 없음 / none |
| `ez2d2m` `EZ2Dancer.exe` | `0x010f` | **있음 / present** | `.protect` |
| 1st SE `Test.exe` | `0x010e` | 있음 / present | 없음 / none |
| 1st Tracks `Test.exe`·`PlzPowerOff.exe`·`AllowIo.exe` | `0x010f` | 없음 / absent | 없음 / none |
| `PortTalk.sys` | `0x010e` | 있음 / present | 없음 / none |

1st SE `.gtide` 빌드는 비트를 지운 채 디렉터리를 비워 두어 선호 주소 고정을 강제하고, `ez2d2m`는 비트를 세운 채 디렉터리를 남겨 둔다. `RELOCS_STRIPPED`를 존중하는 loader는 후자의 relocation 데이터를 쓰지 않는다. 두 헤더가 왜 이렇게 어긋나 있는지는 **미확정**이며, 따라서 이 비트 하나로 재배치 가능 여부를 판단하지 않고 데이터 디렉터리를 함께 확인한다.

*The 1st SE `.gtide` build clears the bit while leaving the directory empty, forcing the image to its preferred base, and `ez2d2m` sets the bit while leaving a directory behind — which a loader honoring `RELOCS_STRIPPED` will not use. Why the two headers disagree this way is **unresolved**, so relocatability is judged from the data directory rather than from the bit alone.*

**확인됨 — 2026-09-14 재측정.** 아래는 PE header의 TimeDateStamp이며 파일시스템 날짜가 아니다. 두 값이 다른 경우가 있으므로 빌드 시점은 이 표를 따른다.

*Confirmed — re-measured 2026-09-14. The table lists the PE header TimeDateStamp, not the filesystem date; the two differ for several files, so build time follows this table.*

| 파일 / file | 타임스탬프 | UTC | 덤프 / dump | 입력 / input |
| --- | --- | --- | --- | --- |
| `Ez2DJ.exe` | `0x3862fd9d` | 1999-12-24 04:59:09 | 1st Tracks | 디렉터리 / directory |
| `ez2dj.exe` | `0x3862df27` | 1999-12-24 02:49:11 | 1st SE | 디렉터리 / directory |
| `Ez2DJ.exe` | `0x3862df27` | 1999-12-24 02:49:11 | 1st SE | CHD |
| `Test.exe` | `0x38607297` | 1999-12-22 | 1st SE | 디렉터리 / directory |
| `Test.exe` | `0x374d68b1` | 1999-05-27 | 1st Tracks | 디렉터리 / directory |
| `PlzPowerOff.exe` | `0x3700321a` | 1999-03-30 | 1st SE·1st Tracks | 디렉터리 / directory |
| `EZ2DJ.exe` | `0x40fa7af9` | 2004-07-18 | 2nd | 디렉터리 / directory |
| `EZ2DJ.EXE` | `0x3baea943` | 2001-09-24 | 3rd | 디렉터리 / directory |
| `EZ2DJ.EXE` | `0x3bca98a3` | 2001-10-15 | 3rd | CHD |
| `EZ2DJ.exe` | `0x3d369bfd` | 2002-07-18 | 4th | CHD |
| `EZ2DJ.exe` | `0x3f53377b` | 2003-09-01 | 5th | 디렉터리 / directory |
| `EZ2DJ.EXE` (bootstrap) | `0x411646a8` | 2004-08-08 | 6th | 디렉터리·CHD |
| `EZ2DJ6th.EXE` | `0x411f6d44` | 2004-08-15 | 6th | 디렉터리·CHD |
| `Ez2DJ.exe` (동봉 1st / bundled 1st) | `0x411bbf5c` | 2004-08-12 | 6th | CHD |
| `AllowIo.exe` | `0x3c3fc787` | 2002-01-12 | 1st Tracks·6th 동봉 | 디렉터리·CHD |
| `PortTalk.sys` | `0x3c3fdf10` | 2002-01-12 | 1st Tracks·6th 동봉 | 디렉터리·CHD |
| `EZ2Dancer.exe` | `0x3a5f074c` | 2001-01-12 | `ez2d2m` | 디렉터리·CHD |

**확인됨 — 2026-09-15.** 1st Tracks 정식 실행 파일(`0x3862fd9d`)과 1st SE 정식 실행 파일(`0x3862df27`)의 TimeDateStamp는 같은 날 약 2시간 10분 차이다. 두 파일은 내용이 다르고 섹션 배치도 다르므로 같은 빌드가 아니다. 두 제품의 빌드 시각이 왜 이렇게 가까운지는 **미확정**이며, 이 값만으로 제품의 출시 순서를 판단하지 않는다.

*Confirmed — 2026-09-15. The 1st Tracks canonical executable (`0x3862fd9d`) and the 1st SE canonical executable (`0x3862df27`) carry TimeDateStamps about two hours and ten minutes apart on the same day. The two files differ in content and section layout, so they are not the same build. Why the two products' build times sit this close is **unresolved**, and release order is not inferred from these values alone.*

**확인됨.** 3rd는 입력에 따라 서로 다른 빌드다. 기존 디렉터리 덤프의 `EZ2DJ.EXE`는 `0x3baea943`이고, 현재 제품이 실행하는 `roms/ez2dj3rd` CHD의 `EZ2DJ.EXE`는 3주 뒤인 `0x3bca98a3`이다. 3rd 관찰을 인용할 때는 어느 입력에서 나온 것인지 함께 적는다.

*Confirmed. 3rd is a different build depending on the input: the earlier directory dump's `EZ2DJ.EXE` is `0x3baea943`, while the `roms/ez2dj3rd` CHD the product actually runs carries `0x3bca98a3`, three weeks later. Cite which input a 3rd observation came from.*

### 파일 해시 / File hashes

**확인됨 — 2026-09-15.** 아래는 각 실행 파일의 크기와 해시다. 같은 제품이라도 입력(디렉터리 덤프 / CHD)에 따라 다른 빌드일 수 있으므로, 관찰을 인용할 때는 경로 대신 이 표의 해시로 대상을 특정한다. CHD 안의 파일은 `re2dj_chd_probe --dump`로 꺼낸 바이트에 대한 값이다.

*Confirmed — 2026-09-15. Sizes and hashes per executable. The same product can be a different build depending on the input (directory dump vs CHD), so cite an observation's subject by the hash in this table rather than by path. Values for files inside a CHD are taken over the bytes extracted with `re2dj_chd_probe --dump`.*

| 대상 / subject | 크기 / size | MD5 | SHA-1 |
| --- | --- | --- | --- |
| 1st Tracks `Ez2DJ.exe` | 577,536 | `3d858e6560dc629e1878da35a923e32b` | `9ad7d1cf0414165b9639126e04a8bd0bb3e9a0bf` |
| 1st SE `ez2dj.exe` (`.gtide`, 디렉터리) | 561,152 | `41d6adc1397fe8eb653b8de624b5602a` | `12d365d0248cf15543f83761b14d990ba086dc2f` |
| 1st SE `Ez2DJ.exe` (`.protect`, CHD) | 634,880 | `5760cfb4f556d70711f18f473457c57b` | `3f3f960542dd9573f529dc8fb45dacd0206baffe` |
| 2nd `EZ2DJ.exe` | 1,028,155 | `4bfe2ac5367e7fa38fe02577d9624b78` | `166cd0d89d3006a8b3f5637db33aaaf58cc4cb64` |
| 3rd `EZ2DJ.EXE` (디렉터리 덤프) | 1,216,512 | `58f38d14ffd50d79307775b44c26166a` | `3e8a17c5ef27d89ab8d95f5aed857900db4bf02a` |
| 3rd `EZ2DJ.EXE` (CHD) | 1,216,512 | `bb447ee2581f77d340d416d2daf090ab` | `11c2061c2c022b5a8b44820eb72c8d052ed6d058` |
| 4th `EZ2DJ.exe` (CHD) | 1,372,160 | `ed0284500b65019d2195e1a022a295de` | `995dffd262ad518321d2008a83722ee4945c96e4` |
| 5th `EZ2DJ.exe` | 1,388,544 | `a3e99089536e7eeab5e8eb13f99309cd` | `4321452730cdef27172522a8a9d7a769a0dcaeea` |
| 6th `EZ2DJ.EXE` (bootstrap) | 126,976 | `ce6d77d8303682636a7050a43215a140` | `0f86d71d7326a1ced7f6cd89ae77ddb24b950e72` |
| 6th `EZ2DJ6th.EXE` (게임 본체 / game body) | 585,728 | `6acf3660802402498a8ea84bb721d3e5` | `887115b709df7358985e86937daf253c1eac84bd` |
| 6th 동봉 1st `Ez2DJ.exe` / bundled 1st | 360,448 | `e0a9718c890c799076f8f084aeabe8eb` | `3a4380ff9c133bcac1badf0f1159da8e096237a4` |
| `ez2d2m` `EZ2Dancer.exe` | 622,592 | `44ccb76d26f5dceb2e5d84d147a40390` | `a8eb2081a1c9f4e9ef36283304bcc9fa734e188b` |
| 1st Tracks `Test.exe` | 266,240 | `799b3f62f1a46253e67f31cd9d571977` | `834d204c0d3eafcf8f27f84f190d0ddce6540e66` |
| 1st SE `Test.exe` | 1,859,633 | `d32f1c4d90cd45cade649b633b06116a` | `0b735b94f0a811fa18eed0bce9f76a9536bc1575` |
| 1st Tracks `PlzPowerOff.exe` | 98,304 | `987fa1a51c08ae23f77ccecdeba96f37` | `47d00f7343f6521d448ef0f19101a0c055ec3e47` |
| 1st SE `PlzPowerOff.exe` | 98,304 | `f2ea5bce4991d702805a08ba4bf3bafe` | `c6826018b4bd50e86a3cc1ded818e2c06abf28e4` |
| `AllowIo.exe` | 40,125 | `9ad64da441e1db7d4d9b83ffa9b23838` | `ef2d05323e590c4a9c6a9412e1074c898b078996` |
| `PortTalk.sys` | 3,567 | `7d5a2d755b6c6579f63657b527d6ff1b` | `fd7d864b96bafa21a76128bfb02dcccb57eddad6` |

**확인됨 — 2026-09-15.** `PlzPowerOff.exe`는 1st Tracks와 1st SE에서 크기(98,304)와 PE TimeDateStamp(`0x3700321a`)가 같지만 해시가 다르다. 같은 빌드 시각의 서로 다른 파일이므로, 두 덤프 사이에서 이 도구를 동일 파일로 취급하지 않는다. 차이의 원인은 **미확정**이다.

*Confirmed — 2026-09-15. `PlzPowerOff.exe` has the same size (98,304) and PE TimeDateStamp (`0x3700321a`) in 1st Tracks and 1st SE but different hashes. They are different files carrying the same build time, so the tool is not treated as one file across the two dumps. The cause of the difference is **unresolved**.*

---

### 절 색인 / Section index

세대순으로 읽으려면 이 표를 따른다. 절 번호는 문서에 추가된 순서이므로 세대순과 다르다.

*Read in generation order by this table. Section numbers follow the order in which sections were added to the document, which is not generation order.*

| 제품 / product | 절 / section |
| --- | --- |
| 1st Tracks | 6 |
| 1st SE | 1 |
| 2nd | 2 |
| 3rd | 3 |
| 4th | 4 |
| 5th | 7 |
| 6th (bootstrap·게임 본체·동봉 1st) | 8 |
| EZ2Dancer 2nd MOVE | 5 |
| 보조 도구 / auxiliary tools | 9 |
| 새 실행 파일 추가 절차 / procedure | 10 |

---

## 1. `ez2dj.exe` — 1st SE 정식 실행 파일 (보호됨)

### 1.0 1st SE는 보호 계열이 서로 다른 두 빌드로 존재한다 / 1st SE exists as two builds from different protector families — 확인됨

**확인됨 — 2026-09-15.** 디렉터리 덤프 `roms/ez2dj1stse/ez2dj/ez2dj.exe`(561,152바이트)와 `roms/ez2dj1stse/ez2dj1stse.chd` 안의 `ez2dj/Ez2DJ.exe`(634,880바이트)는 PE TimeDateStamp가 `0x3862df27`로 같지만 서로 다른 파일이다. 앞의 것은 `.gtide`/`.gdata`/`.gidata` 8섹션 배치이고, 뒤의 것은 `.protect` 6섹션 배치다. 아래 1.1절부터의 값은 전부 디렉터리 덤프(`.gtide` 빌드) 기준이다.

*Confirmed — 2026-09-15. The directory dump `roms/ez2dj1stse/ez2dj/ez2dj.exe` (561,152 bytes) and `ez2dj/Ez2DJ.exe` inside `roms/ez2dj1stse/ez2dj1stse.chd` (634,880 bytes) share the PE TimeDateStamp `0x3862df27` but are different files: the former is the eight-section `.gtide`/`.gdata`/`.gidata` arrangement, the latter a six-section `.protect` arrangement. Every value from 1.1 onward is measured on the directory dump — the `.gtide` build.*

| 항목 / item | `.gtide` 빌드 (디렉터리) | `.protect` 빌드 (CHD) |
| --- | --- | --- |
| 크기 / size | 561,152 | 634,880 |
| TimeDateStamp | `0x3862df27` | `0x3862df27` |
| entry point RVA | `0x01ad23cf` | `0x01ad1240` |
| SizeOfImage | `0x01ada000` | `0x01aec000` |
| 섹션 수 / sections | 8 | 6 |
| 보호 섹션 / protection sections | `.gtide` `.gdata` `.gidata` | `.protect` |
| import directory RVA | `0x01ad8000` (`.gidata`) | `0x01aebbd0` (`.protect`) |
| base relocation directory | `{0, 0}` | RVA `0x01ad2000` |

**확인됨 — 2026-09-15.** 두 빌드의 `.text`·`.rdata`·`.data`·`.reloc` 섹션은 VA·VSize·raw offset·raw size가 모두 같지만 **내용은 다르다.** 반면 `.idata`(raw `0x69000`, 4,096바이트)는 두 빌드에서 바이트 단위로 같다. 즉 packer는 원본 import table 섹션을 그대로 두고 본체 섹션만 변환하며, 두 packer 계열이 서로 다른 변환을 적용한다.

*Confirmed — 2026-09-15. The `.text`, `.rdata`, `.data` and `.reloc` sections of the two builds agree on VA, VSize, raw offset and raw size but **differ in content**, while `.idata` (raw `0x69000`, 4,096 bytes) is byte-identical between them. The packer leaves the original import-table section alone and transforms only the body sections, and the two packer families apply different transforms.*

**미확정.** 같은 게임 빌드에 두 보호 계열이 적용된 이유와 시점, 그리고 `.protect` 빌드의 Hardlock 계약이 `.gtide` 빌드의 LPTDI 계약과 같은지는 확인되지 않았다. 상세는 [1st SE CHD 파일시스템 분석](ez2dj1stse-chd-filesystem.md)에 있다.

*Unresolved: why and when two protector families were applied to the same game build, and whether the `.protect` build's Hardlock contract matches the `.gtide` build's LPTDI contract. Details are in the [1st SE CHD filesystem analysis](ez2dj1stse-chd-filesystem.md).*

### 1.1 헤더와 섹션 — 확인됨

entry point RVA `0x01ad23cf`는 마지막 코드 섹션 `.gtide` 안에 있고, SizeOfImage는 `0x01ada000`이다. import directory는 `.gidata`(RVA `0x01ad8000`)로 옮겨져 있고 IAT directory도 `0x01ad80a0`에 있다. 원본 import table을 담은 `.idata`(RVA `0x01aba000`, 크기 `0x00000fa4`) 섹션은 그대로 남아 있다. base relocation data directory는 `{RVA 0, Size 0}`으로 비어 있으므로 선호 주소 `0x00400000`에 고정 적재해야 한다.

*The entry RVA 0x01ad23cf lies in the last code section `.gtide`; SizeOfImage is 0x01ada000; the import directory moved into `.gidata` (RVA 0x01ad8000) with the IAT directory at 0x01ad80a0. The `.idata` section holding the original import table (RVA 0x01aba000, size 0x00000fa4) is still present. The base-relocation data directory is `{RVA 0, Size 0}`, so the image must load at its preferred base 0x00400000.*

| 섹션 | VA | VSize | Raw Off | Raw Size | Flags |
| --- | --- | --- | --- | --- | --- |
| `.text` | `0x00001000` | `0x00052540` | `0x00001000` | `0x00053000` | code, exec, read |
| `.rdata` | `0x00054000` | `0x00007571` | `0x00054000` | `0x00008000` | data, read |
| `.data` | `0x0005c000` | `0x01a5d2f8` | `0x0005c000` | `0x0000d000` | data, read, write |
| `.idata` | `0x01aba000` | `0x00000fa4` | `0x00069000` | `0x00001000` | data, read, write |
| `.reloc` | `0x01abb000` | `0x00015094` | `0x0006a000` | `0x00016000` | discardable |
| `.gtide` | `0x01ad1000` | `0x0000596e` | `0x00080000` | `0x00006000` | code, exec, read |
| `.gdata` | `0x01ad7000` | `0x00000c00` | `0x00086000` | `0x00001000` | data, read, write |
| `.gidata` | `0x01ad8000` | `0x00001100` | `0x00087000` | `0x00002000` | data, read, write |

앞의 다섯 섹션 `.text`·`.rdata`·`.data`·`.idata`·`.reloc`가 원본 이미지 레이아웃이고, 보호 계층은 그 뒤에 `.gtide`(코드 스텁), `.gdata`(보호 데이터), `.gidata`(import 재배치) 셋을 덧붙인다. 원본 섹션의 VA와 크기는 보호 처리에서 바뀌지 않으므로, `.text`의 RVA 기준 주소는 보호 전후가 같다.

*The first five sections — `.text`, `.rdata`, `.data`, `.idata`, `.reloc` — are the original image layout, and the protection appends `.gtide` (stub code), `.gdata` (protection data), and `.gidata` (relocated imports) behind them. Protection does not change the VA or size of the original sections, so an RVA-based address in `.text` means the same thing before and after protection.*

### 1.2 `.gidata` — import 재배치 — 확인됨

**확인됨 — 2026-09-14 재측정.** import directory(RVA `0x01ad8000`)와 IAT(RVA `0x01ad80a0`)를 직접 해석하면 7 DLL / 161 함수다. 원본 `.idata`(RVA `0x01aba000`)를 같은 방식으로 해석하면 7 DLL / 144 함수이므로, 보호 계층이 더한 것은 정확히 17개다. DLL별로는 KERNEL32 97→113, USER32 21→22이고 GDI32 14, WINMM 8, DDRAW 2, DSOUND 1(ordinal `#1`), ADVAPI32 1(`RegFlushKey`)은 변하지 않는다. 추가 17개 목록과 슬롯 VA는 [import 표면 분석](ez2dj-import-surface.md) 9절에 있다. 런타임에 관찰된 동적 해석은 `GetProcAddress(wsock32, "WSAGetLastError")` 하나뿐이다.

*Confirmed — re-measured 2026-09-14. Parsing the import directory (RVA `0x01ad8000`) and IAT (RVA `0x01ad80a0`) directly yields 7 DLLs / 161 functions. Parsing the original `.idata` (RVA `0x01aba000`) the same way yields 7 DLLs / 144, so the protection layer adds exactly 17. Per DLL, KERNEL32 goes 97→113 and USER32 21→22, while GDI32 (14), WINMM (8), DDRAW (2), DSOUND (ordinal `#1`), and ADVAPI32 (`RegFlushKey`) are unchanged. The 17 additions and their slot VAs are in section 9 of the [import surface analysis](ez2dj-import-surface.md). The only dynamic resolution observed at runtime is `GetProcAddress(wsock32, "WSAGetLastError")`.*


### 1.3 `.gtide` — 보호 스텁 해부 — 확인됨

정적 덤프(파일 오프셋 `0x80000` 기준)에서 다음이 확인됐다.

*From the static dump (file offset 0x80000 base):*

1. **안티디스어셈블 점프.** `eb 01 e8`, `eb 03` 같은 짧은 `jmp`가 코드 전반에 깔려 직후의 쓰레기 바이트(`e8` 등)를 건너뛴다. 선형 디스어셈블러를 무너뜨리는 패턴이다.
2. **XOR 복호화 루프.** entry `0x01ed23cf` 직후와 여러 함수에서 `mov cl,[eax+reg]; xor cl,[ebp-key]; mov [eax+reg],cl; jmp back` 형태의 바이트 단위 복호화 루프가 보인다.
3. **정적으로 일관된 헬퍼 하나.** `0x01ed2504` 근처의 함수는 정적으로 온전히 해석된다: 인자가 `-1`이면 `-1` 반환, `_lread`(IAT 슬롯 `0x01ed8228`)로 10바이트 읽기, 바이트 XOR 루프, `0x646c6f47`(`"Gold"`)과 `0x6f736e65`(`"enso"`) 8바이트 매직 비교, 결과를 `[ebp+0x10]`에 기록.
4. **자기 수정 확인.** 런타임 caller 주소들(`0x01ed2582`, `0x01ed2599`, `0x01ed25b7`, `0x01ed25c9`)의 정적 opcode는 관찰된 호출(`GetVersion`, `LoadLibraryA`, `GetProcAddress`, `FreeLibrary`)과 어긋난다. 실행 시점의 `.gtide` 바이트는 파일과 다르다.

*Anti-disassembly short jumps skip junk bytes throughout; XOR byte-decrypt loops appear at the entry and in several functions; one helper near 0x01ed2504 parses coherently (arg −1 early return, `_lread` of 10 bytes via IAT slot 0x01ed8228, XOR loop, 8-byte magic compare against "Gold"/"enso"); and the runtime caller addresses decode to different calls than the static bytes, confirming self-modification.*

**확인됨.** TLS data directory는 없다. 따라서 진입 전 TLS callback이 `.gtide`를 고친 가능성은 배제된다.

*Confirmed: there is no TLS data directory, so pre-entry TLS-callback modification is excluded.*

**미확정.** `.gtide` 섹션 플래그는 write 비트가 없는데도 런타임 바이트가 바뀌었다. 수정 경로(직접 쓰기가 허용된 환경 요인인지, 관찰 시작 이전 수정인지, 다른 우회인지)는 확인되지 않았다. 확인 방법: entry 직전 정지 시점에 `.gtide` 체크섬을 파일과 비교하고, 이후 시점마다 재비교.

*Unresolved: `.gtide` carries no write flag yet its runtime bytes changed. The modification route (environment that permits direct writes, modification before observation starts, or another bypass) is unknown; verify by checksumming `.gtide` against the file at the pre-entry stop and re-comparing at later points.*

### 1.4 `.gdata` — 문자열·데이터 인벤토리 — 확인됨

파일 오프셋 기준 `0x869b0`~`0x86aa0` 구간을 직접 읽었다. VA 변환은 `VA = 오프셋 + 0x01E51000`이다.

*Read directly from file offsets 0x869b0–0x86aa0; VA = offset + 0x01E51000.*

| VA | 내용 | 런타임 대조 |
| --- | --- | --- |
| `0x01ed79b0` | `".gdata"` 문자열 (패커 흔적) | — |
| `0x01ed79b8` | `"WSOCK32.DLL"` | `LoadLibraryA` 인자와 **일치** |
| `0x01ed79c4` | `"WSAGetLastError"` | `GetProcAddress` 인자와 **일치** |
| `0x01ed79dc` / `0x01ed79e8` | `"WSOCK32.DLL"` / `"WSAGetLastError"` (두 번째 쌍) | 미관찰 |
| `0x01ed79f0` | `"MSVBVM50.DLL"` | 미관찰 |
| `0x01ed7a06`~`0x01ed7a17` | 비ASCII 메시지 바이트 (EUC-KR 추정) | 미관찰 |
| `0x01ed7a18` | `"MSVBVM50.DLL"` (두 번째) | 미관찰 |
| `0x01ed7a2c` | `".gdata"` 문자열 | — |
| `0x01ed7a34` | `"\\.\TDSD.VXD"` | 미관찰 |
| `0x01ed7a44` | dword `1` | — |
| `0x01ed7a4c` | `"\\.\LPTDI0"` | **같은 주소가 런타임에 `"\\.\LPTDI1"`로 관찰됨** |
| `0x01ed7a5c`~`0x01ed7a87` 부근 | 해시성 blob 바이트들 | 미관찰 |
| `0x01ed7a9c` | dword `0xc0e92228` | — |

**확인됨.** `0x01ed7a4c`의 문자열은 파일에는 `\\.\LPTDI0`인데, `--api-trace` 실행에서 같은 주소를 가리키는 `CreateFileA` 첫 인자가 `\\.\LPTDI1`로 디코딩됐다(JSON sanitizing으로 `\`가 `.`로 표기된 `....LPTDI1`). 보호 코드가 포트 번호 자리를 실행 중에 바꿔 쓴다는 뜻이다. `\\.\TDSD.VXD`·`\\.\LPTDI*`·덤프 루트의 비활성 `Tdsd.vxd111`이 함께 보호·I/O 검사 장치 후보다.

*Confirmed: the string at 0x01ed7a4c is `\\.\LPTDI0` in the file, but the `CreateFileA` first argument pointing at the same address decoded as `\\.\LPTDI1` in an `--api-trace` run — the protection rewrites the port digit at runtime. Together with `\\.\TDSD.VXD`, the `\\.\LPTDI*` family, and the disabled `Tdsd.vxd111` at the dump root, these are the candidate protection/I-O check devices.*

**추정.** `MSVBVM50.DLL` 문자열 두 쌍과 매직 비교(`"Gold"`/`"enso"`), 해시 blob은 동글 응답 검증이나 라이선스 데이터일 가능성이 있다. 근거는 문자열 구성뿐이며 목적은 실행 관찰로 확인해야 한다.

*Inferred: the MSVBVM50.DLL pairs, the magic compare, and the hash blobs look like dongle-response or license data, but the evidence is the string composition alone.*

### 1.5 관찰된 런타임 흐름 — 확인됨

`--api-trace`와 언로드 종반 single-step(`--api-trace` 확장)으로 관찰한 흐름이다. caller 주소는 실행마다 동일했다. 근거는 [작업 로그 20260823-042](../work-logs/20260823-042-protected-api-observation-trace.md)와 [HDD 레이아웃 분석](ez2dj-hdd-layout.md) 3절이다.

*Observed via `--api-trace` plus the unload-tail single-step extension; caller addresses repeat across runs. Evidence lives in work log 20260823-042 and the HDD layout analysis.*

```mermaid
flowchart TD
    E["entry 0x01ed23cf (.gtide)"] --> DEC["XOR 복호화 루프 / decrypt loops"]
    DEC --> GV1["GetVersion — caller 0x01ed49d9"]
    GV1 --> CF["CreateFileA \\.\LPTDIn — caller 0x01ed41f1"]
    CF -->|개방 실패| GV2["GetVersion — caller 0x01ed2582"]
    CF -->|synthetic 성공| IOCTL["DeviceIoControl ×2<br/>0x9c406410 / 0x9c406414"]
    IOCTL --> GV2
    GV2 --> LL["LoadLibraryA WSOCK32.DLL — caller 0x01ed2599"]
    LL --> GPA["GetProcAddress WSAGetLastError — caller 0x01ed25b7"]
    GPA --> FL["FreeLibrary — caller 0x01ed25c9"]
    FL --> EV["ZwSetEvent 등 시스템 콜 · WOW64 게이트 왕복"]
    EV --> RT["게스트 복귀 (.gtide)"]
    RT --> POP["pop eax/ebx/ecx/edx/edi/esi @0x01ed2730<br/>(스택 식재 블록 → fault 서명 레지스터)"]
    POP --> JMP["leave · jmp [.gdata 0x01ed7010] → 0x01ed3806"]
    JMP -->|개방 실패| RET["ret @0x01ed3833 → private RW page"]
    RET --> FAULT["페이지 데이터 2명령 실행 후 ff ff → 0xC000001D"]
    JMP -->|synthetic 성공| ORIG["원본 entry 0x0043a640"]
```

fault 전에 `VirtualAlloc`·`VirtualProtect` 호출은 관찰되지 않았다. fault 서명은 실행마다 동일하다: `EBX` = fault page base, `ECX=EDX=ESI=EDI` = entry VA `0x01ed23cf`, `EAX` = `0x001affcc`(debugger 없는 실행의 종료 코드와 동일), `EBP` = kernel32 내부 주소, `ESP` = `0x001aff80`. fault page 내용은 `{0x00010000, 0xffffffff, 0x00400000, ntdll 포인터들, ...}` 구조이고, allocation base `0x00200000`의 기존 process heap 안에 있다.

정밀 관찰([작업 로그 20260823-044](../work-logs/20260823-044-protected-fault-path-precision.md)), 복귀 추적([20260823-045](../work-logs/20260823-045-post-gate-resume-trace.md)), 종료 귀속 관찰([20260823-046](../work-logs/20260823-046-teardown-attribution.md))로 다음이 확인됐다.

1. **전환 직전의 시스템 콜은 `ntdll!ZwSetEvent`다.** 샘플 심볼 해석에서 스텁 주소가 `ZwSetEvent+0x0`과 정확히 일치하고, `mov eax,0x7000e; mov edx,&thunk; call edx; ret 8` 패턴이 NtSetEvent의 두 인자(`ret 8`)와 맞다.
2. **fault 시점 보고 컨텍스트는 32비트 모드다.** `cs=0x0023, ds/es/gs/ss=0x002b, fs=0x0053`.
3. **WOW64 게이트 통과 순간 single-step 보고가 끊긴다.** TF가 전환을 살아남지 못하며, 스텁 복귀 주소의 software breakpoint로 복귀를 잡아 TF를 재무장하면 이후 구간을 다시 추적할 수 있다(재무장 가능하게 하여 4회의 게이트 왕복을 모두 포착).
4. **복귀 시점 stack에는 ws2_32/wsock32 프레임이 없다.** `KERNELBASE!FreeLibrary+0x16`과 `ntdll!LdrUnloadDll+0x15d`만 보이므로 ZwSetEvent·힙 파괴 구간은 LdrUnloadDll 자체의 마무리 처리 안에서 실행된다(ws2_32 detach 코드가 아님).
5. **fault 서명 값은 스텁이 식재한 stack 블록이다.** entry VA 참조 탐색이 stack 위 `{LdrUnloadDll+0x166, entry ×5, page base}` 연속 블록(`0x001aff08`~)과 `{page base, entry ×3}` 블록들을 찾았고(20 match 중 15 run), 이는 fault 레지스터(ECX=EDX=ESI=EDI=entry, EBX=page)와 정확히 같은 배치다.
6. **최종 전송은 게스트 코드 자신이 수행한다.** 시스템 콜 왕복 후 `.gtide`로 돌아온 스텁은 `0x01ed2730`에서 `pop eax; pop ebx; pop ecx; pop edx; pop edi; pop esi; leave`로 식재 블록을 그대로 레지스터에 복원하고(fault 서명 완성), `leave` 뒤 `jmp dword [0x01ed7010]`(.gdata 포인터)로 `0x01ed3806`에 도착, 플래그 `[0x01ed7074]` 검사 뒤 `0x01ed3833`의 `ret`으로 private RW page(page base)에 점프한다.
7. **page 내용은 코드가 아니어서 두 명령을 우연 실행한 뒤 죽는다.** page 선두 `{0x00010000, 0xffffffff, image base, ntdll 포인터}` 구조에서 `add [eax],...` 두 명령이 DEP 부재 환경에서 실행되고 `ff ff`에서 #UD가 난다.
8. **LPTDI 개방 성공은 실패 분기를 우회한다.** [작업 48](../work-logs/20260824-048-lptdi-mock-open.md)의 mock-off/on 각 2회 비교에서 off는 실행별 private page(`0x00393004`, `0x0023f004`)의 #UD를 재현했다. on은 `CreateFileA` kernel32 호출 없이 synthetic handle `0xFEED0001`을 받고 IOCTL `0x9c406410`·`0x9c406414`를 호출한 뒤, 두 번 모두 `.gtide`에서 원본 entry `0x0043a640`으로 넘어갔다. 이어 원본 `.text` caller의 `GetVersion`·`VirtualAlloc`·`GetProcAddress`가 관찰됐다.
9. **원본 초기화 AV는 손상된 `.data` initializer slot 호출이다.** [작업 49](../work-logs/20260824-049-original-init-av-attribution.md)의 두 실행에서 execute AV `0x19d521bd`, `EAX=ECX=EDX=0x0045c008`, `[EDX]=0x19d521bd`가 동일했다. stack return `0x0043b688` 주변 runtime bytes는 비보호 동형 빌드의 `0x0043b683: call dword ptr [edx]`와 일치한다. 그 함수는 `[0x0045c008, 0x0045c014)`의 nonzero initializer를 호출한다. 비보호 배열 `{0, 0x0043c600, 0x0044e710, 0}`과 달리 canonical runtime의 `0x0045c000`부터 8 dword는 `{0xb9f5c1dd, 0x69e5f14d, 0x19d521bd, 0xc908172d, 0x79f968ad, 0x29a5b10d, 0xd995e17d, 0x89c8d81d}`로 두 실행에서 동일했다.
10. **두 IOCTL은 실패하고 어떤 출력도 쓰지 않는다.** [작업 50](../work-logs/20260824-050-device-io-control-return-trace.md)의 두 실행에서 `0x9c406410`(input 4, output 8)과 `0x9c406414`(input 24, output 104)는 모두 `EAX=0`, bytes-returned 무변화, input/output buffer 무변화였다. 첫 input 4바이트와 두 번째 input의 nonzero challenge 필드는 실행마다 달랐다. IOCTL code를 CTL_CODE bitfield로 해석하면 vendor device type `0x9c40`, read access, `METHOD_BUFFERED`, 연속 function `0x904`·`0x905`다. 이는 고정 presence query보다 challenge-response protocol을 지지하지만 올바른 output 의미는 미확정이다.
11. **0바이트 성공은 제어 흐름을 바꾸지만 유효한 성공 응답이 아니다.** [작업 51](../work-logs/20260824-051-lptdi-ioctl-zero-success.md)에서 두 IOCTL에 output 무변화, bytes-returned 0, `TRUE`를 반환했다. 두 API-trace 실행은 원본 entry와 initializer AV에 도달하지 않고 WSOCK32 해제 뒤 기존 private-page 종료 choreography로 돌아가 실행별 `0x002d6004`, `0x00209004`에서 #UD가 났다. exit-break 실행도 `0x0038b004`, 종료 코드 `0xc000001d`를 재현했다. BOOL은 보호 분기에 인과적으로 관여하지만 정상 경로에는 실제 response data도 필요하다.
12. **full bytes-returned도 유효 payload를 대체하지 못한다.** [작업 52](../work-logs/20260824-052-lptdi-hasp-response-contract.md)에서 호출 전 output을 유지하고 각각 8/104 bytes와 `TRUE`를 반환했다. 두 실행은 원본 entry 전에 기존 private-page 경로로 돌아가 `0x00310004`, `0x00237004` #UD로 끝났다. 공개 HASP4 `HaspCode`의 4-byte seed→4×16-bit output은 첫 IOCTL shape와 맞지만, classic HASP의 공개 device path `\\.\HASP`와 28-byte call packet은 LPTDI 전체 인터페이스와 다르다. HASP 계열 가능성은 유력한 추정이며 vendor와 payload는 미확정이다.
13. **첫 8바이트 output의 첫 DWORD 소비 위치가 확인됐다.** [작업 53](../work-logs/20260824-053-lptdi-post-ioctl-trace.md)의 full-size preserving trace에서 `0x9c406410`은 세 번 호출됐다. 매 복귀 뒤 `0x01ed4253: cmp dword ptr [ebp-0x70], ebx`가 output 첫 DWORD를 0과 비교한다. 세 번째 시도 뒤 `0x01ed4279: mov eax, dword ptr [ebp-0x70]`가 그 값을 반환하고, 상위 `0x01ed4d0d`, `0x01ed2b85`가 nonzero 여부를 다시 검사한다. 보존된 bytes `f8 0f 0f 77 ...`의 첫 DWORD는 little-endian `0x770f0ff8`로 그대로 전달됐다. 두 512-step 실행의 호출별 주소/바이트 흐름은 112, 112, 394 sample로 동일했다. 이 구간은 첫 DWORD의 zero/nonzero 소비 계약만 확인하며, 8바이트 전체의 HASP code 의미나 정상 response 생성 규칙은 여전히 미확정이다.
14. **첫 IOCTL의 다음 단계 통과값은 첫 DWORD 0이다.** [작업 54](../work-logs/20260824-054-lptdi-external-response-profile.md)의 외부 profile 반복 실행에서 8바이트 zero response는 `0x9c406410`을 한 번만 호출하고 `0x9c406414`로 진행했다. 반면 첫 DWORD 1 response는 `0x9c406410`을 정확히 세 번 호출하고 두 번째 IOCTL 없이 private-page #UD로 끝났다. zero 실행에서 profile에 없는 `0x9c406414`는 `FALSE`를 반환했으며, 이후 원본 `.text`에 도달해 두 번 모두 동일한 initializer execute AV `0x19d521bd`와 손상된 `.data` window를 재현했다. 따라서 첫 DWORD 0은 첫 단계 통과에 충분하지만 전체 보호 해제에는 충분하지 않으며, 다음 미확정 계약은 104바이트 두 번째 output이다.
15. **두 번째 IOCTL은 DWORD0=0일 때 output offset 4~11로 8바이트 상태를 만든다.** [작업 55](../work-logs/20260824-055-lptdi-second-response-consumption.md)의 두 all-zero 실행에서 `0x01ed4dd5`가 DWORD0을 0과 비교한 뒤, `0x01ed4df2`가 offset 4~11을 차례로 읽었다. 각 바이트는 `0x01ed4dfc`에서 두 번째 IOCTL input seed를 두 번 변환해 얻은 8바이트 mask와 XOR되고, `0x01ed4e07`에서 `[0x01ed7bf4]`가 가리키는 상태에 기록됐다. all-zero payload는 고정 AV `0x19d521bd`를 실행별 `0xd3e72bdf`, `0x0c5c6c3c`로 바꾸고 `.data` window도 바꿨으므로 이 8바이트가 `.data` 복원에 인과적으로 관여한다. DWORD0=1 canonical 두 실행은 loop를 건너뛰고 private-page #UD(`0x003d4004`, `0x002fc004`)로 끝났다. 104바이트 중 offset 0과 4~11 이외는 이 경로에서 읽힌 증거가 없으며, 올바른 응답 생성 규칙은 미확정이다.
16. **challenge mask 변환과 8바이트 상태의 결정성이 확인됐다.** [작업 56](../work-logs/20260824-056-lptdi-adaptive-target-state.md)은 `0x01ed4141`의 runtime 명령에서 32비트 변환을 복원했다. 두 번째 input DWORD에 이 변환을 두 번 연속 적용한 little-endian 8바이트가 response offset 4~11과 XOR된다. 적응형 응답으로 target state를 zero로 고정한 두 실행은 seed `0x7cd97507`, `0x5d7f6e64`에 각각 다른 payload를 반환했지만, 모두 같은 AV `0x19d521bd`와 같은 `.data` window를 재현했다. 따라서 실행별 challenge가 `.data`에 주던 변동은 이 8바이트 상태를 통해 전달되며, 다음 미확정 값은 정상 initializer를 만드는 고정 target state다. 이 변환을 공식 HASP/Hardlock 알고리즘으로 식별하지는 않았다.
17. **정상 `.data` 복원 상태는 최소값 `0900000000000000`이다.** [작업 57](../work-logs/20260824-057-lptdi-target-state-inversion.md)의 4096-step trace에서 `0x01ed2bd0`~`0x01ed2bd5`가 상태 첫 DWORD를 `0x01ed7296`에 seed하고, `0x01ed2742`가 매 바이트 같은 변환으로 갱신하며, `0x01ed26c1`~`0x01ed26ce`가 그 하위 바이트를 보호 raw에서 빼는 흐름이 확인됐다. 보호/비보호 첫 64바이트 차이는 초기 하위 바이트 `0x09`의 출력열과 64/64 일치했다. `0900000000000000` 두 실행은 initializer `{0, 0, 0, 0x0043c600, 0x0044e710, 0, 0, 0x0043c730}`을 반복 복원하고 기존 AV 없이 ExitProcess breakpoint에 도달했다. 상위 24비트와 두 번째 DWORD의 필요성은 이 경로에서 관찰되지 않았다. 이 값은 현재 바이너리의 최소 복원 상태이며 물리 동글 key나 공식 vendor 알고리즘으로 확정하지 않는다.
18. **첫 자산 API 전의 다음 경계는 640×480×16 display-mode 실패다.** [작업 58](../work-logs/20260824-058-first-original-asset-api.md)의 host/VFS 비교에서 `RegisterClassA`·`CreateWindowExA`·`ShowWindow`·`UpdateWindow`는 모두 실행됐다. 이후 `0x00437894`가 `SetCurrentDirectoryA("c:\\ez2dj")`를 호출하고, `0x00437cba`가 640×480×16 `DEVMODEA`로 `ChangeDisplaySettingsExA`를 호출했다. 반환 뒤 코드는 성공 0과 restart 1 어느 쪽도 아닌 분기로 `0x0041f257: PostQuitMessage(0)`에 도달했다. 두 실행 모두 파일 API 없이 `ExitProcess` return `0x0043b63f`로 끝났다. VFS 정책과 무관한 표시 초기화 경계이며 정확한 음수 DISP_CHANGE 값은 아직 직접 기록하지 않았다.
19. **Direct3D 3 초기화 HLE 뒤 최초 경계는 port `0x103` input이다.** [작업 61](../work-logs/20260825-061-direct3d3-opengl-hle.md)의 최종 두 실행은 가상 HAL과 논리 surface/device/viewport로 다섯 graphics stage를 전부 통과해 기존 `0x00422f39` AV를 제거했다. 다음 예외는 두 번 모두 `0x00419609`가 port `0x103`을 인자로 `0x00438980`을 호출하고, `0x00438987: in al,dx`를 실행한 지점의 `0xc0000096`이었다. caller는 이어서 `0x104`, `0x105`도 읽도록 작성돼 있다. 이는 첫 3D 초기화 경계가 제거됐음을 확인하지만 port 값의 장치 의미는 확정하지 않는다.
20. **port 호출 계약과 idle HLE 통과가 확인됐다.** [작업 62](../work-logs/20260825-062-legacy-io-port-hle.md)의 정적 확인에서 `0x101`, `0x102`, `0x106` read는 bitwise NOT 뒤 24개 boolean으로 풀리므로 active-low이다. `0x103`~`0x105`는 이전값과 비교되고 세 번째 byte는 modulo-256 delta에 쓰인다. byte output은 `0x100`~`0x103`, `0x106`에 기록된다. 공용 idle state `ff ff 00 00 00 ff`를 사용한 최종 두 실행은 `0x103`~`0x105` read를 처리하고 privileged exception과 `av_access` 없이 같은 `ExitProcess` return `0x00424061`에 도달했다. port의 물리 배선과 button/axis/output 의미는 **미확정**이다.
21. **controlled exit의 실제 원인은 `coin0.wav` KSND load 실패다.** [작업 63](../work-logs/20260825-063-controlled-exit-attribution.md)은 공유 종료 helper `0x00424040`의 EBP frame을 ExitProcess breakpoint에서 읽었다. 최종 두 실행 모두 caller `0x00424813`, format `KSND(ksndLoadSound) : failed to load %s`, detail `coin0.wav`를 기록했고 `av_access`는 없었다. 정적 caller는 `0x00423f70` search-path lookup이 1을 반환할 때 이 종료 경로를 선택한다. HDD에는 `System/Common/coin0.wav`가 실제로 존재한다. search-path 등록 수와 실제 `CreateFileA` 후보가 아직 관찰되지 않았으므로 VFS failure인지 search-path 초기화 failure인지는 **미확정**이다.
22. **KSND search-path는 정상 등록됐고 VFS mount root가 한 단계 위다.** [작업 64](../work-logs/20260825-064-ksnd-search-path-observation.md)의 두 실행은 count 1과 entry `System/Common`을 동일하게 기록했다. 주입 runtime은 이를 `roms/ez2dj1stse/System/Common/coin0.wav`로 host `CreateFileA`에 전달했지만 실제 파일은 `roms/ez2dj1stse/ez2dj/System/Common/coin0.wav`에 있다. 따라서 현재 실패는 search-path 초기화가 아니라 launcher가 dump root를 VFS root로 주입한 데서 생긴 **확인된 mount-root 불일치**다. 수정 뒤 다음 자산/API 경계는 아직 미확정이다.
23. **working-directory mount 수정 뒤 최초 자산 load가 통과하고 다음 null AV가 드러났다.** [작업 65](../work-logs/20260825-065-target-working-directory-vfs-mount.md)는 target profile의 `ez2dj` working directory를 VFS source root로 주입했다. 최종 두 실행 모두 `System/Common/coin0.wav`, `coin1.wav`, `System/WarningMsg/WarningMsg.bmp` host 후보를 기록한 뒤 `0x0042292b`에서 null read AV가 발생했다. 레지스터는 두 번 모두 `ECX=0`, `EIP=0x0042292b`였고 instruction bytes `8b 11 ff 52 44`는 null interface의 vtable slot 호출 형태다. 이 객체의 종류와 생성 실패 원인은 **미확정**이다.
24. **null 객체는 RGB565 DirectDraw texture surface였고, 다음 경계는 DrawPrimitive다.** [작업 66](../work-logs/20260825-066-texture-surface-gdi-hle.md)의 정적 분석은 `0x0042285e`를 `IDirectDraw4::CreateSurface`, descriptor flags `0x00101007`, caps `DDSCAPS_TEXTURE`, `0x0042292b`를 surface `GetDC`로 확인했다. 이어 `ReleaseDC`, `SetColorKey(DDCKEY_SRCBLT)`, IID가 확인된 `IDirect3DTexture2` QueryInterface가 호출된다. surface HLE 뒤 처음 드러난 null Blt slot도 확인된 `DDBLT_COLORFILL`로 구현했다. 최종 두 실행은 두 AV를 모두 제거하고 return `0x0042325f`에서 vtable `+0x70`, 즉 `IDirect3DDevice3::DrawPrimitive` null slot execute AV를 동일하게 기록했다. primitive type 5, vertex type `0x1c4`, count 4이며 정확한 FVF 의미와 vertex 변환은 다음 작업에서 확정한다.
25. **최소 OpenGL draw HLE 뒤 그래픽 null slot 연쇄가 제거됐다.** [작업 67](../work-logs/20260825-067-drawprimitive-opengl-backend.md)은 `0x1c4`를 `D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1`의 32바이트 정점으로 확정하고, 네 정점 triangle strip을 공용 command와 Windows WGL/GLSL backend로 연결했다. 첫 통합 실행은 기존 `0x0042325c` AV를 통과한 뒤 call site `0x00431f2d`, vtable `+0xa0`의 `SetTextureStageState` null slot을 return `0x00431f33`에서 드러냈다. Get/Set state 보존과 post-handoff debug marker 기록을 연결한 최종 두 실행 `20260825-024310-301.jsonl`, `20260825-024347-572.jsonl`은 각각 DrawPrimitive 성공 표식 201회, OpenGL 실패 0회, `av_access` 0회를 기록하고 모두 caller `0x004249f6`, format `ksnd: Cant Load Sound %s`, detail `title.wav` 제어 종료에 도달했다. **확인됨:** 이전 그래픽 AV와 이어진 stage-state AV는 제거됐고 backend draw가 성공을 반환했다. **추정:** OpenGL draw가 의도한 화면과 시각적으로 일치한다. **당시 미확정:** `title.wav` 실패의 검색 경로(작업 68에서 해소), 실제 present 결과, 나머지 texture-stage state의 정확한 shader 의미.
26. **`title.wav`는 검색·parse에 성공하고 DirectSound buffer 생성에서 실패한다.** [작업 68](../work-logs/20260826-068-ksnd-title-load-attribution.md)의 파일명별 재무장 trace에서 최종 두 실행 `20260826-001806-977.jsonl`, `20260826-001915-355.jsonl`은 모두 `title.wav`의 payload 9,438,264바이트를 parse하고 return `0x0042483d`에서 EAX 1을 기록했다. 이어 global DirectSound 객체의 vtable `+0x0c`, 즉 `IDirectSound::CreateSoundBuffer`가 retry 0~9까지 매번 `0x80004001`을 반환하고 output buffer는 null로 남았다. Lock `+0x2c`와 Unlock `+0x4c`에는 도달하지 않았다. **확인됨:** VFS 경로와 WAV parser는 현재 실패 원인이 아니며 `0x80004001`은 `E_NOTIMPL`이다. **미확정:** system DirectSound가 이 descriptor를 거부한 내부 이유와 HLE backend의 정확한 buffer/state 집합. 두 실행의 `av_access`와 OpenGL 실패는 모두 0회다.
27. **SDL3/SDL3_mixer DirectSound HLE 뒤 전체 초기 sound bank가 통과하고 다음 D3D3 AV가 드러났다.** [작업 69](../work-logs/20260826-069-directsound-sdl3-mixer-hle.md)은 `DSOUND.dll` ordinal `#1`을 guest-callable facade로 바꾸고 `0x140e2` static buffer와 `0x140c6` hardware-placement streaming buffer를 가상화했다. trace `20260826-005558-184.jsonl`은 실제 SDL playback device, secondary buffer 121개, Lock/Unlock 각 299회, `title.wav`의 360,448바이트 looping Play와 OpenGL failure 0회를 기록했다. **확인됨:** 기존 `CreateSoundBuffer E_NOTIMPL`과 KSND 종료는 제거됐다. 다음 execute AV는 address 0, return `0x00420276`, call site `0x00420273`의 global `IDirect3D3` vtable `+0x24`이며 Direct3D 3 계약상 `CreateVertexBuffer`다. **미확정:** vertex buffer descriptor와 이어지는 method 집합, 실제 audio/화면의 사용자 청취·시각 정확성.
28. **null 호출 AV 뒤 예외 dispatch 붕괴가 재현됐고 audio 경계 재검증도 동일 결과를 확인했다.** 작업 69 재검증 실행 `20260826-014926-561.jsonl`은 기준 trace와 동일하게 primary 1개, secondary 121개(`0x140e2` 119 + `0x140c6` 2), Lock/Unlock 각 299회, looping Play 1회, OpenGL failure 0회를 기록했다. null `CreateVertexBuffer` 호출 AV가 debugger에서 `DBG_EXCEPTION_NOT_HANDLED`로 guest에 전달된 직후 같은 thread·같은 ESP에서 두 번째 `c0000005`(address `0xfaa77401`, 미커밋 region)가 발생하고 process 전체가 `0xc0000005`로 종료한다. 이 패턴은 두 실행에서 결정적으로 동일하다. **확인됨:** audio HLE 자체의 실패나 퇴행은 없으며, process 종료는 다음 graphics 경계 AV가 처리되지 않을 때의 dispatch 후속 붕괴다. **추정:** 두 번째 AV는 guest SEH/WOW64 exception dispatch가 null-call 뒤 회복 불가능한 상태에 진입한 결과다.
29. **`CreateVertexBuffer` 통과 뒤 게스트 정점 루프와 Lock 불명 모순이 관찰됐다.** [작업 70](../work-logs/20260826-070-direct3d3-vertex-buffer-hle.md)의 실행 `20260826-022620-578.jsonl`은 marker `caps=0x00000000:fvf=0x00000112:vertices=121:flags=0x00000000`로 첫 정점 버퍼 생성을 기록했다. FVF `0x112`=XYZ|NORMAL|TEX1이므로 **untransformed 파이프라인 사용이 확인됐고** stride 32가 유도된다. `.text`(RVA=파일 오프셋)의 `0x00420230`–`0x00420390` 해독: descriptor `{16,0,0x112,0x79}`를 stack에 구성하고 global `[0x01eb7ce0]`으로 `CreateVertexBuffer(&desc, out=[ebp+8], 0, 0)`을 호출한 뒤 반환 객체로 `Lock(vb, 1, &[ebp-0x20], NULL)` 형태의 `call [vtbl+0xc]`(`0x0042028b`)이 이어지며, 11×11 이중 루프(i,j∈0..10)가 `idx=i+j*11`과 `shl 5`로 stride-32 정점을 `[data+idx*32+{0,4,8(0),0xc(0)}]`에 기록한다. 첫 실패는 `0x00420353` `mov [ecx+eax],edx`(ecx=`[ebp-0x20]`, address `0x001b013c`, index 67)이고 이후 711회 execute@0 AV로 process가 붕괴했다. **확인됨:** 위 marker·해독·AV 사실과 `IDirect3DVertexBuffer` marker 부재. **모순/미확정:** 모든 VbLock 경로는 `*data`를 반드시 기록하므로 `[ebp-0x20]`이 스택 잔재 `0x001af8dc`(ebp−0xC)였다는 것은 VbLock이 실제로 호출되지 않았음을 시사하지만 원인은 불명이다. 다음 단계는 VbLock 진입 계측(self/vtable/data 포인터 기록)이다.

30. **확인됨 — 작업 70 완료.** Lock 미호출 모순의 원인은 facade가 원본의 `lpdwSize=nullptr`을 marker 전에 거절한 것이었다. 수정 후 final trace `20260826-104802-472.jsonl`, `20260826-104944-099.jsonl`에서 반환 facade와 Lock self/vtable이 각각 일치했고, flags 1의 null-size Lock은 3,872바이트(121×32)를 반환한 뒤 Unlock됐다. 두 실행 모두 `0x00420353`, `av_access`, OpenGL failure가 0회이며 DrawPrimitive 2,961회/3,855회 뒤 caller `0x00424f68`, `KSnd(ksndDuplicate) : Error on duplicate`로 제어 종료했다. 따라서 정점 buffer 경계는 회복됐고 다음 확인 대상은 DirectSound duplicate다.

31. **확인됨 — 작업 71.** Microsoft 계약에 따라 PCM storage를 공유하고 cursor/control/Play 상태와 SDL voice를 분리한 `DuplicateSoundBuffer` HLE 뒤 final trace `20260826-110206-895.jsonl`, `20260826-110358-397.jsonl`은 secondary buffer 126개씩, duplicate 70회/47회, Play 84회/60회, DrawPrimitive 37,937회/36,111회를 기록했다. 두 실행 모두 `av_access`, OpenGL failure와 SDL error가 0회이고 `KSnd(ksndDuplicate)` 제어 종료도 사라졌다. **확인됨:** 원본은 관찰 시간 동안 새 안정 실패 경계 없이 메인 루프를 유지했으며 증거 확보 후 수동 종료했다. 실제 화면·오디오·입력 정확성은 사용자 관찰이 필요한 미확정 항목이다.

결론: 종료는 우연한 손상이 아니라 **LPTDI 보호 응답에 따라 스텁이 선택하는 계획된 실패 경로**다. 개방 성공과 host IOCTL 실패 조합은 손상된 `.data` initializer slot AV를 일으키고, IOCTL TRUE와 빈 output 조합은 private-page #UD로 돌아간다. 실행별 challenge mask를 상쇄해 최소 target state `0900000000000000`을 만들면 정상 `.data` initializer가 복원되고 기존 AV가 제거된다. 물리 동글의 원래 wire-response 알고리즘과 vendor 귀속은 여전히 미확정이다.

*Precision observation, resume tracing, and teardown attribution confirmed seven facts: (1) the pre-transition syscall is exactly ntdll!ZwSetEvent; (2) the fault-time synthesized context reports 32-bit user segments; (3) single-step reporting dies at each gate but a software breakpoint on a detected stub's return address re-arms tracing — with repeatable arming all four gate crossings were caught; (4) the resume-time stack holds KERNELBASE!FreeLibrary+0x16 and ntdll!LdrUnloadDll+0x15d with no ws2_32/wsock32 frames, so the event-signal and heap-destroy stretch runs inside LdrUnloadDll's own finalization rather than ws2_32 detach code; (5) the fault-signature values live in stub-planted stack blocks — the entry scan found {LdrUnloadDll+0x166, entry×5, page-base} and {page-base, entry×3} runs matching the register layout exactly; (6) after returning into .gtide the stub itself executes pop eax/ebx/ecx/edx/edi/esi + leave at 0x01ed2730, restoring precisely that signature, then jmp through its .gdata pointer table to 0x01ed3806 and finally ret at 0x01ed3833 onto the private RW page; and (7) the page data is not code — two accidental add instructions execute before ff ff raises #UD.*

*Task 48 adds an eighth confirmed fact: in two matched pairs, mock-off reproduced #UD on run-varying private pages, while mock-on intercepted CreateFileA, passed synthetic handle 0xFEED0001 through IOCTLs 0x9c406410 and 0x9c406414, and reached original entry 0x0043a640 in both runs. Original .text then called GetVersion, VirtualAlloc, and GetProcAddress before a stable execute access violation at 0x19d521bd. Failed LPTDI open is therefore causally responsible for selecting the private-page failure path; the narrower "skipped decryption" mechanism remains inferred because success bypasses that page rather than filling it.*

*Task 49 confirms the later AV as an indirect call through a corrupt `.data` initializer slot. Both runs report execute AV 0x19d521bd with EAX=ECX=EDX=0x0045c008 and `[EDX]=0x19d521bd`; runtime code around return 0x0043b688 matches `call dword ptr [edx]` at 0x0043b683 in the sibling unprotected build. The eight-dword canonical runtime window is stable but differs completely from the unprotected initializer array. The limited `.text` call site is restored while this `.data` region is not.*

*Task 50 confirms that both IOCTLs fail without writing any output. Across two runs, 0x9c406410 (input 4/output 8) and 0x9c406414 (input 24/output 104) return EAX zero with unchanged bytes-returned and unchanged buffers. Challenge input fields vary by run. CTL_CODE decoding gives vendor device type 0x9c40, read access, METHOD_BUFFERED, and consecutive functions 0x904/0x905. This supports a challenge-response protocol rather than a fixed presence query, while the correct output semantics remain unresolved.*

*Task 51 confirms that zero-byte success changes control flow but is not a valid success response. Returning TRUE, zero bytes, and unchanged output for both IOCTLs prevented both canonical runs from reaching the original entry or the later initializer AV. After WSOCK32 unload they instead returned to the existing private-page teardown choreography and raised #UD at per-run addresses 0x002d6004 and 0x00209004; an exit-break run independently ended with 0xc000001d at 0x0038b004. The BOOL is causally relevant, but valid response data is also required.*

*Task 52 confirms that full bytes-returned cannot replace valid payload. Preserving the pre-call output while returning TRUE and 8/104 bytes sent both runs back to the pre-original-entry private-page path, ending in #UD at 0x00310004 and 0x00237004. Public HASP4 HaspCode's four-byte seed to four 16-bit values matches the first IOCTL shape, but classic HASP's published `\\.\HASP` path and 28-byte call packet differ from the complete LPTDI interface. HASP remains a strong inference; the vendor and payload are unresolved.*

*Task 53 confirms where the first DWORD of the eight-byte output is consumed. Full-size-preserving traces call 0x9c406410 three times. After each return, `0x01ed4253: cmp dword ptr [ebp-0x70], ebx` compares the first output DWORD with zero. After the third attempt, `0x01ed4279: mov eax, dword ptr [ebp-0x70]` returns it, and callers at 0x01ed4d0d and 0x01ed2b85 test it for nonzero again. The preserved bytes begin with little-endian 0x770f0ff8 and propagate unchanged. Two 512-step runs produced identical per-call address/byte trails of 112, 112, and 394 samples. This establishes only a first-DWORD zero/nonzero consumption contract in this stage; the meaning of all eight bytes and the valid response algorithm remain unresolved.*

*Task 54 confirms that zero is the first IOCTL's advance value. In repeated external-profile runs, an eight-byte zero response called 0x9c406410 once and advanced to 0x9c406414. A first-DWORD-one response called 0x9c406410 exactly three times and ended in private-page #UD without reaching the second IOCTL. The absent 0x9c406414 profile entry returned FALSE on zero runs; execution then reached original `.text` and reproduced the identical initializer execute AV at 0x19d521bd and the same corrupt `.data` window twice. First-DWORD zero is therefore sufficient to pass the first stage but not the complete protection path; the next unresolved contract is the 104-byte second output.*

*Task 55 confirms that the second IOCTL builds an eight-byte state from output offsets 4 through 11 when DWORD0 is zero. In two all-zero runs, 0x01ed4dd5 compared DWORD0 with zero, 0x01ed4df2 read offsets 4 through 11 in order, 0x01ed4dfc XORed each byte with an eight-byte mask obtained by transforming the second-IOCTL input seed twice, and 0x01ed4e07 wrote the result through the pointer at [0x01ed7bf4]. The all-zero payload changed the stable 0x19d521bd AV to per-run addresses 0xd3e72bdf and 0x0c5c6c3c and changed the `.data` window, causally connecting these eight bytes to `.data` restoration. Two canonical DWORD0-one runs skipped the loop and ended in private-page #UD at 0x003d4004 and 0x002fc004. No read of the other 92 bytes was observed on this path, and the valid response rule remains unresolved.*

*Task 56 confirms the challenge-mask transform and determinism of the eight-byte state. Runtime instructions at 0x01ed4141 apply a reconstructed 32-bit transform twice to the second input DWORD; the resulting eight little-endian bytes are XORed with response offsets 4 through 11. Adaptive zero-target-state runs returned different payloads for seeds 0x7cd97507 and 0x5d7f6e64, yet both reproduced the same AV at 0x19d521bd and the same `.data` window. Per-run challenge variation therefore reaches `.data` through this eight-byte state, leaving the fixed target state that restores the normal initializer as the next unresolved value. The transform is not identified as an official HASP or Hardlock algorithm.*

*Task 57 confirms minimal normal-restoration state `0900000000000000`. The 4096-step trace seeds its first DWORD into `0x01ed7296` at 0x01ed2bd0–0x01ed2bd5, advances it once per byte at 0x01ed2742 with the same transform, and subtracts its low byte from protected raw data at 0x01ed26c1–0x01ed26ce. The first 64 protected/unprotected byte differences match all 64 generated bytes for initial low byte 0x09. Two adaptive runs restored initializer `{0, 0, 0, 0x0043c600, 0x0044e710, 0, 0, 0x0043c730}` and reached the ExitProcess breakpoint without the old AV. The upper 24 bits and second DWORD were not observed as required. This is the minimal restoration state for this binary, not confirmation of a physical-dongle key or official vendor algorithm.*

*Task 58 identifies the next pre-asset boundary as display-mode failure. Host and VFS runs both execute RegisterClassA, CreateWindowExA, ShowWindow, and UpdateWindow, then call SetCurrentDirectoryA("c:\\ez2dj") at 0x00437894 and ChangeDisplaySettingsExA at 0x00437cba with a 640×480×16 DEVMODEA. The executed return branch is neither success zero nor restart one and reaches PostQuitMessage(0) at 0x0041f257. Both runs exit through return 0x0043b63f without a file API. This is display startup independent of VFS policy; the exact negative DISP_CHANGE value has not yet been directly captured.*

*Task 61 confirms port 0x103 input as the next boundary after Direct3D 3 initialization HLE. Two final runs pass all five graphics stages with a virtual HAL and logical surfaces, device, and viewport, eliminating the old AV at 0x00422f39. Both next raise 0xc0000096 when caller 0x00419609 invokes helper 0x00438980 and executes `in al,dx` at 0x00438987 for port 0x103. The caller is written to read 0x104 and 0x105 next. This confirms removal of the first 3D boundary but does not identify the device or port-value semantics.*

*Task 62 confirms the port contract and idle-HLE passage. Reads from 0x101, 0x102, and 0x106 are inverted and expanded into 24 booleans, confirming active-low inputs. Ports 0x103 through 0x105 are compared with previous bytes, with the third used as a modulo-256 delta. Byte writes target 0x100 through 0x103 and 0x106. Two final runs with shared idle state `ff ff 00 00 00 ff` handle the three initial counter reads and reach the same ExitProcess return 0x00424061 without a privileged exception or `av_access`. Physical wiring and button, axis, and output meanings remain unresolved.*

*Task 63 attributes the controlled exit to a KSND `coin0.wav` load failure. Reading the shared helper's EBP frame at the ExitProcess breakpoint yields caller 0x00424813, format `KSND(ksndLoadSound) : failed to load %s`, and detail `coin0.wav` in both final runs, with no access violation. The static caller selects this path when search-path lookup 0x00423f70 returns one, while the HDD actually contains `System/Common/coin0.wav`. Search-path count and concrete CreateFile candidates remain unobserved, so VFS failure versus search-path initialization failure is unresolved.*

*Task 64 confirms that KSND registration is present and the VFS mount root is one directory too high. Both runs record count one and entry `System/Common`. The injected runtime passes `roms/ez2dj1stse/System/Common/coin0.wav` to host CreateFileA, while the actual file is under `roms/ez2dj1stse/ez2dj/System/Common/coin0.wav`. The current failure is therefore a confirmed mount-root mismatch caused by injecting the dump root, not missing KSND search initialization. The next boundary after correction remains unresolved.*

*Task 65 injects the target profile's `ez2dj` working directory as the VFS source root. Both final runs reach host candidates for `System/Common/coin0.wav`, `coin1.wav`, and `System/WarningMsg/WarningMsg.bmp`, then fail with a null read AV at 0x0042292b. Both contexts have ECX zero; instruction bytes `8b 11 ff 52 44` form a vtable-slot call through a null interface. The object type and reason its creation failed remain unresolved.*

*Task 66 identifies the null object as an RGB565 DirectDraw texture surface. Static evidence maps 0x0042285e to IDirectDraw4::CreateSurface with flags 0x00101007 and DDSCAPS_TEXTURE, followed by GetDC at 0x0042292b, ReleaseDC, SetColorKey(DDCKEY_SRCBLT), and a confirmed IDirect3DTexture2 QueryInterface. The surface HLE also handles the subsequently exposed DDBLT_COLORFILL null slot. Two final runs remove both AVs and reproduce the next execute AV at return 0x0042325f: null IDirect3DDevice3::DrawPrimitive vtable slot +0x70. Observed arguments are primitive type 5, vertex type 0x1c4, and count 4; exact FVF meaning and vertex translation remain for the next task.*

*Task 67 confirms FVF 0x1c4 as a 32-byte D3DFVF_XYZRHW | DIFFUSE | SPECULAR | TEX1 vertex and routes the four-vertex triangle strip through a neutral command and Windows WGL/GLSL backend. The first integrated run passes the old 0x0042325c AV and exposes SetTextureStageState at call site 0x00431f2d, vtable +0xa0, returning to 0x00431f33. After symmetric state retention and post-handoff debug-marker recording are connected, final logs 20260825-024310-301.jsonl and 20260825-024347-572.jsonl each record 201 DrawPrimitive success markers, zero OpenGL failures, and zero av_access events before the same controlled exit from caller 0x004249f6 with `ksnd: Cant Load Sound %s` and `title.wav`. Confirmed: the prior graphics and subsequent state-slot AVs are removed and backend draws return success. Inferred: rendered output visually matches intent. Unresolved at that time: the title.wav lookup failure (resolved in Task 68), observed presentation, and exact shader semantics of remaining texture-stage states.*

*Task 68 confirms that title.wav succeeds through lookup and parsing, then fails at DirectSound buffer creation. In final logs 20260826-001806-977.jsonl and 20260826-001915-355.jsonl, the rearming filename-aware trace parses the same 9,438,264-byte payload and records EAX one at return 0x0042483d. The global DirectSound object's vtable +0x0c IDirectSound::CreateSoundBuffer then returns 0x80004001 for retries zero through nine and leaves the output buffer null. Lock +0x2c and Unlock +0x4c are never reached. Confirmed: VFS path resolution and WAV parsing are not the current failure, and 0x80004001 is E_NOTIMPL. Unresolved: why system DirectSound rejects this descriptor and the exact buffer/state set for an HLE backend. Both runs contain zero av_access and zero OpenGL failure events.*

*Task 69 replaces DSOUND ordinal 1 with an SDL3/SDL3_mixer-backed guest COM facade and virtualizes observed 0x140e2 static and 0x140c6 hardware-placement streaming buffers. Trace 20260826-005558-184.jsonl records a real SDL playback device, 121 secondary buffers, 299 Lock/Unlock pairs, looping playback of a 360,448-byte title.wav buffer, and zero OpenGL failures. Confirmed: the former CreateSoundBuffer E_NOTIMPL and KSND exit are removed. The next execute AV is address zero returning to 0x00420276, from global IDirect3D3 vtable slot +0x24 CreateVertexBuffer at call site 0x00420273. Unresolved: the vertex-buffer descriptor and following method set, and user-observed audio/visual accuracy.*

*Re-verification item 28: rerun trace 20260826-014926-561.jsonl reproduces the baseline exactly — one primary buffer, 121 secondary buffers (119 static 0x140e2 plus 2 streaming 0x140c6), 299 Lock/Unlock pairs, one looping Play, and zero OpenGL failures. After the null CreateVertexBuffer call AV is delivered to the guest with DBG_EXCEPTION_NOT_HANDLED, a second c0000005 at address 0xfaa77401 in an uncommitted region fires on the same thread with the same ESP and the whole process exits 0xc0000005; both runs show this deterministically. Confirmed: no audio HLE failure or regression — process death is post-dispatch collapse behind the unhandled next graphics boundary. Inferred: the second AV is guest SEH/WOW64 dispatch entering an unrecoverable state after the null call.*

*Task 70 item 29: run 20260826-022620-578.jsonl records the first successful vertex-buffer creation with marker caps=0, fvf=0x112, vertices=121, flags=0. FVF 0x112 (XYZ|NORMAL|TEX1) confirms the untransformed pipeline and implies stride 32. Decoding .text 0x00420230–0x00420390 (RVA equals file offset) shows the guest building descriptor {16,0,0x112,0x79} on its stack, calling CreateVertexBuffer(&desc, out=[ebp+8], 0, 0) through global [0x01eb7ce0], immediately issuing call [vtbl+0xc] shaped as Lock(vb, 1, &[ebp-0x20], NULL), then an 11×11 double loop storing stride-32 vertices at [data + idx*32 + {0,4,8(0),0xc(0)}] with idx=i+j*11. The first failure is the write at 0x00420353 (ecx=[ebp-0x20], address 0x001b013c, index 67), followed by 711 execute-at-zero AVs collapsing the process. Confirmed: the marker facts, the decode, the AV sequence, and the total absence of IDirect3DVertexBuffer markers. Contradiction/unresolved: every VbLock path writes *data, yet the observed local kept stack residue 0x001af8dc (ebp−0xC), implying VbLock was never actually invoked — cause unknown. Next step: entry-point instrumentation of VbLock recording self, vtable, data, and size pointers.*

*Confirmed — Task 70 completion. The apparent missing Lock was the facade rejecting the original null `lpdwSize` before its marker. After the fix, final traces 20260826-104802-472.jsonl and 20260826-104944-099.jsonl show matching returned facade and Lock self/vtable pointers, a successful flags-one null-size Lock returning 3,872 bytes (121×32), and Unlock. Both have zero 0x00420353 events, access violations, and OpenGL failures, followed by 2,961/3,855 DrawPrimitive calls and the same controlled exit at caller 0x00424f68 with `KSnd(ksndDuplicate) : Error on duplicate`. The vertex-buffer boundary is recovered; DirectSound duplication is next.*

*Confirmed — Task 71. After implementing the Microsoft duplication contract with shared PCM plus independent cursor/control/Play state and SDL voices, final traces 20260826-110206-895.jsonl and 20260826-110358-397.jsonl record 126 secondary buffers each, 70/47 duplicates, 84/60 Play calls, and 37,937/36,111 DrawPrimitive calls. Both have zero access violations, OpenGL failures, and SDL errors; the former KSND duplicate exit is gone. The original remains in its main loop for the observation period and was stopped manually after evidence collection. Visual, audible, and input accuracy still require user observation.*

32. **확인됨 — 작업 072의 원본 fixed-function state.** bounded runtime trace는 stage 0 `COLOROP=MODULATE`, `COLORARG1=TEXTURE`, `COLORARG2=DIFFUSE`, min/mag linear filter, `COLORKEYENABLE=1`, alpha test `NOTEQUAL`/reference 0, alpha blend enable과 `SRCBLEND/DESTBLEND`의 `ZERO/SRCALPHA`·`ONE/ZERO` 전환을 기록했다. 기존 backend는 이 state를 저장만 하고 nearest·overwrite·float color-key discard로 그렸으므로 사용자 관찰의 누락 그림·테두리와 일치하는 구현 결손이었다. surface identity/revision cache, RGB565 key-to-alpha upload와 확인된 fixed-function state 적용 뒤 debugger 회귀 로그 `20260826-184241-943.jsonl`은 OpenGL failure와 AV 0회를 기록했다. **미확정:** 실제 누락 그림과 테두리가 모두 해소됐는지는 사용자 재검증이 필요하다.

33. **확인됨 — debugger I/O 왕복과 detached runtime.** 상세 I/O JSON을 억제해도 debugger mode의 자산 진행 속도는 거의 변하지 않았다. Windows exception 순서상 debugger가 vectored handler보다 first chance를 먼저 받으므로 debugger 유지 자체가 각 `IN`/`OUT`의 왕복 비용이다. `--run-detached`는 원본 entry와 IAT를 검증한 뒤 debugger를 분리하며 injected handler가 RVA `0x38987`의 `IN AL,DX`, RVA `0x389ab`의 `OUT DX,AL`과 기존 허용 port만 처리한다. 로그 `20260826-183749-602.jsonl`은 준비와 detach를 기록했고 원본 process는 40초 동안 유지된 뒤 검증을 위해 강제 종료됐다. 원본 instruction byte는 수정하지 않았다.

*Confirmed — Task 72 fixed-function state. A bounded runtime trace records stage-zero MODULATE with TEXTURE/DIFFUSE arguments, linear min/mag filtering, COLORKEYENABLE, alpha test NOTEQUAL against reference zero, alpha blending, and ZERO/SRCALPHA versus ONE/ZERO blend-factor transitions. The previous nearest, overwrite, float-discard backend ignored these retained states. After per-surface identity/revision caching, RGB565 key-to-alpha upload, and confirmed state translation, debugger regression log 20260826-184241-943.jsonl records zero OpenGL failures and access violations. Whether every missing image and border is visually corrected remains unresolved pending user revalidation.*

*Confirmed — debugger I/O round trips and detached runtime. Suppressing detailed I/O JSON did not materially change debugger-mode asset progress because debugger first-chance delivery still crosses the process boundary for every IN/OUT. `--run-detached` verifies entry and IAT state, detaches, and lets the injected handler accept only IN AL,DX at RVA 0x38987, OUT DX,AL at RVA 0x389ab, and the existing allowed ports. Log 20260826-183749-602.jsonl records preparation and detachment; the original process remained alive for 40 seconds until forcibly stopped for verification. Original instruction bytes are unchanged.*

34. **확인됨 — 작업 073의 offscreen BMP와 2D blit 경로.** 사용자 실행의 WER dump는 `0xc0000005`, fault `0x004088d6`, read address `0x00000008`, `ECX=0`을 기록했다. 명령은 게임 객체 `[eax+0x2c8]`의 null 값을 받아 `[ecx+8]`을 읽으며, 호출자 `0x004085e5`와 객체 `0x016403a4`도 dump에서 확인했다. 생성자 `0x0040a9c4`는 문자열 `1p_meter_back`을 `0x0041ff10`에 넘겨 이 멤버를 채운다. 조회 함수는 `%s.bmp`를 열고 `DDSURFACEDESC2 {dwFlags=7, ddsCaps=0x40}`으로 surface를 만든다. `0x40`은 `DDSCAPS_OFFSCREENPLAIN`이며, 성공 뒤 `GetDC`/GDI copy/`SetColorKey`와 vtable `+0x1c` source-key `BltFast`, `+0x14` source-key `Blt`가 이어진다. crash dump의 동적 surface count `[0x00bb6e68]`은 0이었다. 기존 HLE가 offscreen surface를 `DDERR_UNSUPPORTED`로 거절하고 `BltFast` slot을 비워 둔 것이 누락 그림과 null 역참조의 직접 원인이다. 이를 구현한 로그 `20260826-201731-528.jsonl`은 기존 약 76초 crash 지점을 넘어 120초 동안 응답 상태를 유지했고 검증을 위해 강제 종료됐다. 같은 시간대에 새 WER crash는 없다. **미확정:** 사용자 화면에서 모든 offscreen sprite의 위치와 컬러키 결과가 정확한지는 재검증이 필요하다.

*Confirmed — Task 073 offscreen BMP and 2D blit path. The user's WER dump records `0xc0000005` at `0x004088d6`, reading `0x00000008` with `ECX=0`. The instruction consumes null game-object member `[eax+0x2c8]`; caller `0x004085e5` and object `0x016403a4` are also present in the dump. Constructor `0x0040a9c4` fills the member by passing `1p_meter_back` to `0x0041ff10`. That lookup opens `%s.bmp` and creates a surface from `DDSURFACEDESC2 {dwFlags=7, ddsCaps=0x40}`; `0x40` is `DDSCAPS_OFFSCREENPLAIN`. Success is followed by GetDC/GDI copy/SetColorKey, source-key BltFast at vtable +0x1c, and source-key Blt at +0x14. The dump's dynamic-surface count `[0x00bb6e68]` is zero. The HLE's unsupported offscreen surface and unset BltFast slot directly caused both missing images and the null dereference. After implementing them, log `20260826-201731-528.jsonl` stays responsive for 120 seconds beyond the former roughly 76-second crash point and is then forcibly stopped for verification, with no new WER crash in that interval. User validation of every offscreen sprite position and color-key result remains unresolved.*

35. **확인됨 — Win32 제품 loader 실행과 낮은 체감 음량.** 사용자는 일반 `re2dj --run`으로 보호된 원본이 실행되는 것을 확인했고, 오디오는 출력되지만 지나치게 작다고 관찰했다. 현재 DirectSound buffer 변환식 `10^(volume/2000)`은 1/100 dB 계약과 일치하므로 이를 원인으로 확정할 수 없다. **추정:** 원본이 별도 WINMM mixer API로 조절하던 캐비닛 master volume이 SDL 출력 계층에 대응되지 않은 것이 체감 차이의 한 원인이다. 작업 080은 buffer별 상대 gain을 보존하면서 제품 기본 `+6 dB` host master gain을 추가했다. **미확정:** 실제 캐비닛 음압과 같은 절대 기준, `+6 dB`의 최종 체감·clipping 여부, 원본 WINMM control ID와 값의 정확한 의미.

*Confirmed — Win32 product-loader execution and low perceived volume. The user confirmed that the protected original runs through ordinary `re2dj --run` and produces audio, but perceived output is much too quiet. The current `10^(volume/2000)` DirectSound buffer conversion matches the hundredths-of-a-decibel contract and is not itself a confirmed cause. Inferred: one contributor is the missing SDL equivalent of cabinet master volume, which the original controls through separate WINMM mixer APIs. Task 080 adds a default `+6 dB` host master gain while preserving relative buffer gains. Unresolved: an absolute cabinet loudness reference, perceived level and clipping at `+6 dB`, and exact original WINMM control IDs and values.*

36. **확인됨 — 작은 title 출력의 주원인은 streaming buffer 갱신 누락이다.** 작업 082의 `0 dB` 실제 실행 `20260828-081711-510.audio.log`에서 원본은 WINMM device 0을 열고 speaker destination volume control ID 2(범위 0–65535)에 57,194를 썼으며 호출은 성공했다. 이어 44.1 kHz stereo 16-bit, 360,448바이트 looping DirectSound buffer를 만들고 첫 재생 청크의 peak `0.457763672`, RMS `0.075547613`을 기록했다. 이 값은 사용자 HDD의 `System/Title/title.wav` 첫 360,448 data bytes와 정확히 일치하므로 WAV loader나 HLE 복사 과정의 선행 감쇠는 없다. 첫 청크 RMS는 `-22.44 dBFS`, WAV 전체 RMS는 `-9.29 dBFS`로 `13.14 dB` 차이다. 원본은 재생 뒤 같은 ring buffer를 계속 `Lock/Unlock`하여 5–8번째 관찰 청크에서 peak `0.979309082`, RMS `0.140381544..0.260812569`까지 갱신했지만 trace는 각 갱신에 `backend-refresh=0`을 확인했다. 현재 SDL backend는 `Play` 순간 `MIX_LoadRawAudio`로 첫 snapshot만 만들고 이후 `Unlock` 내용을 갱신하지 않으므로 조용한 도입부 청크를 반복한다. 이는 사용자가 `+12 dB`에서 일반 음량처럼 느낀 관찰과 수치상 일치한다. **확인됨:** WINMM volume 호출 누락과 PCM 변환 감쇠가 이 실행의 주원인은 아니다. **미확정:** `SetVolume(-10000)`의 원본 fade 상태 전이와 모든 gameplay buffer의 상대 음량. 다음 수정은 master gain 확대가 아니라 DirectSound streaming/ring-buffer 동기화여야 한다.

*Confirmed — the main cause of low title output is missing streaming-buffer refresh. In Task 082's real 0 dB run `20260828-081711-510.audio.log`, the original opens WINMM device 0 and successfully writes 57,194 to speaker-destination volume control ID 2 (range 0–65,535). It then creates a 44.1 kHz stereo 16-bit 360,448-byte looping DirectSound buffer whose first-play peak is 0.457763672 and RMS is 0.075547613. These exactly match the first 360,448 data bytes of `System/Title/title.wav` on the user-supplied HDD, ruling out attenuation in WAV loading or the HLE copy. The first chunk is -22.44 dBFS RMS while the complete WAV is -9.29 dBFS RMS, a 13.14 dB difference. After playback begins, the original continuously updates the ring buffer through Lock/Unlock; observed updates five through eight reach peak 0.979309082 and RMS 0.140381544..0.260812569, while every trace reports `backend-refresh=0`. The current SDL backend creates one `MIX_LoadRawAudio` snapshot at Play and never applies later Unlock contents, so it repeats the quiet introduction chunk. This numerically agrees with the user's observation that +12 dB sounds normal. Confirmed: missing WINMM volume calls and PCM conversion attenuation are not the main cause in this run. Unresolved: the original fade transition behind `SetVolume(-10000)` and relative levels across all gameplay buffers. The next fix should implement DirectSound streaming/ring-buffer synchronization instead of increasing master gain.*

37. **확인됨 — 원본 streaming writer는 whole-buffer lock 안에서 45,056바이트 청크를 순환 갱신한다.** 작업 083의 첫 실행 `20260828-151051-585.audio.log`는 원본이 매번 `DSBLOCK_ENTIREBUFFER`로 360,448바이트 전체를 Lock/Unlock하지만 play cursor는 약 44–46KB씩 전진함을 확인했다. Unlock 길이 전체를 SDL stream에 추가한 초기 변환은 queue를 5,448,476바이트까지 증가시켰으므로 실제 write 길이로 사용할 수 없다. committed snapshot과 현재 PCM을 frame 단위로 비교한 최종 실행 `20260828-151817-074.audio.log`는 64회 Unlock 중 초기 no-change 7회를 제외한 57회를 모두 45,056바이트 dirty 구간으로 분리했다. offset은 `0..315392`를 45,056바이트 간격으로 순환하고 queue는 251,160–358,684바이트로 안정됐다. 후속 PCM peak는 약 0.98까지 도달했으며 대응 JSONL의 AV, controlled exit와 OpenGL failure는 0건이다. **확인됨:** stale 첫 snapshot 반복과 무제한 queue 증가는 제거됐다. **미확정:** 다른 게임 버전의 streaming descriptor와 전체 곡·효과음의 사용자 청취 정확성.

*Confirmed — the original streaming writer cyclically updates 45,056-byte chunks inside whole-buffer locks. Task 083's first run, `20260828-151051-585.audio.log`, shows complete 360,448-byte `DSBLOCK_ENTIREBUFFER` Lock/Unlock calls while the play cursor advances roughly 44–46 KB. Treating the Unlock length as new PCM grew the SDL queue to 5,448,476 bytes and is therefore invalid. The final committed-snapshot implementation in `20260828-151817-074.audio.log` classifies 57 of 64 Unlocks—excluding seven initial no-change calls—as exact 45,056-byte dirty intervals. Offsets cycle from 0 through 315,392 in 45,056-byte steps, the queue remains stable between 251,160 and 358,684 bytes, later PCM reaches roughly 0.98 peak, and the matching JSONL contains zero AV, controlled-exit, or OpenGL-failure events. Confirmed: stale first-snapshot repetition and unbounded queue growth are removed. Unresolved: streaming descriptors in other game versions and user-audible accuracy across the complete song and effects.*

38. **확인됨 — 남은 title 음량 저하는 원본 `DemoVolume=0` 설정 때문이다.** 약 60초의 `20260829-001324-200.audio.log`에서 streaming queue와 `+6 dB` master gain은 정상인데 title buffer가 `SetVolume(-10000)`을 한 번만 받고 그대로 유지됐다. caller trace는 wrapper 밖 원본 RVA `0x3120f`를 가리켰다. 대응 unprotected binary에서 이 함수는 전역 인덱스로 VA `0x00466f70`의 dword table `[-10000, -2222, -1111, 0]`을 조회한다. 전역 인덱스는 `GetPrivateProfileIntA("GAMEASSIGNMENTS", "DemoVolume", 3, ...)` 반환값이며 사용자 HDD의 실제 INI 값은 0이다. 따라서 원본이 의도대로 DirectSound 최소 gain을 선택한 것이 직접 원인이고, 기존의 cabinet master 추정이나 PCM 선행 감쇠는 이 현상을 설명하지 않는다. 작업 086은 해당 import thunk의 이 key만 외부 기본 profile 3으로 재정의하고 다른 key는 pass-through한다. 최종 trace `20260829-003716-488.audio.log`는 `configured=3`, `SetVolume(0)`, track/master gain `1.0`, 계속되는 45,056바이트 streaming refresh를 확인했다. 원본 EXE와 HDD INI는 변경하지 않았다. **미확정:** 다른 게임 버전도 같은 key와 table을 사용하는지, 전체 gameplay 효과음의 사용자 청취 정확성. 상세 주소와 상태 구분은 [데모 음량 프로필 분석](ez2dj-demo-volume.md)에 둔다.

*Confirmed — the remaining low title level comes from the original `DemoVolume=0` setting. In the roughly 60-second `20260829-001324-200.audio.log`, streaming and +6 dB master gain work, but the title buffer receives one persistent `SetVolume(-10000)`. Caller tracing identifies original RVA `0x3120f`. In the corresponding unprotected binary, that function indexes the dword table `[-10000, -2222, -1111, 0]` at VA `0x00466f70` with a global loaded from `GetPrivateProfileIntA("GAMEASSIGNMENTS", "DemoVolume", 3, ...)`; the user HDD's actual INI value is zero. The original therefore deliberately selects minimum DirectSound gain. This directly supersedes the cabinet-master conjecture and rules out upstream PCM attenuation for this symptom. Task 086 overrides only that key at its import thunk with external default profile 3 and passes other keys through. Final trace `20260829-003716-488.audio.log` confirms `configured=3`, `SetVolume(0)`, track/master gain `1.0`, and continuing 45,056-byte streaming refreshes. Neither the original EXE nor HDD INI was changed. Unresolved: whether other game versions share this key/table and user-audible accuracy across all gameplay effects.*

**미확정.** 남은 질문: 실패 경로 private page의 원래 목적, 플래그 `[0x01ed7074]`의 의미, entry 직후 XOR 루프의 실제 대상, 물리 동글의 원래 wire-response 알고리즘과 vendor 귀속. 다음 실행 단계는 새 detached runtime에서 화면 정확성, 체감 속도와 실제 오디오·입력을 사용자 환경에서 재검증하는 것이다.

*Unresolved: the intended role of the failure-path private page, flag [0x01ed7074], early XOR-loop targets, the physical dongle's original wire-response algorithm, and vendor attribution. The next execution milestone is user revalidation of visual accuracy, perceived speed, audio, and input under the detached runtime.*

---

### 1.12 Music Select texture Load 경계 — 작업 088

39. **확인됨 — Music Select 곡 BMP는 로드되며 texture Load HLE 결손은 해당 장면의 직접 원인이 아니었다.** 사용자 실행의 `20260829-013719-626.vfs.log`는 `System\MusicSelect\disc\_3week.bmp`를 포함한 곡 그림의 `LoadImageA` 성공을 기록했다. 작업 088은 당시 모든 호출에 `DDERR_UNSUPPORTED`를 반환하던 `IDirect3DTexture2::Load`에 동일 root·크기 RGB565 texture의 pixel row, source color key와 destination revision 복사를 구현했다. 그러나 사용자 재검증에서도 화면 변화가 없었고 최신 `20260829-015640-892.ddraw.log`의 `TextureLoad` 호출은 0회였다. 따라서 texture-copy 결손을 이 장면의 직접 원인으로 보았던 **이전 추정은 기각됨**이다. **확인됨:** 같은 로그에서 실패한 `DrawPrimitive`는 texture 114/115를 사용하는 `FVF 0x112` 14회와 texture 없는 `FVF 0x1e2` 50회이며 모두 `0x80004001`을 반환했다. **미확정:** 작업 089 수정 뒤 중앙 그림 표시 여부와 두 정점 형식 각각의 정확한 시각적 역할.

*Confirmed — Music Select song BMPs load, and the missing texture Load HLE was not the direct cause of this scene defect. User-run log `20260829-013719-626.vfs.log` records successful `LoadImageA` calls for artwork including `System\MusicSelect\disc\_3week.bmp`. Task 088 implemented same-root, equal-sized RGB565 pixel-row, source-color-key, and destination-revision copying for the formerly unconditional `DDERR_UNSUPPORTED` `IDirect3DTexture2::Load`. However, user revalidation showed no visual change and latest log `20260829-015640-892.ddraw.log` records zero `TextureLoad` calls, so the prior direct-cause inference is rejected. Confirmed: failed draws in the same log comprise fourteen textured FVF `0x112` calls using textures 114/115 and fifty untextured FVF `0x1e2` calls, all returning `0x80004001`. Unresolved: center-artwork visibility after Task 089 and the exact visual role of each vertex format.*

### 1.13 변환 전 Direct3D 정점 경계 — 작업 089

40. **확인됨 — Music Select 진행 중 32바이트 변환 전 정점 형식 두 종류가 중앙 곡 그림의 실제 미구현 draw 경계였다.** `FVF 0x112`는 `D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1`인 `D3DVERTEX`, `FVF 0x1e2`는 `D3DFVF_XYZ | D3DFVF_RESERVED1 | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1`인 `D3DLVERTEX`로 해석되며 둘 다 stride 32바이트다. 작업 089는 원본 정점 데이터를 바꾸지 않고 facade가 보존한 world/view/projection matrix와 `D3DVIEWPORT2`를 플랫폼 중립 decoder에 전달해 기존 XYZRHW 명령으로 변환한다. identity·matrix 합성·두 field layout·비정상 입력 단위 테스트와 Windows build, CTest 3/3이 통과했고 사용자가 중앙 그림 복구를 확인했다. **미확정:** `0x112` normal에 대한 lighting이 다른 장면에서 필요한지 여부.

*Confirmed — two 32-byte untransformed vertex formats were the actual unsupported boundary for the Music Select center artwork. FVF `0x112` is `D3DVERTEX` with `D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1`; FVF `0x1e2` is `D3DLVERTEX` with `D3DFVF_XYZ | D3DFVF_RESERVED1 | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1`. Task 089 preserves original vertex data and passes facade-retained world/view/projection matrices and `D3DVIEWPORT2` into a platform-neutral decoder that produces the existing XYZRHW command. Unit tests cover identity, composed transforms, both field layouts, and invalid inputs; Windows builds and CTest 3/3 pass, and the user confirmed restoration of the center artwork. Unresolved: whether FVF `0x112` normals require lighting in other scenes.*

### 1.15 Music Select 논리 좌표와 host viewport — 작업 097

45. **확인됨 — 최신 Music Select 유사 구간의 정적 논리 좌표는 자산 크기와 중앙 정렬에 맞는다.** 실행 `20260830-120003-655.ddraw.log`의 `frame=3327`에서 297x112 `CLUBMIX_PANEL`은 `x=172..469`로, 175x39 `DEMOPLAY`는 `x=232.5..407.5`로 그려졌다. 두 값 모두 논리 640x480 화면의 중앙 배치와 일치한다. 64x64 디스크와 128x128 마스크도 해당 장면의 좌측 carousel 영역 안에서 일관된 크기로 기록됐다. 따라서 이 trace만으로 원본 좌표 오프셋이나 z 정렬 오류는 확인되지 않는다.

**추정 — host 크기 변경 뒤 stale viewport가 좌표 이상으로 보일 수 있다.** SDL3/OpenGL backend는 이전 구현에서 첫 draw 때만 `glViewport`를 설정했다. native child 창이 host shell의 `WM_SIZE` 또는 DPI 변경으로 크기를 바꾼 뒤에도 pixel viewport가 남으면, guest의 논리 좌표는 올바르더라도 실제 출력 비율과 위치가 어긋날 수 있다. 작업 097은 조회한 pixel 폭/높이가 바뀔 때만 `glViewport`를 다시 적용하도록 수정했다.

**미확정.** 사용자가 관찰한 특정 중앙 artwork의 최종 위치는 동일 장면을 재현한 캡처가 필요하다. 현재 보정은 Music Select 전용 좌표 오프셋을 추가하지 않으며, 실제 draw 호출의 좌표와 자산을 다시 대조해야 한다.

46. **확인됨 — 최신 ClubMix 추적에서 디스크의 투명 경계는 색상 키로 활성화되어 있으나 우측 상단 디스크 draw 좌표는 관찰되지 않는다.** 실행 `20260830-121711-829.ddraw.log`의 `frame=603` 이후 `256x256` 후보 표면(`texture=21`, `texture=22`)은 `key=1`, `colorkey=1`, `alphatest=0`으로 그려졌고, 각각 48,147개와 25,534개의 비키 픽셀이 기록됐다. 이는 RGB565 색상 키 discard 경로가 후보 디스크의 유효 픽셀을 모두 제거하는 상태가 아님을 보여준다. 같은 추적에서 폭·높이 64 이상인 텍스처 draw 중 논리 좌표 `x>300`, `y<100`인 상단 우측 후보는 0회였다. 따라서 투명도는 외곽선과 겹침 품질에는 영향을 줄 수 있지만, 이번 누락의 직접 원인은 현재 근거상 좌표/상태 전환 쪽이 우선이며, 대형 합성 표면(`texture=20`, 512x512)의 내부 구성은 미확정이다.

*Confirmed — the latest ClubMix trace has active color-key transparency for the disc edges, but no upper-right disc draw coordinates. After `frame=603` in `20260830-121711-829.ddraw.log`, the 256x256 candidates (`texture=21` and `texture=22`) draw with `key=1`, `colorkey=1`, and `alphatest=0`, with 48,147 and 25,534 non-key pixels respectively. This shows that the RGB565 color-key discard path is not removing all valid candidate-disc pixels. The same trace contains zero textured draws of width and height at least 64 with logical `x>300` and `y<100`. Transparency can still affect edges and overlap quality, but the direct cause of the missing upper-right image is currently more likely coordinate/state transition; the internal composition of the 512x512 surface (`texture=20`) remains unresolved.*

*Confirmed — the static logical coordinates in the latest Music Select-like interval agree with resource sizes and centering. In run `20260830-120003-655.ddraw.log`, at `frame=3327`, the 297x112 `CLUBMIX_PANEL` is drawn at `x=172..469` and the 175x39 `DEMOPLAY` asset at `x=232.5..407.5`, both centered in the 640x480 logical surface. The 64x64 discs and 128x128 mask also remain in a consistent left-carousel region. This trace does not establish a source coordinate offset or z-order defect.

*Inferred — a stale host viewport after a resize may present as a coordinate error. The earlier SDL3/OpenGL backend set `glViewport` only on the first draw. If the native child changes size through the host shell's `WM_SIZE` or DPI handling, the unchanged pixel viewport can displace or scale otherwise-correct guest coordinates. Task 097 now reapplies `glViewport` only when the queried pixel width or height changes.

*Unresolved: the final position of the specific artwork observed by the user still requires a reproducible capture of the same scene. The correction adds no Music Select-specific coordinate offset; the actual draw coordinates and assets must be compared again.*

47. **확인됨 — 늦은 display-surface 합성에는 디스크 후보 표면이 나타나지 않으며, 색상 키가 후보 이미지를 전부 제거하지 않는다.** `frame>=3000` 별도 bounded trace의 `20260830-133722-498.ddraw.log`에서 `frame=3328` visible surface `id=2 (640x480)`에 대한 source는 `id=174 (28x363)`, `id=242 (72x54)`, `id=253 (21x22)`뿐이다. `512x512` 또는 `256x256` 후보 표면을 display surface에 `Blt`/`BltFast`하는 호출은 확인되지 않았다. `frame=603`의 직접 draw `texture=21/22`는 `key=1`, `colorkey=1`, 비키 픽셀 48,147/25,534, `DD_OK`를 기록한다. 따라서 투명도는 가장자리·겹침 품질에는 관여할 수 있지만, 현재 우측 상단 디스크 누락의 직접 원인으로 확인되지 않았다. 원본 애니메이션/상태 전환과 `texture=20 (512x512)` 내부 합성은 미확정이다. 원본 픽셀·자산 내용은 저장하지 않았다.

*Confirmed — the late display-surface composition contains no disc-candidate surface, and color-keying does not erase all candidate pixels. In the separate bounded trace `20260830-133722-498.ddraw.log` for `frame>=3000`, the `frame=3328` visible surface `id=2 (640x480)` receives only sources `id=174 (28x363)`, `id=242 (72x54)`, and `id=253 (21x22)`. No `512x512` or `256x256` candidate surface is sent to the display surface by `Blt`/`BltFast`. The direct draws at `frame=603` for `texture=21/22` report `key=1`, `colorkey=1`, 48,147/25,534 non-key pixels, and `DD_OK`. Transparency may still affect edge and overlap quality, but it is not confirmed as the direct cause of the missing upper-right disc; the original animation/state transition and the internal composition of `texture=20 (512x512)` remain unresolved. Original pixels and asset contents were not stored.*

48. **확인됨 — 후보 표면의 내부 유효 영역은 존재하며 중앙 직접 draw만 관찰된다.** `20260830-141656-891.ddraw.log`의 `frame=603`에서 `texture=20`의 non-key/non-zero 영역은 `(0,0)-(511,511)`이고, `texture=21/22`는 각각 `(1,9)-(255,247)`, `(4,47)-(250,208)`이다. 후보의 비키 픽셀 수는 48,147/25,534이며 직접 draw 결과는 `DD_OK`였다. 따라서 투명도 discard가 표면을 비운 것이 아니며, 관찰된 실행에서 후보 표면은 논리 중앙 `x=195..451`, `y=3..259` 경로로만 그려졌다. 상단 우측 경로는 원본 애니메이션/상태 전환 관점에서 계속 미확정이다. bounding box 요약만 기록하고 원본 픽셀은 저장하지 않았다.

*Confirmed — candidate surface content exists, while only the central direct-draw path is observed. At `frame=603` in `20260830-141656-891.ddraw.log`, the non-key/non-zero area of `texture=20` is `(0,0)-(511,511)`, and the areas of `texture=21/22` are `(1,9)-(255,247)` and `(4,47)-(250,208)`. The candidate non-key counts are 48,147/25,534 and the direct draw results are `DD_OK`. Color-key discard therefore did not empty the surfaces; in the observed run they are drawn only through the logical-central path `x=195..451`, `y=3..259`. The upper-right path remains unresolved at the original animation/state-transition level. Only bounding-box summaries were recorded; original pixels were not stored.*

### 1.14 Win32 창 닫기와 process lifetime — 작업 090

41. **확인됨 — close 감지 뒤 `ExitProcess(0)` termination 교착과 self hard-termination 해결.** 제품 trace `20260829-112237-831`은 PID 41488, HWND `0x0002159a`에서 close message 2회, `visible=0`, `watcher-exit`을 기록했지만 process가 `HasExited=True`, thread 1개, handle 410개의 종료 중 상태로 남고 parent가 대기하는 것을 확인했다. HWND·watcher 실패 가설은 **기각됨**이고 termination sequence 미완료가 **확인됨**이다. **추정:** 남은 thread가 DLL/process detach lock을 기다렸으며 정확한 DLL과 lock은 미확정이다. 원본 WndProc 정리 뒤 current-process `TerminateProcess(..., 0)`을 사용한 실행 `20260829-112906-743`은 같은 close/watcher 경계 뒤 `runtime_detached_exit` code 0과 성공 outcome을 기록했다. Debug/Release CTest 3/3이 통과했고 사용자가 창 닫기 시 process 종료를 확인했다.

*Confirmed — `ExitProcess(0)` termination deadlock after close detection and its self-hard-termination resolution. Product trace `20260829-112237-831` records two close messages, `visible=0`, and `watcher-exit` for PID 41488 and HWND `0x0002159a`, but the process remains terminating with `HasExited=True`, one thread, and 410 handles while its parent waits. HWND and watcher-failure hypotheses are rejected; incomplete termination is confirmed. Inferred: the remaining thread waited on a DLL/process-detach lock; the exact DLL and lock remain unresolved. Run `20260829-112906-743`, using current-process `TerminateProcess(..., 0)` after original-WndProc cleanup, records the same close/watcher boundary followed by `runtime_detached_exit` code zero and a successful outcome. Debug/Release CTest passes 3/3, and the user confirmed process termination on window close.*

42. **확인됨 — 종료 중 교착의 정확한 위치는 injected runtime 정적 오디오 backend의 SDL/WASAPI 종료 경로다.** 제품 실행 `20260829-230321-304`에서 `ez2dj.exe` PID 21140은 CPU 증가가 없고 thread 1개만 남은 채 `ntdll!NtWaitForAlertByThreadId`에서 대기했다. 실행 중인 thread를 짧게 정지하여 읽은 WOW64 raw stack은 `WaitOnAddress` → `SDL_WaitSemaphoreTimeoutNS` → `WASAPI_ProxyToManagementThread` → `WASAPI_DeinitializeStart` → `SDL_QuitAudio` → `Sdl3MixerAudioBackend::~Sdl3MixerAudioBackend` → CRT atexit → `LdrShutdownProcess` → `ExitProcess` 순서를 확인했다. 따라서 작업 090에서 미확정이던 DLL/process-detach lock은 오디오 singleton의 process-exit 소멸자가 SDL audio 관리 thread의 응답을 기다리는 교착으로 확정된다. 종료 중에는 다른 thread가 이미 제거되므로 응답 주체가 없다. 같은 실행의 VFS fallback open과 마지막 Direct3D draw는 성공했으며, 렌더링 또는 입력 HLE 자체가 이 정지 상태의 직접 원인이라는 증거는 없다. **미확정:** 원본 코드가 이번 `ExitProcess` 경로에 진입한 최초 조건과 exit code는 현재 detached 로그만으로 확정하지 못했다.

*Confirmed — the exact shutdown deadlock is in the injected runtime's static audio-backend SDL/WASAPI teardown path. In product run `20260829-230321-304`, ez2dj.exe PID 21140 showed no CPU growth and retained one thread waiting in `ntdll!NtWaitForAlertByThreadId`. A brief WOW64 thread-context capture produced the raw stack sequence `WaitOnAddress` → `SDL_WaitSemaphoreTimeoutNS` → `WASAPI_ProxyToManagementThread` → `WASAPI_DeinitializeStart` → `SDL_QuitAudio` → `Sdl3MixerAudioBackend::~Sdl3MixerAudioBackend` → CRT atexit → `LdrShutdownProcess` → `ExitProcess`. This resolves task 090's unknown DLL/process-detach lock as the process-exit destructor of the audio singleton waiting for an SDL audio management thread that has already been removed during process shutdown. VFS fallback opens and the final Direct3D draws in the same run succeeded; there is no evidence that rendering or input HLE directly caused this stopped state. Unresolved: the initial original-code condition that entered `ExitProcess`, and its exit code, cannot be established from the current detached log.*

43. **확인됨 — process-lifetime 오디오 backend가 종료 교착을 제거하고 뒤의 null execute AV를 노출했다.** 수정 전 실제 WASAPI exit child probe는 native `ExitProcess(0)` 뒤 5초 timeout으로 실패했다. `Sdl3MixerAudioBackend::Instance()`를 atexit에 등록되지 않는 process-lifetime allocation으로 바꾼 뒤 같은 probe는 0.56초에 exit code 0으로 끝났다. Debug/Release CTest는 각각 3/3 통과했다. 실제 제품 실행 `20260829-233725-840`의 PID 20768은 이전과 같은 약 100초 경계에서 thread 1개 교착으로 남지 않고 부모와 함께 종료됐다. launcher는 `runtime_detached_exit` code `0xc0000005`와 success outcome을 기록했고, Windows Application Error는 fault module unknown, fault offset `0x00000000`을 기록했으며 WER dump `ez2dj.exe.20768.dmp`를 생성했다. **확인됨:** SDL/WASAPI teardown은 더 이상 종료를 막지 않는다. **미확정:** 실행 주소 0을 호출한 원본 call site와 누락된 HLE 계약은 dump 분석이 필요하다.

*Confirmed — the process-lifetime audio backend removes the shutdown deadlock and exposes the following null-execute access violation. Before the fix, a real WASAPI exit-child probe timed out five seconds after native `ExitProcess(0)`. After changing `Sdl3MixerAudioBackend::Instance()` to a process-lifetime allocation that is not registered with atexit, the same probe exited with code zero in 0.56 seconds. Debug and Release CTest each pass 3/3. Product run `20260829-233725-840`, PID 20768, reached the same roughly 100-second boundary but did not remain in the former one-thread deadlock; both child and parent exited. The launcher records `runtime_detached_exit` code `0xc0000005` and a success outcome. Windows Application Error records an unknown faulting module at offset `0x00000000`, and WER produced `ez2dj.exe.20768.dmp`. Confirmed: SDL/WASAPI teardown no longer blocks termination. Unresolved: dump analysis must identify the original call site and missing HLE contract behind the execute-at-zero fault.*

44. **확인됨 — 작업 095의 execute-at-zero는 `IDirect3DDevice3::DrawIndexedPrimitiveVB` null slot이었다.** WER dump `ez2dj.exe.20768.dmp`의 exception thread 23292는 `EIP=0`, `ESP=0x001af9d4`, 첫 stack 복귀 주소 `0x004206a3`을 기록한다. 원본 `0x00420670`–`0x004206a3`은 flags 0, index count `0x258`(600), index pointer, vertex-buffer pointer, primitive 4를 push한 뒤 global device `[0x01eb7cc0]`의 vtable `+0x8c`를 간접 호출한다. DirectX 6 `IDirect3DDevice3` ABI에서 이 슬롯은 정확히 `DrawIndexedPrimitiveVB`이고 primitive 4는 `D3DPT_TRIANGLELIST`다. dump의 `EDX`가 가리킨 facade vtable에서 `+0x8c`가 null인 것도 일치했다. **확인됨:** 공용 16-bit index 범위 검사·전개, triangle-list 명령과 Win32 COM 슬롯 구현 뒤 Debug/Release CTest 3/3이 통과했다. 제품 실행 `20260830-000841-620`은 약 3분 동안 응답 상태를 유지했고 정상 close 뒤 child와 parent가 exit code 0으로 종료했으며 기존 `0xc0000005`는 재현되지 않았다. **미확정:** 이 재실행 trace에는 primitive 4 draw marker가 나타나지 않아 실제 제품에서 같은 호출의 성공 진입 자체는 runtime probe로만 검증됐다. 원본이 이전 실행에서 그 호출 경로에 들어간 최초 상태 조건은 미확정이다.

*Confirmed — Task 095 attributes the execute-at-zero failure to a null `IDirect3DDevice3::DrawIndexedPrimitiveVB` slot. Exception thread 23292 in WER dump `ez2dj.exe.20768.dmp` records EIP zero, ESP `0x001af9d4`, and first stack return `0x004206a3`. Original code `0x00420670`–`0x004206a3` pushes flags zero, index count `0x258` (600), an index pointer, a vertex-buffer pointer, and primitive 4 before indirectly calling global device `[0x01eb7cc0]` at vtable offset `+0x8c`. In the DirectX 6 `IDirect3DDevice3` ABI that slot is exactly `DrawIndexedPrimitiveVB`, and primitive 4 is `D3DPT_TRIANGLELIST`. The facade vtable addressed by dump register EDX also has a null `+0x8c` slot. Confirmed: after implementing shared bounds-checked 16-bit index expansion, triangle-list commands, and the Win32 COM slot, Debug and Release CTest each pass 3/3. Product run `20260830-000841-620` remains responsive for roughly three minutes and exits both child and parent with code zero after a normal close; the former `0xc0000005` does not recur. Unresolved: that rerun contains no primitive-four draw marker, so actual entry into the same product call is verified only by the runtime probe. The original state condition that first selected the call path in the earlier run remains unknown.*

### 1.16 호스트 표시 모드 요청 — 작업 183

45. **확인됨 — 원본은 실행마다 `ChangeDisplaySettingsExA`를 정확히 한 번 호출한다.** 제품 실행 `20260905-012007-893`과 `20260905-012235-527`은 각각 `entry=ChangeDisplaySettingsExA:device=default:flags=0x00000001:fields=0x001c0000:640x480x16:refresh=0` 한 줄을 남겼다. `flags=0x1`은 `CDS_UPDATEREGISTRY`, `fields=0x1c0000`은 `DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT`이며 주사율은 지정하지 않는다. 항목 18이 정적으로 기록한 `0x00437cba`의 요청과 일치하는 실행 증거다.

46. **확인됨 — 이 요청이 흡수되지 않으면 호스트 데스크탑 해상도가 실제로 바뀐다.** 표시 경계가 설치되지 않은 실행 `20260905-005825-782`에는 흡수 기록이 없고 데스크탑이 640×480으로 바뀌었다. 경계를 무조건 설치한 뒤의 두 실행은 실행 전후와 실행 중 표본 14회 모두 3,840×2,160×32 @60Hz로 동일했다.

47. **확인됨 — 요청을 흡수해도 그리기 동작은 달라지지 않는다.** 경계 설치 전후 세 실행의 그래픽 추적이 항목별로 같다. `DrawPrimitive` 25, `Flip` 8, `Blt` 8, `CreateSurface` 10, `GetDC`/`ReleaseDC` 각 9, `LateDraw` 2,599, 실패 기록 0이다. 유일한 차이는 새로 추가된 흡수 기록 한 줄이다. 따라서 `DISP_CHANGE_SUCCESSFUL` 응답은 항목 18이 기록한 `PostQuitMessage(0)` 분기를 유발하지 않는다.

48. **추정 — 창 모드 크기가 고DPI 데스크탑에서 축소된다.** 데스크탑이 640×480으로 강제된 상태에서는 창이 의도한 1,296×999로 나왔으나, 3,840×2,160 데스크탑에서는 868×677로 나온다. 1,296 × (96/144) ≈ 864가 관측값과 맞으므로 게스트 프로세스가 DPI 인식을 선언하지 않아 Windows가 창을 축소 배치하는 것으로 보인다. 프로세스의 DPI 인식 상태를 직접 조회해 확인하지는 않았다.

*Confirmed — the original calls `ChangeDisplaySettingsExA` exactly once per run, with `CDS_UPDATEREGISTRY` and a 640x480x16 `DEVMODEA` that specifies no refresh rate, matching the static call site recorded in item 18. Left unabsorbed, that request really does change the host desktop resolution: the run without the boundary switched the desktop to 640x480, while the runs with it stayed at 3840x2160x32 @60Hz throughout. Absorbing it does not change what the guest draws — three runs before and after produce identical trace counts (25 DrawPrimitive, 8 Flip, 8 Blt, 10 CreateSurface, 9 GetDC/ReleaseDC, 2,599 LateDraw, zero failures) — so answering `DISP_CHANGE_SUCCESSFUL` does not take the `PostQuitMessage(0)` branch of item 18. Inferred: the windowed size shrinks on a high-DPI desktop, 868x677 instead of the intended 1,296x999, consistent with the guest process not declaring DPI awareness; its awareness state has not been queried directly.*

---

## 2. `EZ2DJ.exe` — 2nd Trax 대표 실행 파일 (보호 여부 미확정)

### 2.1 헤더와 섹션 — 확인됨

`roms/ez2dj2nd/ez2dj/EZ2DJ.exe`를 `re2dj_pe_analyzer`로 확인했다. entry point RVA `0x00079550`은 `.text` 안에 있고, SizeOfImage는 `0x0047d000`이다. import directory는 `.idata`(RVA `0x00473000`, 크기 `0x0000162e`)에 있고, base relocation directory는 `.reloc`(RVA `0x00475000`, 크기 `0x00007ed4`)에 있다.

*Verified `roms/ez2dj2nd/ez2dj/EZ2DJ.exe` with `re2dj_pe_analyzer`. The entry RVA `0x00079550` is in `.text`; SizeOfImage is `0x0047d000`; the import directory is in `.idata` (RVA `0x00473000`, size `0x0000162e`); and the base-relocation directory is in `.reloc` (RVA `0x00475000`, size `0x00007ed4`).*

| 섹션 | VA | VSize | Raw Off | Raw Size | Flags |
| --- | --- | --- | --- | --- | --- |
| `.text` | `0x00001000` | `0x000db680` | `0x00001000` | `0x000dc000` | code, exec, read |
| `.rdata` | `0x000dd000` | `0x0000c0fa` | `0x000dd000` | `0x0000d000` | data, read |
| `.data` | `0x000ea000` | `0x00388538` | `0x000ea000` | `0x00007000` | data, read, write |
| `.idata` | `0x00473000` | `0x0000162e` | `0x000f1000` | `0x00002000` | data, read, write |
| `.reloc` | `0x00475000` | `0x00007ed4` | `0x000f3000` | `0x00008000` | discardable |

**미확정.** entry point가 `.text`에 있다는 것은 1st SE의 `.gtide`와 같은 보호 전용 진입 섹션이 없다는 뜻이지만, Hardlock이나 다른 런타임 보호 계층의 존재를 부정하지는 않는다. 이를 확인하려면 2nd 실행 중 장치/API 경계와 자기 수정 여부를 별도로 관찰해야 한다.

*Unresolved. An entry point in `.text` means there is no protection-specific entry section like 1st SE's `.gtide`, but it does not rule out Hardlock or another runtime protection layer. Confirming that requires separate observation of the 2nd run's device/API boundary and self-modification behavior.*

### 2.2 HDD sibling과 실행 경로 — 부분 확인

같은 `ez2dj` 디렉터리에는 `EZ2DJ.ini`, `bg`, `sound`, `system`이 있고 `System.ini`는 없다. `ez2dj2nd` target profile은 이 네 항목과 PE header를 fingerprint로 사용한다. 1st SE HLE 기본값을 복제한 것은 사용자의 요청에 따른 호환성 기준이며, 2nd 전용 legacy I/O 주소와 Hardlock 응답은 아직 확인되지 않았다.

*The same `ez2dj` directory contains `EZ2DJ.ini`, `bg`, `sound`, and `system`, but no `System.ini`. The `ez2dj2nd` target profile uses those four entries and the PE header as its fingerprint. Copying the 1st SE HLE defaults follows the user's request as a compatibility baseline; 2nd-specific legacy-I/O addresses and Hardlock responses remain unconfirmed.*

---

## 3. `EZ2DJ.EXE` — 3rd Trax 정식 실행 파일 (보호됨)

### 3.1 헤더와 섹션 — 확인됨

entry point RVA `0x00642240`은 `.protect` 섹션 안에 있고, SizeOfImage는 `0x0067c000`이다. import directory RVA `0x0067af90`과 base relocation directory RVA `0x00643000`이 **모두 `.protect` 가상 범위 안**(`0x00642000` + `0x00039251`)에 있다. 즉 import와 reloc까지 패커 섹션이 소유한다.

*The entry RVA 0x00642240 lies in `.protect`; SizeOfImage is 0x0067c000; and both the import directory (RVA 0x0067af90) and the base-relocation directory (RVA 0x00643000) fall inside the `.protect` virtual range — the packer section owns imports and relocations too.*

| 섹션 | VA | VSize | Raw Off | Raw Size | Flags |
| --- | --- | --- | --- | --- | --- |
| `.text` | `0x00001000` | `0x000c0c96` | `0x00001000` | `0x000c1000` | code, exec, read |
| `.rdata` | `0x000c2000` | `0x0000a68c` | `0x000c2000` | `0x0000b000` | data, read |
| `.data` | `0x000cd000` | `0x00567790` | `0x000cd000` | `0x00015000` | data, read, write |
| `.idata` | `0x00635000` | `0x000016df` | `0x000e2000` | `0x00002000` | data, read, write |
| `.reloc` | `0x00637000` | `0x0000af0a` | `0x000e4000` | `0x0000b000` | data, read, write, discardable |
| `.protect` | `0x00642000` | `0x00039251` | `0x000ef000` | `0x0003a000` | code, exec, read, write |

**확인됨.** `.protect` 플래그는 `0xe0000020`으로 **write 비트가 있다**(1st SE의 `.gtide`와 다름). 섹션 자체가 RWX로 선언되어 자기 수정이 플래그 수준에서 허용된다.

*Confirmed: `.protect` flags 0xe0000020 include write — unlike 1st SE's `.gtide` — so self-modification is permitted at the flag level.*

**추정.** `.data`는 raw 84 KB에 비해 가상 5.5 MB로, 1st SE와 같은 0 채움 정적 버퍼 패턴이다.

*Inferred: `.data` is 84 KB raw against 5.5 MB virtual — the same zero-filled static-buffer pattern as 1st SE.*

### 3.2 import와 런타임 — 부분 확인

정적 import table을 `dumpbin /imports`로 확인하면 KERNEL32의 기본 파일 API(`CreateFileA`, `ReadFile`, `WriteFile`, `CloseHandle`, `GetFileSize` 등), `USER32!MessageBoxA`/`UpdateWindow`, `WINMM!mixerGetLineControlsA`, `DSOUND` ordinal `#1`, `DINPUT!DirectInputCreateA`, `DDRAW!DirectDrawCreateEx`, `AVIFIL32!AVIStreamInfoA`, `WS2_32` ordinal `#9`가 있다. 반면 현재 launcher가 제공하는 `DirectDrawCreate`, `ChangeDisplaySettingsExA`, `LoadImageA`, `GetPrivateProfileIntA`, `GetCommandLineA`, `GetWindowsDirectoryA`, `GetFileType` import는 3rd 정적 IAT에 없다. VFS의 선택적 import 처리는 이 차이를 허용하지만, 3rd 기본 정책에는 DirectDraw/display·command-line/Windows-directory·DemoVolume·legacy I/O hook을 넣지 않는다.

*The static import table, checked with `dumpbin /imports`, contains KERNEL32 file APIs such as `CreateFileA`, `ReadFile`, `WriteFile`, `CloseHandle`, and `GetFileSize`; `USER32!MessageBoxA`/`UpdateWindow`; `WINMM!mixerGetLineControlsA`; DSOUND ordinal `#1`; `DINPUT!DirectInputCreateA`; `DDRAW!DirectDrawCreateEx`; `AVIFIL32!AVIStreamInfoA`; and WS2_32 ordinal `#9`. The 3rd static IAT does not contain the `DirectDrawCreate`, `ChangeDisplaySettingsExA`, `LoadImageA`, `GetPrivateProfileIntA`, `GetCommandLineA`, `GetWindowsDirectoryA`, or `GetFileType` imports currently hooked by the launcher. Optional VFS import handling tolerates this difference, while the 3rd baseline deliberately omits DirectDraw/display, command-line/Windows-directory, DemoVolume, and legacy-I/O hooks.*

**확인됨 — 2026-08-30.** 실행별 로그 `logs/windows_x86_launcher_probe/ez2dj3rd/20260830-152959-334.jsonl`의 `re2dj ez2dj3rd` 실행은 저장소 root 기준 `roms/ez2dj3rd`를 선택하고 `ez2dj/EZ2DJ.EXE`를 built-in profile로 매칭했다. launcher는 entry breakpoint, runtime 주입, DirectSound ordinal hook, `ez2dj` working-directory VFS mount와 선택적 `LoadImageA` 생략을 통과해 `runtime_detached`까지 기록했다. 게임 process는 정상 실행 상태로 유지되어 검증 후 수동 종료했다. 이는 단축 경로와 현재 HLE 준비가 동작한다는 확인이지, 3rd의 화면·입력·보호 해제 전체 성공을 뜻하지 않는다.

*Confirmed — 2026-08-30. The run recorded in `logs/windows_x86_launcher_probe/ez2dj3rd/20260830-152959-334.jsonl` selects `roms/ez2dj3rd` relative to the repository root and matches `ez2dj/EZ2DJ.EXE` as the built-in profile. The launcher reached the entry breakpoint, injected the runtime, hooked DirectSound ordinal 1, mounted VFS from the `ez2dj` working directory, skipped the absent optional `LoadImageA` import, and recorded `runtime_detached`. The game process remained alive in its normal run state and was stopped manually for verification. This confirms shortcut resolution and current HLE preparation, not complete 3rd visual/input/protection success.*

**미확정.** 3rd의 게스트 드라이브 문자와 Win32 작업 디렉터리(`System.ini` 부재), `DirectInput`/AVI/WS2_32의 실제 런타임 역할, `DirectDrawCreateEx`를 통한 그래픽 경로, 보호 스텁의 세부 구조와 LPTDI 응답 계약.

**확인됨 — 2026-08-30.** 3rd의 정적 EXE 검색에서는 `LPTDI`, `TDSD.VXD`, `DeviceIoControl` 문자열이 발견되지 않았고, `EZ2DJ.INI`의 `UseIOCard=1`만 확인됐다. `--hle-vfs --run-detached` 실행 로그 `20260830-172403-483.jsonl`은 entry 주입·VFS mount·detached까지 기록했지만 LPTDI 응답 이벤트나 3rd VFS asset trace는 만들지 않았다. 1st SE target state `0900000000000000`을 3rd에 강제로 전달한 최신 실행 `20260830-172624-412.jsonl`은 정적 IAT에 없는 `DeviceIoControl`을 패치하기 전에 `LPTDI device mock is not configured for this target` 정책 오류로 거부됐다. 따라서 1st의 LPTDI mock/raw-I/O 정책은 3rd와 공유할 수 없으며, 3rd의 실제 `UseIOCard` 소비 방식과 보호 응답 계약은 여전히 미확정이다.

*Confirmed — 2026-08-30. Static EXE scanning of 3rd found no `LPTDI`, `TDSD.VXD`, or `DeviceIoControl` strings; only `UseIOCard=1` was observed in `EZ2DJ.INI`. The `--hle-vfs --run-detached` run in `20260830-172403-483.jsonl` recorded entry injection, VFS mount, and detachment but produced no LPTDI response event or 3rd VFS asset trace. The latest run `20260830-172624-412.jsonl`, which forced the 1st SE target state `0900000000000000` onto 3rd, was rejected by profile policy with `LPTDI device mock is not configured for this target` before attempting to patch the statically absent `DeviceIoControl` import. The 1st LPTDI mock/raw-I/O policy therefore cannot be shared with 3rd; 3rd's actual `UseIOCard` consumption and protection-response contract remain unresolved.*

*Unresolved: 3rd's guest drive and Win32 working directory (no System.ini), the actual runtime roles of DirectInput/AVI/WS2_32, the graphics path through `DirectDrawCreateEx`, the protection stub's anatomy, and the LPTDI response contract.*

**확인됨 — 2026-08-30.** 제품 명령 `re2dj ez2dj3rd`를 실제 실행한 로그 `logs/windows_x86_launcher_probe/ez2dj3rd/20260830-211620-710.jsonl`에서 `roms/ez2dj3rd/ez2dj/EZ2DJ.EXE` 선택, runtime 주입, `ez2dj` working-directory VFS mount, DirectSound hook, `runtime_detached`와 응답 상태의 원본 프로세스를 확인했다. 프로세스 창 열거에서는 `#32770` 클래스와 `Hardlock` 제목의 대화상자가 확인되었으며, 대화상자 본문은 `Error 1009 : Cannot open Hardlock driver.`였다. 따라서 현재 3rd 실행 경계는 게임 화면이 아니라 Hardlock 보호 경계다.

**확인됨 — 2026-08-30.** 3rd VFS runtime probe는 `\\.\\Hardlock`을 설정되지 않은 장치 경로로 통과시킬 때 `device-open` trace에 API, 요청 경로, 실패 상태와 `ERROR_INVALID_NAME`을 기록했다. 그러나 안정적인 3rd 제품 실행에서는 해당 요청이 현재 정적 VFS import thunk를 통과했다는 증거가 없으므로, 이 probe 결과를 실제 Hardlock 응답 계약으로 해석하지 않는다.

*Confirmed — 2026-08-30. The product command `re2dj ez2dj3rd`, recorded in `logs/windows_x86_launcher_probe/ez2dj3rd/20260830-211620-710.jsonl`, selected `roms/ez2dj3rd/ez2dj/EZ2DJ.EXE`, injected the runtime, mounted VFS from the `ez2dj` working directory, hooked DirectSound, recorded `runtime_detached`, and left the original process responsive. Window enumeration found a `#32770` dialog titled `Hardlock` with body `Error 1009 : Cannot open Hardlock driver.` The current 3rd execution boundary is therefore the Hardlock protection boundary, not a game screen.*

*Confirmed — 2026-08-30. The 3rd VFS runtime probe records API, request path, failure status, and `ERROR_INVALID_NAME` in a bounded `device-open` trace when an unconfigured `\\.\\Hardlock` path is passed to it. The stable 3rd product run does not prove that this request passes through the current static VFS import thunk, so the probe result is not treated as the real Hardlock response contract.*

**확인됨 — 2026-08-30, 프로파일별 응답 경계 구현.** `TargetLptdiPolicy`가 synthetic device path prefix와 post-XOR target state를 각각 보유하도록 확장되었다. 1st SE는 `\\.\\LPTDI`와 `0900000000000000`, 3rd는 `\\.\\Hardlock`과 `0000000000000000`을 사용하며, 공용 runtime은 동일한 challenge-mask 변환을 재사용한다. 3rd 값은 1st SE 값의 암묵적 복사가 아니라 zero-state 진단 probe이며 실제 Hardlock 동글 응답이나 seed로 확정하지 않는다. 정적 `DeviceIoControl` import가 없는 3rd를 위해 launcher는 `GetProcAddress` 결과를 runtime의 `CreateFileA`·파일 wrapper·`DeviceIoControl` wrapper로 연결한다. unit test, product-loader probe와 VFS runtime probe에서 두 경계 및 동적 wrapper를 확인했다.

*Confirmed — 2026-08-30, profile-specific response boundary implemented. `TargetLptdiPolicy` now carries an independent synthetic device path prefix and post-XOR target state. 1st SE uses `\\.\\LPTDI` with `0900000000000000`; 3rd uses `\\.\\Hardlock` with `0000000000000000`; the shared runtime reuses the same challenge-mask transform. The 3rd value is a zero-state diagnostic probe, not an implicit copy of 1st SE and not a confirmed physical Hardlock response or seed. Because 3rd has no static `DeviceIoControl` import, the launcher routes `GetProcAddress` results to the runtime's `CreateFileA`, file-wrapper, and `DeviceIoControl` wrappers. The unit test, product-loader probe, and VFS runtime probe verify both boundaries and the dynamic wrapper.*

**확인됨 — 2026-08-31, Function 0x0e 경계.** all-slot `KERNEL32.dll!GetProcAddress` 연결을 적용한 실행 로그 `20260831-000859-972.jsonl`은 두 resolver 슬롯을 모두 연결한 뒤 `.vfs.log`에 `CreateFileA("\\.\\NTICE")`와 `CreateFileA("\\.\\FEnteDev")`를 기록했다. 계측 실행 로그 `20260830-233623-425.vfs.log`에서 `FEnteDev`를 synthetic handle로 연결했을 때 `0x9c402468`, `0x9c402450`, `0x9c40244c`, `0x9c402458` 요청이 이어졌다. 마지막 요청은 256바이트 descriptor와 뒤 8바이트 암호 블록으로 구성되고 descriptor의 `Function`은 `0x0e`였다. 이는 1st SE의 `0x9c406410/414` LPTDI 변환으로 대체할 수 없는 별도 Hardlock 계약이다.

*Confirmed — 2026-08-31, Function 0x0e boundary. With all matching `KERNEL32.dll!GetProcAddress` slots routed, run log `20260831-000859-972.jsonl` records `CreateFileA("\\.\\NTICE")` and `CreateFileA("\\.\\FEnteDev")` in the VFS log. Instrumentation log `20260830-233623-425.vfs.log` shows that, when `FEnteDev` was connected to a synthetic handle, the request sequence continued through `0x9c402468`, `0x9c402450`, `0x9c40244c`, and `0x9c402458`. The final request has a 256-byte descriptor followed by an eight-byte encrypted block, and the descriptor's `Function` is `0x0e`. This is a separate Hardlock contract and cannot be replaced by the 1st SE `0x9c406410/414` LPTDI transform.*

**미확정 — 2026-08-31.** 3rd Function `0x0e`의 유효한 8바이트 응답과 이를 생성하는 세 개의 16비트 seed는 현재 원본 EXE와 실행 trace만으로 확정하지 못했다. 공개 Hardlock 자료도 이 envelope 단계가 세 seed에 의존한다고 설명하며, 실제 동글 dump 또는 알려진 입출력 응답이 다음 분석 입력으로 필요하다. 따라서 zero target state를 3rd seed로 간주하거나, 응답 버퍼를 그대로 보존하는 mock을 성공 구현으로 취급하지 않는다.

*Unresolved — 2026-08-31. The valid eight-byte response for the 3rd Function `0x0e`, and the three 16-bit seeds that generate it, are not established from the original executable and execution traces alone. Public Hardlock material also describes this envelope stage as seed-dependent, so an original dongle dump or a known input/output response is required for the next analysis step. The zero target state must not be treated as the 3rd seed, and a mock that leaves the response buffer unchanged must not be treated as a successful implementation.*

**확인됨 — 2026-08-31, EXE 입력 블록 매핑.** 계측된 18개 Function `0x0e` 입력은 3rd `EZ2DJ.EXE`의 raw offset/RVA `0x1000`부터 `0x8000` 간격으로 이어지는 `.text` chunk 시작 8바이트와 순서대로 정확히 일치했다. 18회 중 고유 입력은 17개이며, 호출 wrapper는 in-place 반환 버퍼와 API 성공 여부를 사용하지만 출력 8바이트를 EXE 내부 고정 상수와 직접 비교하지 않는다. 전체 표는 `docs/analysis/ez2dj3rd-hardlock-function-0e.md`에 유지한다.

*Confirmed — 2026-08-31, EXE input-block mapping. The 18 instrumented Function `0x0e` inputs exactly match, in order, the first eight bytes of `.text` chunks spaced every `0x8000` bytes from raw offset/RVA `0x1000` in the 3rd `EZ2DJ.EXE`. There are 17 unique inputs across 18 calls. The wrapper uses an in-place return buffer and the API success result, but does not directly compare the returned eight bytes with a fixed constant stored in the EXE. The complete table is maintained in `docs/analysis/ez2dj3rd-hardlock-function-0e.md`.*

**확인됨 — 2026-08-31, wrapper 복원과 현재 실행 경계.** 런타임 코드 창을 주소별로 합쳐 복원한 `0x00a4f008..0x00a4f167` 경로는 256바이트 descriptor와 `count * 8` block 배열을 임시 in-place IOCTL packet으로 조립합니다. 새 경량 IOCTL 로그를 사용한 실행은 `0x9c402468` 한 번 뒤 종료했으며 `0x458`에는 도달하지 않았습니다. 유효한 8바이트 response와 세 16비트 seed는 EXE 내부 평문 상수로 확인되지 않았고, synthetic no-op output은 유효 쌍이 아닙니다.

*Confirmed — 2026-08-31, wrapper reconstruction and current runtime boundary. Address-based reconstruction of runtime windows at `0x00a4f008..0x00a4f167` shows the wrapper assembling a temporary in-place IOCTL packet from a 256-byte descriptor and a `count * 8` block array. A run with lightweight IOCTL logging stopped after one `0x9c402468` request and did not reach `0x458`. No valid eight-byte response or three 16-bit seeds were found as plaintext constants in the executable, and synthetic no-op output is not a valid pair.*

---

### 3.3 CHD 빌드는 디렉터리 덤프와 다른 빌드다 — 확인됨 / The CHD build differs from the directory dump — Confirmed

**확인됨 — 2026-09-14.** 위 3.1의 값은 기존 디렉터리 덤프에서 측정한 것이다. 현재 제품이 실제로 실행하는 `roms/ez2dj3rd/ez2dj3rd.chd` 안의 `EZ2DJ/EZ2DJ.EXE`는 크기 1,216,512바이트, FAT 기록 시각 2001-10-15 17:07이며 PE TimeDateStamp가 `0x3bca98a3`(2001-10-15)로 디렉터리 덤프의 `0x3baea943`(2001-09-24)와 다르다. 섹션 레이아웃은 같은 형태이지만 크기가 다르다.

*Confirmed — 2026-09-14. The values in 3.1 were measured on the earlier directory dump. `EZ2DJ/EZ2DJ.EXE` inside `roms/ez2dj3rd/ez2dj3rd.chd`, which the product actually runs, is 1,216,512 bytes with FAT write time 2001-10-15 17:07 and PE TimeDateStamp `0x3bca98a3` (2001-10-15), against `0x3baea943` (2001-09-24) for the directory dump. The section layout has the same shape but different sizes.*

| 항목 / item | 디렉터리 덤프 / directory dump | CHD |
| --- | --- | --- |
| TimeDateStamp | `0x3baea943` | `0x3bca98a3` |
| entry point RVA | `0x00642240` | `0x00642240` |
| SizeOfImage | `0x0067c000` | `0x0067c000` |
| import directory RVA | `0x0067af90` | `0x0067b480` |
| `.text` VSize | `0x000c0c96` | `0x000c0e56` |
| `.protect` VSize | `0x00039251` | `0x00039741` |

3rd 관찰을 인용할 때는 어느 입력에서 나온 것인지 함께 적는다. 두 빌드의 RVA가 대부분 같으므로 주소 하나만으로는 구분되지 않는다.

*Cite which input a 3rd observation came from. Most RVAs agree between the two builds, so an address alone does not distinguish them.*

---

## 4. `EZ2DJ.exe` — 4th Trax 정식 실행 파일 (보호됨) / 4th Trax canonical executable (protected)

### 4.1 헤더와 섹션 — 확인됨 / Headers and sections — Confirmed

**확인됨 — 2026-09-14.** `roms/ez2dj4th/ez2dj4th.chd`의 `EZ2DJ/EZ2DJ.exe`를 `re2dj_chd_probe --dump`로 꺼내 `re2dj_pe_analyzer`로 측정했다. 크기 1,372,160바이트, PE TimeDateStamp `0x3d369bfd`(2002-07-18)다. entry point RVA `0x006e0240`은 `.protect` 안에 있고, import directory RVA `0x007192b0`과 base relocation directory RVA `0x006e1000`도 `.protect` 가상 범위 안에 있다. 3rd와 같은 packer 배치다.

*Confirmed — 2026-09-14. `EZ2DJ/EZ2DJ.exe` was extracted from `roms/ez2dj4th/ez2dj4th.chd` with `re2dj_chd_probe --dump` and measured with `re2dj_pe_analyzer`: 1,372,160 bytes, PE TimeDateStamp `0x3d369bfd` (2002-07-18). The entry RVA `0x006e0240` lies in `.protect`, and both the import directory (RVA `0x007192b0`) and the base-relocation directory (RVA `0x006e1000`) fall inside the `.protect` virtual range — the same packer arrangement as 3rd.*

| 섹션 | VA | VSize | Raw Off | Raw Size | Flags |
| --- | --- | --- | --- | --- | --- |
| `.text` | `0x00001000` | `0x000db022` | `0x00001000` | `0x000dc000` | code, exec, read |
| `.rdata` | `0x000dd000` | `0x0000c766` | `0x000dd000` | `0x0000d000` | data, read |
| `.data` | `0x000ea000` | `0x005e66b0` | `0x000ea000` | `0x0001c000` | data, read, write |
| `.idata` | `0x006d1000` | `0x0000171c` | `0x00106000` | `0x00002000` | data, read, write |
| `.reloc` | `0x006d3000` | `0x0000c05d` | `0x00108000` | `0x0000d000` | data, read, write, discardable |
| `.protect` | `0x006e0000` | `0x00039569` | `0x00115000` | `0x0003a000` | code, exec, read, write |

**확인됨.** `.protect` 플래그는 3rd와 같은 `0xe0000020`이며 write 비트를 포함한다. SizeOfImage는 `0x0071a000`이다.

*Confirmed. The `.protect` flags are `0xe0000020` as in 3rd, including the write bit. SizeOfImage is `0x0071a000`.*

### 4.2 import — 확인됨 / Imports — Confirmed

**확인됨 — 2026-09-14.** 원본 `.idata`(RVA `0x006d1000`)는 10 DLL / 161 함수이고, loader가 bind하는 `.protect` packed table은 36 항목이다. 3rd(10 DLL / 159)와 거의 같고 `WINMM` 표면만 8에서 10으로 늘었다. DLL별 수치는 [import 표면 분석](ez2dj-import-surface.md) 1절에 있다.

*Confirmed — 2026-09-14. The original `.idata` (RVA `0x006d1000`) holds 10 DLLs / 161 functions, while the packed table in `.protect` that the loader binds holds 36 entries. This is nearly identical to 3rd (10 DLLs / 159); only the `WINMM` surface grows from 8 to 10. Per-DLL counts are in section 1 of the [import surface analysis](ez2dj-import-surface.md).*

`.protect` packed table은 3rd와 항목 수가 같고 kernel32 대표 stub 하나만 다르다. 3rd는 `SetCurrentDirectoryA`, 4th는 `CreateThread`다. 나머지 35개 항목은 이름과 순서가 모두 같다.

*The packed tables of 3rd and 4th hold the same number of entries and differ in exactly one kernel32 representative stub — `SetCurrentDirectoryA` for 3rd, `CreateThread` for 4th. The other 35 entries match in both name and order.*

---

## 5. `EZ2Dancer.exe` — EZ2Dancer 2nd MOVE 정식 실행 파일 (보호됨) / EZ2Dancer 2nd MOVE canonical executable (protected)

**확인됨 — 2026-09-14.** 이 문서가 다루는 첫 비-EZ2DJ 제품이다. 크기 622,592바이트, PE TimeDateStamp `0x3a5f074c`(2001-01-12), entry point RVA `0x00401240`, SizeOfImage `0x0043b000`이다. 섹션은 `.text`, `.rdata`, `.data`, `.protect` 넷뿐이며 **`.idata`와 `.reloc` 섹션이 없다.** 원본 import table을 담은 별도 섹션이 없다는 점이 EZ2DJ 3rd·4th와 다르다. loader가 bind하는 packed table은 `.protect` 안 32 항목이다.

*Confirmed — 2026-09-14. This is the first non-EZ2DJ product covered here: 622,592 bytes, PE TimeDateStamp `0x3a5f074c` (2001-01-12), entry RVA `0x00401240`, SizeOfImage `0x0043b000`. It has only four sections — `.text`, `.rdata`, `.data`, `.protect` — and **no `.idata` or `.reloc` section**, unlike EZ2DJ 3rd and 4th, which keep the original import table in its own section. The packed table the loader binds holds 32 entries inside `.protect`.*

| 섹션 | VA | VSize | Raw Off | Raw Size | Flags |
| --- | --- | --- | --- | --- | --- |
| `.text` | `0x00001000` | `0x0004b93e` | `0x00001000` | `0x0004c000` | code, exec, read |
| `.rdata` | `0x0004d000` | `0x00004c10` | `0x0004d000` | `0x00005000` | data, read, write |
| `.data` | `0x00052000` | `0x003ae42c` | `0x00052000` | `0x0000c000` | data, read, write |
| `.protect` | `0x00401000` | `0x000399cd` | `0x0005e000` | `0x0003a000` | code, exec, read, write |

**미확정.** 원본 import table의 위치. `.idata` 섹션이 없으므로 보호 해제 뒤 어디에 재구성되는지는 런타임 관찰로 확인해야 한다.

*Unresolved: where the original import table lives. With no `.idata` section, the location it is reconstructed at after unprotection must be established by runtime observation.*

상세 관찰은 [ez2d2m CHD 파일시스템과 실행 파일 관찰](ez2d2m-chd-filesystem.md)에 있다.

*Detailed observations are in the [ez2d2m CHD filesystem and executable analysis](ez2d2m-chd-filesystem.md).*

---

## 6. `Ez2DJ.exe` — 1st Tracks 정식 실행 파일 (보호됨) / 1st Tracks canonical executable (protected)

### 6.1 헤더와 섹션 — 확인됨 / Headers and sections — Confirmed

**확인됨 — 2026-09-15.** 크기 577,536바이트, PE TimeDateStamp `0x3862fd9d`(1999-12-24 04:59:09 UTC)다. entry point RVA `0x0199b240`은 `.protect` 안에 있고 SizeOfImage는 `0x019b6000`이다. import directory RVA `0x019b5620`과 base relocation directory RVA `0x0199c000`이 모두 `.protect` 가상 범위(`0x0199b000` + `0x0001a8c2`) 안에 있다. 3rd·4th·5th와 같은 packer 배치이며, 1st SE 디렉터리 덤프의 `.gtide`/`.gdata`/`.gidata` 배치와는 다르다.

*Confirmed — 2026-09-15. 577,536 bytes, PE TimeDateStamp `0x3862fd9d` (1999-12-24 04:59:09 UTC). The entry RVA `0x0199b240` lies in `.protect`; SizeOfImage is `0x019b6000`; and both the import directory (RVA `0x019b5620`) and the base-relocation directory (RVA `0x0199c000`) fall inside the `.protect` virtual range (`0x0199b000` + `0x0001a8c2`). This is the same packer arrangement as 3rd, 4th and 5th, and differs from the `.gtide`/`.gdata`/`.gidata` arrangement of the 1st SE directory dump.*

| 섹션 | VA | VSize | Raw Off | Raw Size | Flags |
| --- | --- | --- | --- | --- | --- |
| `.text` | `0x00001000` | `0x0004eec6` | `0x00001000` | `0x0004f000` | code, exec, read |
| `.rdata` | `0x00050000` | `0x00006fe1` | `0x00050000` | `0x00007000` | data, read |
| `.data` | `0x00057000` | `0x0192d3d8` | `0x00057000` | `0x00005000` | data, read, write |
| `.idata` | `0x01985000` | `0x00000f5c` | `0x0005c000` | `0x00001000` | data, read, write |
| `.reloc` | `0x01986000` | `0x000142f4` | `0x0005d000` | `0x00015000` | data, read, write, discardable |
| `.protect` | `0x0199b000` | `0x0001a8c2` | `0x00072000` | `0x0001b000` | code, exec, read, write |

**추정.** `.data`는 raw 20 KB에 비해 가상 25.3 MB로, 다른 모든 EZ2DJ 빌드에서 관찰한 0 채움 정적 버퍼 패턴과 같다.

*Inferred: `.data` is 20 KB raw against 25.3 MB virtual — the same zero-filled static-buffer pattern observed in every other EZ2DJ build.*

### 6.2 import — 확인됨 / Imports — Confirmed

**확인됨 — 2026-09-15.** 원본 `.idata`(RVA `0x01985000`)는 7 DLL / 141 함수다. loader가 bind하는 packed table은 슬롯 30개이며 이름 기준 고유 항목은 22개다. `GetProcAddress`, `GetModuleHandleA`, `RtlUnwind` 세 이름이 두 번씩 나온다. `re2dj_pe_loader`가 보고하는 22와 직접 해석의 30은 이 중복 때문에 다르며, 두 경로는 모순되지 않는다.

*Confirmed — 2026-09-15. The original `.idata` (RVA `0x01985000`) holds 7 DLLs / 141 functions. The packed table the loader binds has 30 slots resolving to 22 distinct names; `GetProcAddress`, `GetModuleHandleA` and `RtlUnwind` each appear twice. The 22 reported by `re2dj_pe_loader` and the 30 counted by the direct parse differ only because of those duplicates; the two paths do not disagree.*

**확인됨 — 2026-09-15.** 1st Tracks의 141개는 1st SE 144개의 **진부분집합**이다. 1st SE가 더 가진 것은 `GDI32!BitBlt`, `GDI32!SetBkColor`, `KERNEL32!GetWindowsDirectoryA` 셋뿐이고, 1st Tracks에만 있는 이름은 없다. 두 제품의 그래픽 진입점은 모두 `DirectDrawCreate`이며 `DirectDrawCreateEx`가 아니다.

*Confirmed — 2026-09-15. The 141 names of 1st Tracks form a **strict subset** of the 144 of 1st SE. 1st SE adds exactly three — `GDI32!BitBlt`, `GDI32!SetBkColor`, `KERNEL32!GetWindowsDirectoryA` — and nothing appears only in 1st Tracks. Both products enter graphics through `DirectDrawCreate`, not `DirectDrawCreateEx`.*

**확인됨 — 2026-09-15.** 이 실행 파일의 평문 문자열에는 `HARDLOCK`, `FEnteDev`, `LPTDI`, `TDSD`가 없다. packer가 본체를 변환하므로 이것은 보호 장치를 쓰지 않는다는 근거가 아니다. 실제 장치 계약은 **미확정**이다.

*Confirmed — 2026-09-15. The plaintext strings of this executable contain no `HARDLOCK`, `FEnteDev`, `LPTDI` or `TDSD`. Because the packer transforms the body, this is not evidence that it uses no protection device. Its actual device contract is **unresolved**.*

### 6.3 두 디렉터리 배치 / Two directory layouts — 확인됨

**확인됨 — 2026-09-15.** 사용자 입력의 `roms/ez2dj1st`에는 `ez2dj`와 `ez2dj1` 두 디렉터리가 있고, `Ez2DJ.exe`·`Test.exe`·`PlzPowerOff.exe` 세 파일이 둘 사이에서 바이트 단위로 같다. `ez2dj1` 쪽에만 `AllowIo.exe`, `PortTalk.sys`, `Cursor.cur`, `Icon.ico`, `Version.bmp`, `WarningMsg_Asia.bmp`, `WarningMsg_Japan.bmp`, `WarningMsg_Korea.bmp`가 더 있다. 따라서 두 디렉터리는 같은 실행 파일의 서로 다른 배치이며, 분석 대상으로는 하나로 취급한다.

*Confirmed — 2026-09-15. The user input `roms/ez2dj1st` holds two directories, `ez2dj` and `ez2dj1`, whose `Ez2DJ.exe`, `Test.exe` and `PlzPowerOff.exe` are byte-identical to each other. Only `ez2dj1` additionally carries `AllowIo.exe`, `PortTalk.sys`, `Cursor.cur`, `Icon.ico`, `Version.bmp` and the three `WarningMsg_*.bmp` files. The two directories are therefore different layouts of the same executable and are treated as one analysis subject.*

**확인됨 — 2026-09-15.** `ez2dj1`의 `AllowIo.exe`(40,125바이트, `0x3c3fc787`)와 `PortTalk.sys`(3,567바이트, `0x3c3fdf10`)는 둘 다 2002-01-12 빌드다. 1st Tracks 게임 실행 파일보다 2년 뒤이므로, 이 배치는 원래의 1999년 출하 구성 그대로가 아니다. `PortTalk.sys`는 `.text .rdata .data INIT .rsrc .reloc` 섹션을 가진 커널 드라이버로, legacy I/O port 접근을 사용자 모드에 열어 주는 공개 도구 계열이다. 8.4절에서 6th 동봉 배치와 같은 파일임을 확인한다.

*Confirmed — 2026-09-15. `ez2dj1`'s `AllowIo.exe` (40,125 bytes, `0x3c3fc787`) and `PortTalk.sys` (3,567 bytes, `0x3c3fdf10`) are both 2002-01-12 builds — two years after the 1st Tracks game executable — so this layout is not the original 1999 shipping configuration. `PortTalk.sys` is a kernel driver with `.text .rdata .data INIT .rsrc .reloc` sections, of the well-known family that opens legacy I/O port access to user mode. Section 8.4 confirms these are the same files as in the 6th's bundled layout.*

---

## 7. `EZ2DJ.exe` — 5th Trax 정식 실행 파일 (보호됨) / 5th Trax canonical executable (protected)

### 7.1 헤더와 섹션 — 확인됨 / Headers and sections — Confirmed

**확인됨 — 2026-09-15.** `roms/ez2dj5th/ez2dj/EZ2DJ.exe`를 `re2dj_pe_analyzer`로 측정했다. 크기 1,388,544바이트, PE TimeDateStamp `0x3f53377b`(2003-09-01)다. entry point RVA `0x0070d240`은 `.protect` 안에 있고 SizeOfImage는 `0x00746000`이다. import directory RVA `0x007458e0`과 base relocation directory RVA `0x0070e000`이 모두 `.protect` 가상 범위(`0x0070d000` + `0x00038ba0`) 안에 있다. 3rd·4th와 같은 packer 배치다.

*Confirmed — 2026-09-15. `roms/ez2dj5th/ez2dj/EZ2DJ.exe` measured with `re2dj_pe_analyzer`: 1,388,544 bytes, PE TimeDateStamp `0x3f53377b` (2003-09-01). The entry RVA `0x0070d240` lies in `.protect`; SizeOfImage is `0x00746000`; and both the import directory (RVA `0x007458e0`) and the base-relocation directory (RVA `0x0070e000`) fall inside the `.protect` virtual range (`0x0070d000` + `0x00038ba0`) — the same packer arrangement as 3rd and 4th.*

| 섹션 | VA | VSize | Raw Off | Raw Size | Flags |
| --- | --- | --- | --- | --- | --- |
| `.text` | `0x00001000` | `0x000e19cb` | `0x00001000` | `0x000e2000` | code, exec, read |
| `.rdata` | `0x000e3000` | `0x0000d5a1` | `0x000e3000` | `0x0000e000` | data, read |
| `.data` | `0x000f1000` | `0x0060c3b0` | `0x000f1000` | `0x0001a000` | data, read, write |
| `.idata` | `0x006fe000` | `0x0000171c` | `0x0010b000` | `0x00002000` | data, read, write |
| `.reloc` | `0x00700000` | `0x0000c801` | `0x0010d000` | `0x0000d000` | data, read, write, discardable |
| `.protect` | `0x0070d000` | `0x00038ba0` | `0x0011a000` | `0x00039000` | code, exec, read, write |

**확인됨.** `.protect` 플래그는 3rd·4th와 같은 `0xe0000020`이며 write 비트를 포함한다. `.idata`의 VSize `0x0000171c`는 4th와 정확히 같다.

*Confirmed. The `.protect` flags are `0xe0000020` as in 3rd and 4th, including the write bit. The `.idata` VSize `0x0000171c` is exactly the same as 4th's.*

### 7.2 import — 4th와 같은 표면 / The same surface as 4th — 확인됨

**확인됨 — 2026-09-15.** 원본 `.idata`(RVA `0x006fe000`)는 10 DLL / 161 함수다. 4th의 원본 `.idata`와 **DLL 목록과 함수 이름 집합이 완전히 같다.** 표에 나타나는 순서만 다르다. 두 빌드를 DLL별로 비교했을 때 한쪽에만 있는 이름은 없다.

*Confirmed — 2026-09-15. The original `.idata` (RVA `0x006fe000`) holds 10 DLLs / 161 functions, and its **DLL list and function-name set are exactly the same as 4th's** original `.idata`; only the order in the table differs. A per-DLL comparison of the two builds finds no name present in one and absent in the other.*

| DLL | 4th | 5th |
| --- | --- | --- |
| `KERNEL32.dll` | 88 | 88 |
| `USER32.dll` | 32 | 32 |
| `GDI32.dll` | 12 | 12 |
| `WINMM.dll` | 10 | 10 |
| `WS2_32.dll` | 9 | 9 |
| `AVIFIL32.dll` | 5 | 5 |
| `DDRAW.dll` | 2 | 2 |
| `ADVAPI32.dll` | 1 | 1 |
| `DSOUND.dll` | 1 | 1 |
| `DINPUT.dll` | 1 | 1 |
| 합계 / total | **161** | **161** |

**확인됨 — 2026-09-15.** loader가 bind하는 packed table도 4th와 같은 모양이다. 슬롯 38개, 고유 이름 36개이며 `GetProcAddress`와 `GetModuleHandleA`가 두 번씩 나온다. 36개 항목 중 30개는 이름과 순서가 4th와 같고, DLL당 대표 stub 6개만 다르다.

*Confirmed — 2026-09-15. The packed table the loader binds has the same shape as 4th's: 38 slots resolving to 36 distinct names, with `GetProcAddress` and `GetModuleHandleA` appearing twice. Thirty of the 36 match 4th in both name and order; only six per-DLL representative stubs differ.*

| 슬롯 / slot | 4th | 5th |
| --- | --- | --- |
| `0xf00001a0` | `kernel32!CreateThread` | `kernel32!CompareStringW` |
| `0xf00001b0` | `user32!UpdateWindow` | `user32!ScreenToClient` |
| `0xf00001c0` | `gdi32!GetStockObject` | `gdi32!CreateCompatibleDC` |
| `0xf00001e0` | `winmm!mixerGetLineControlsA` | `winmm!timeEndPeriod` |
| `0xf0000210` | `ddraw!DirectDrawCreateEx` | `ddraw!DirectDrawEnumerateExA` |
| `0xf0000220` | `avifil32!AVIStreamInfoA` | `avifil32!AVIStreamGetFrame` |
| `0xf0000230` | `ws2_32!#9` | `ws2_32!#17` |

**추정.** import 표면이 4th와 같다는 것은 5th가 4th 대비 새로운 Win32 API HLE를 요구하지 않는다는 뜻이다. 그러나 같은 API 집합이 같은 호출 순서와 같은 인자 계약을 뜻하지는 않으므로, 5th 실행 성공의 근거로 쓰지 않는다.

*Inferred: an identical import surface means 5th demands no Win32 API beyond what 4th already needs from the HLE. It does not mean the same call order or the same argument contracts, so it is not treated as evidence that 5th will run.*

### 7.3 입력 출처 / Input provenance — 미확정

**미확정 — 2026-09-15.** `roms/ez2dj5th/ez2dj/`가 같은 디렉터리의 `ez2dj5.chd`에서 나온 것인지는 확인하지 못했다. 현재 `Fat32Volume`은 5th 이미지에서 in-range FAT32 partition을 찾지 못하므로([5th·6th CHD 파일시스템](ez2dj5th-6th-chd-filesystem.md)), CHD 안의 대응 파일과 해시를 비교할 수 없다. 확인하려면 5th 이미지의 파티션 종류와 파일시스템을 먼저 풀어야 한다.

*Unresolved — 2026-09-15. Whether `roms/ez2dj5th/ez2dj/` was extracted from the `ez2dj5.chd` beside it is not established. `Fat32Volume` currently finds no in-range FAT32 partition in the 5th image (see [5th/6th CHD filesystem](ez2dj5th-6th-chd-filesystem.md)), so the corresponding file inside the CHD cannot be hashed for comparison. Resolving the 5th image's partition type and filesystem comes first.*

**확인됨 — 2026-09-15.** 같은 디렉터리에 `EZ2DJ.INI`, `CACHE.REG`, `CACHE.TXT`, `FONTEN.DAT`, `FONTKR.DAT`, `BG`, `SOUND`, `SYSTEM`이 있다. `EZ2DJ.INI`는 평문이며 `UseIOCard = 1`, `FullScreen = 1`, 640×480 창 크기를 담는다. 3rd와 같은 `UseIOCard` 키이며, 5th가 이 값을 어떻게 소비하는지는 **미확정**이다.

*Confirmed — 2026-09-15. The same directory holds `EZ2DJ.INI`, `CACHE.REG`, `CACHE.TXT`, `FONTEN.DAT`, `FONTKR.DAT`, `BG`, `SOUND` and `SYSTEM`. `EZ2DJ.INI` is plaintext and carries `UseIOCard = 1`, `FullScreen = 1` and a 640x480 window size — the same `UseIOCard` key as 3rd. How 5th consumes that value is **unresolved**.*

---

## 8. 6th Trax — bootstrap·게임 본체·동봉 1st Tracks / 6th Trax — bootstrap, game body, bundled 1st Tracks

6th는 이 문서에서 처음 다루는 **두 프로세스 구조**다. 캐비닛이 실행하는 `EZ2DJ.EXE`는 게임이 아니라 launcher이고, 실제 게임은 그것이 만드는 자식 프로세스다. 세 실행 파일 모두 `.protect`나 `.gtide` 같은 보호 섹션이 없다.

*6th is the first **two-process structure** covered here: the `EZ2DJ.EXE` the cabinet runs is a launcher, not the game, and the real game is the child process it creates. None of the three executables carries a protection section such as `.protect` or `.gtide`.*

```mermaid
flowchart TD
    A["EZ2DJ.EXE<br/>bootstrap 126,976 B<br/>보호 없음 / unprotected"]
    B["EZ2DJ6th.EXE<br/>6th 게임 본체 / game body<br/>585,728 B"]
    C["EZ2DJ1ST 배치 Ez2DJ.exe<br/>동봉 1st Tracks / bundled 1st<br/>360,448 B"]
    D["HARDLOCK.VXD<br/>FEnteDev"]
    A -->|"CreateProcessA"| B
    A -->|"CreateProcessA"| C
    A -.->|"장치 문자열 보유 / carries device strings"| D
    B -.-> D
    C -.-> D
```

### 8.1 `EZ2DJ.EXE` — bootstrap — 확인됨 / Confirmed

**확인됨 — 2026-09-15.** 크기 126,976바이트, PE TimeDateStamp `0x411646a8`(2004-08-08)다. entry point RVA `0x000153ff`는 `.text` 안에 있고 SizeOfImage는 `0x00021000`이다. 섹션은 `.text`, `.rdata`, `.data` 셋뿐이고 보호 섹션이 없다. import directory는 `.rdata`(RVA `0x0001a524`), IAT도 `.rdata`(RVA `0x0001a000`)에 있다. characteristics는 `0x010f`로 `RELOCS_STRIPPED` 비트를 포함하므로 재배치 정보가 없다.

*Confirmed — 2026-09-15. 126,976 bytes, PE TimeDateStamp `0x411646a8` (2004-08-08). The entry RVA `0x000153ff` is in `.text`; SizeOfImage is `0x00021000`; there are only three sections — `.text`, `.rdata`, `.data` — and no protection section. The import directory sits in `.rdata` (RVA `0x0001a524`) with the IAT also in `.rdata` (RVA `0x0001a000`). Its characteristics `0x010f` include `RELOCS_STRIPPED`, so it carries no relocation information.*

| 섹션 | VA | VSize | Raw Off | Raw Size | Flags |
| --- | --- | --- | --- | --- | --- |
| `.text` | `0x00001000` | `0x00018c3a` | `0x00001000` | `0x00019000` | code, exec, read |
| `.rdata` | `0x0001a000` | `0x00000b28` | `0x0001a000` | `0x00001000` | data, read |
| `.data` | `0x0001b000` | `0x0000533c` | `0x0001b000` | `0x00004000` | data, read, write |

**확인됨 — 2026-09-15.** 보호가 없으므로 import table은 하나뿐이고, 그것이 곧 loader가 bind하는 표면이다. 2 DLL / 68 함수이며 `KERNEL32` 65개, `USER32` 3개(`GetForegroundWindow`, `MessageBoxA`, `GetKeyState`)다. 프로세스 생성 경계는 `CreateProcessA`, `WaitForSingleObject`, `GetExitCodeProcess`, `SetPriorityClass`, `TerminateProcess`, `SetCurrentDirectoryA`로 구성된다.

*Confirmed — 2026-09-15. With no protection there is a single import table, and it is the surface the loader binds: 2 DLLs / 68 functions — 65 from `KERNEL32` and three from `USER32` (`GetForegroundWindow`, `MessageBoxA`, `GetKeyState`). Its process-creation boundary is `CreateProcessA`, `WaitForSingleObject`, `GetExitCodeProcess`, `SetPriorityClass`, `TerminateProcess` and `SetCurrentDirectoryA`.*

**확인됨 — 2026-09-15.** 평문 문자열에 자식 경로가 **두 개** 있다. raw offset `0x1b0b0`의 `.\EZ2DJ6TH.EXE`와 `0x1b094`의 `.\EZ2DJ1ST\EZ2DJ.EXE`이며, `0x1b088`에는 `%s\EZ2DJ1ST`가 있다. 기존 [5th·6th CHD 파일시스템 문서](ez2dj5th-6th-chd-filesystem.md)는 첫 번째만 기록했다. 어떤 조건에서 어느 자식을 고르는지는 **미확정**이다.

*Confirmed — 2026-09-15. The plaintext strings contain **two** child paths: `.\EZ2DJ6TH.EXE` at raw offset `0x1b0b0` and `.\EZ2DJ1ST\EZ2DJ.EXE` at `0x1b094`, with `%s\EZ2DJ1ST` at `0x1b088`. The existing [5th/6th CHD filesystem document](ez2dj5th-6th-chd-filesystem.md) recorded only the first. Under which condition each child is chosen is **unresolved**.*

**확인됨 — 2026-09-15.** bootstrap은 `HARDLOCK.VXD`와 `FEnteDev` 장치 문자열을 raw offset `0x1b5c0`부터 담고 있다. 즉 Hardlock 경계는 자식만의 것이 아니라 launcher도 지난다. `RegOpenKeyA`·`RegCloseKey` 문자열도 있으나 `ADVAPI32` static import는 없으므로, 레지스트리 접근은 `LoadLibraryA`/`GetProcAddress`로 동적 해석된다.

*Confirmed — 2026-09-15. The bootstrap carries the `HARDLOCK.VXD` and `FEnteDev` device strings from raw offset `0x1b5c0`, so the Hardlock boundary is crossed by the launcher too, not only by the child. It also carries `RegOpenKeyA` and `RegCloseKey` strings while importing no `ADVAPI32`, so registry access is resolved dynamically through `LoadLibraryA`/`GetProcAddress`.*

### 8.2 `EZ2DJ6th.EXE` — 게임 본체 / game body — 확인됨

**확인됨 — 2026-09-15.** 크기 585,728바이트, PE TimeDateStamp `0x411f6d44`(2004-08-15)다. entry point RVA `0x000667d4`는 `.text` 안에 있고 SizeOfImage는 `0x00e34000`이다. 섹션은 `.text`, `.rdata`, `.data` 셋뿐이며 보호 섹션이 없다. characteristics `0x010f`에 `RELOCS_STRIPPED`가 있다.

*Confirmed — 2026-09-15. 585,728 bytes, PE TimeDateStamp `0x411f6d44` (2004-08-15). The entry RVA `0x000667d4` is in `.text`; SizeOfImage is `0x00e34000`; there are only three sections and no protection section; characteristics `0x010f` include `RELOCS_STRIPPED`.*

| 섹션 | VA | VSize | Raw Off | Raw Size | Flags |
| --- | --- | --- | --- | --- | --- |
| `.text` | `0x00001000` | `0x000705c2` | `0x00001000` | `0x00071000` | code, exec, read |
| `.rdata` | `0x00072000` | `0x0000abfa` | `0x00072000` | `0x0000b000` | data, read |
| `.data` | `0x0007d000` | `0x00db6b9c` | `0x0007d000` | `0x00012000` | data, read, write |

**확인됨 — 2026-09-15.** import는 7 DLL / 137 함수이며, 보호가 없으므로 이것이 원본이자 loader가 bind하는 표면이다. 지금까지 분석한 EZ2DJ 빌드 가운데 **게임 표면을 unpack 없이 읽을 수 있는 첫 번째 빌드**다.

*Confirmed — 2026-09-15. Imports are 7 DLLs / 137 functions and, with no protection, this is both the original and the loader-bound surface. It is the **first EZ2DJ build analyzed here whose game surface can be read without unpacking**.*

| DLL | 4th·5th | 6th |
| --- | --- | --- |
| `KERNEL32.dll` | 88 | 87 |
| `USER32.dll` | 32 | 25 |
| `GDI32.dll` | 12 | 11 |
| `WINMM.dll` | 10 | 10 |
| `DSOUND.dll` | 1 | 1 |
| `DINPUT.dll` | 1 | 1 |
| `DDRAW.dll` | 2 | 2 |
| `ADVAPI32.dll` | 1 | — |
| `AVIFIL32.dll` | 5 | — |
| `WS2_32.dll` | 9 | — |
| 합계 / total | **161** | **137** |

**확인됨 — 2026-09-15.** 6th는 4th·5th 대비 `ADVAPI32`, `AVIFIL32`, `WS2_32` 세 DLL을 **전부** 뺀다. 따라서 6th 게임 본체는 AVI 재생과 Winsock 경계를 요구하지 않는다. 대신 `GetPrivateProfileIntA`, `WritePrivateProfileStringA`, `GetFullPathNameA`, `GetCurrentThread`, `GDI32!DeleteDC`가 새로 들어온다. 빠지는 `USER32` 이름에는 `ChangeDisplaySettingsExA`, `EnumDisplaySettingsA`, `ExitWindowsEx`, `ReleaseDC`, `SetCursor`, `RedrawWindow`, `DrawMenuBar`가 있다.

*Confirmed — 2026-09-15. Against 4th and 5th, 6th drops `ADVAPI32`, `AVIFIL32` and `WS2_32` **entirely**, so the 6th game body requires neither AVI playback nor a Winsock boundary. It adds `GetPrivateProfileIntA`, `WritePrivateProfileStringA`, `GetFullPathNameA`, `GetCurrentThread` and `GDI32!DeleteDC`. The `USER32` names it drops include `ChangeDisplaySettingsExA`, `EnumDisplaySettingsA`, `ExitWindowsEx`, `ReleaseDC`, `SetCursor`, `RedrawWindow` and `DrawMenuBar`.*

**확인됨 — 2026-09-15.** 이 실행 파일도 `HARDLOCK.VXD`와 `FEnteDev` 장치 문자열을 raw offset `0x8b748`부터 담는다. 또 동봉 1st Tracks 배치의 `bookkeeping.ini` 상대 경로를 raw `0x891f8`에서 참조하므로, 게임 본체 역시 그 배치를 알고 있다.

*Confirmed — 2026-09-15. This executable also carries the `HARDLOCK.VXD` and `FEnteDev` device strings from raw offset `0x8b748`, and at raw `0x891f8` it references the bundled 1st Tracks layout's `bookkeeping.ini` by relative path, so the game body is aware of that layout as well.*

### 8.3 입력 일치 / Input agreement — 확인됨

**확인됨 — 2026-09-15.** `roms/ez2dj6th/extracted/EZ2DJ/`의 두 실행 파일은 `roms/ez2dj6th/6th.chd` 안의 같은 이름 파일과 바이트 단위로 같다. `re2dj_chd_probe --dump`로 꺼낸 바이트의 MD5·SHA-1·SHA-256이 디렉터리 쪽 값과 모두 일치한다. 따라서 6th는 3rd와 달리 입력에 따라 빌드가 갈리지 않는다. `roms/ez2d2m/extracted/ez2dancer/EZ2Dancer.exe`와 `ez2d2m.chd` 내부 파일도 같은 방식으로 일치를 확인했다.

*Confirmed — 2026-09-15. Both executables under `roms/ez2dj6th/extracted/EZ2DJ/` are byte-identical to the same-named files inside `roms/ez2dj6th/6th.chd`: the MD5, SHA-1 and SHA-256 of the bytes extracted with `re2dj_chd_probe --dump` all match the directory-side values. Unlike 3rd, 6th therefore does not split into different builds by input. The same check confirms agreement between `roms/ez2d2m/extracted/ez2dancer/EZ2Dancer.exe` and its counterpart inside `ez2d2m.chd`.*

### 8.4 동봉 1st Tracks — 보호되지 않은 2004년 재빌드 / Bundled 1st Tracks — an unprotected 2004 rebuild — 확인됨

**확인됨 — 2026-09-15.** `6th.chd`의 `EZ2DJ/Ez2Dj1st/`에는 완전한 1st Tracks 배치가 들어 있다. `Ez2DJ.exe`, `EZ2DJ.INI`, `Test.exe`, `PlzPowerOff.exe`, `AllowIo.exe`, `PortTalk.sys`, `Cursor.cur`, `bookkeeping.ini`, `RANK_1.DAT`, `RANK_3.DAT`, `Version.abm`, `WarningMsg_Asia.abm`, `WarningMsg_Japan.abm`, `WarningMsg_Korea.abm`, `System`, `Songs` 열여섯 항목이다.

*Confirmed — 2026-09-15. `EZ2DJ/Ez2Dj1st/` inside `6th.chd` holds a complete 1st Tracks layout — sixteen entries: `Ez2DJ.exe`, `EZ2DJ.INI`, `Test.exe`, `PlzPowerOff.exe`, `AllowIo.exe`, `PortTalk.sys`, `Cursor.cur`, `bookkeeping.ini`, `RANK_1.DAT`, `RANK_3.DAT`, `Version.abm`, the three `WarningMsg_*.abm` files, `System` and `Songs`.*

**확인됨 — 2026-09-15.** 이 배치의 `Ez2DJ.exe`는 360,448바이트, PE TimeDateStamp `0x411bbf5c`(2004-08-12)이고 **보호 섹션이 없다.** entry point RVA `0x00036f30`은 `.text` 안에 있으며 섹션은 `.text`, `.rdata`, `.data` 셋뿐이다. SizeOfImage는 `0x01914000`이고 characteristics는 `0x010f`다. 즉 1999년 보호 빌드(6절)와 같은 게임의 2004년 **보호되지 않은 재빌드**다.

*Confirmed — 2026-09-15. That layout's `Ez2DJ.exe` is 360,448 bytes with PE TimeDateStamp `0x411bbf5c` (2004-08-12) and **carries no protection section**: the entry RVA `0x00036f30` is in `.text`, there are only three sections — `.text`, `.rdata`, `.data` — SizeOfImage is `0x01914000`, and characteristics are `0x010f`. It is an **unprotected 2004 rebuild** of the same game as the 1999 protected build in section 6.*

| 섹션 | VA | VSize | Raw Off | Raw Size | Flags |
| --- | --- | --- | --- | --- | --- |
| `.text` | `0x00001000` | `0x00048e5b` | `0x00001000` | `0x00049000` | code, exec, read |
| `.rdata` | `0x0004a000` | `0x00004384` | `0x0004a000` | `0x00005000` | data, read |
| `.data` | `0x0004f000` | `0x018c4628` | `0x0004f000` | `0x00009000` | data, read, write |

**확인됨 — 2026-09-15.** import는 6 DLL / 138 함수이며 보호가 없으므로 이것이 전체 표면이다. 1999년 빌드의 141개와 비교하면 `ADVAPI32` 전체(`RegFlushKey` 하나)가 빠지고, `USER32`에서 `ChangeDisplaySettingsExA`, `EnumDisplaySettingsA`, `ExitWindowsEx`, `LoadImageA`가 빠지며, `KERNEL32`에서 `QueryPerformanceFrequency`가 들어오고 `GDI32!CreateDIBitmap`, `USER32!GetDC`, `USER32!MessageBoxA`가 추가된다.

*Confirmed — 2026-09-15. Imports are 6 DLLs / 138 functions and, with no protection, that is the complete surface. Against the 141 of the 1999 build it drops all of `ADVAPI32` (a single `RegFlushKey`), drops `ChangeDisplaySettingsExA`, `EnumDisplaySettingsA`, `ExitWindowsEx` and `LoadImageA` from `USER32`, and adds `QueryPerformanceFrequency` to `KERNEL32` plus `GDI32!CreateDIBitmap`, `USER32!GetDC` and `USER32!MessageBoxA`.*

| DLL | 1999 보호 빌드 / protected | 2004 동봉 빌드 / bundled |
| --- | --- | --- |
| `KERNEL32.dll` | 96 | 95 |
| `USER32.dll` | 21 | 20 |
| `GDI32.dll` | 12 | 13 |
| `WINMM.dll` | 8 | 7 |
| `DDRAW.dll` | 2 | 2 |
| `DSOUND.dll` | 1 | 1 |
| `ADVAPI32.dll` | 1 | — |
| 합계 / total | **141** | **138** |

**확인됨 — 2026-09-15.** 이 빌드도 `HARDLOCK.VXD`와 `FEnteDev` 장치 문자열을 raw offset `0x53f18`부터 담는다. 1999년 보호 빌드에는 이 문자열이 평문으로 없다. 따라서 동봉 빌드는 6th 캐비닛의 Hardlock 경계 위에서 동작하도록 만들어진 것이고, 1999년 빌드의 장치 계약과 같다고 볼 근거는 없다.

*Confirmed — 2026-09-15. This build also carries the `HARDLOCK.VXD` and `FEnteDev` device strings from raw offset `0x53f18`, while the 1999 protected build carries neither as plaintext. The bundled build is therefore made to run on the 6th cabinet's Hardlock boundary, and there is no basis for treating its device contract as the same as the 1999 build's.*

**확인됨 — 2026-09-15.** 이 배치의 `Test.exe`, `PlzPowerOff.exe`, `AllowIo.exe`, `PortTalk.sys` 네 파일은 `roms/ez2dj1st/ez2dj1/`의 같은 이름 파일과 SHA-256이 같다. 반면 `Ez2DJ.exe`는 다르다(360,448바이트 대 577,536바이트).

*Confirmed — 2026-09-15. Four files of this layout — `Test.exe`, `PlzPowerOff.exe`, `AllowIo.exe`, `PortTalk.sys` — share their SHA-256 with the same-named files under `roms/ez2dj1st/ez2dj1/`, while `Ez2DJ.exe` does not (360,448 against 577,536 bytes).*

**추정 — 2026-09-15.** 보조 도구 네 개가 일치하고 `Version`/`WarningMsg` 자산 이름이 확장자만 다른(`.abm` 대 `.bmp`) 점으로 보아, `roms/ez2dj1st/ez2dj1/`은 6th 캐비닛의 동봉 1st Tracks 배치와 같은 계보에서 나왔고 게임 실행 파일만 1999년 보호 빌드로 바뀐 상태로 보인다. 그러나 배치의 실제 출처는 **미확정**이며, 해시 일치만으로 두 디렉터리가 같은 덤프에서 나왔다고 확정하지 않는다.

*Inferred — 2026-09-15. Four matching auxiliary tools, and `Version`/`WarningMsg` assets that differ only in extension (`.abm` against `.bmp`), suggest `roms/ez2dj1st/ez2dj1/` descends from the same lineage as the 6th cabinet's bundled 1st Tracks layout with only the game executable swapped for the 1999 protected build. The layout's actual provenance is **unresolved**, and matching hashes alone are not treated as proof that the two directories came from one dump.*

### 8.5 `EZ2DJ.INI`는 평문이 아니다 / `EZ2DJ.INI` is not plaintext — 확인됨

**확인됨 — 2026-09-15.** 6th의 `EZ2DJ/EZ2DJ.INI`(816바이트)는 읽을 수 있는 INI 텍스트가 아니다. 같은 디렉터리의 `bookkeeping.ini`(162바이트)는 평문이며 `[GAMEASSIGNMENTS]`와 `[STATISTICS]` 섹션을 담는다. `EZ2DJ6th.EXE`는 `GetPrivateProfileIntA`와 `WritePrivateProfileStringA`를 import하므로 평문 INI를 읽는 경로가 있으며, 그 경로가 `bookkeeping.ini`만 대상으로 하는지 아니면 `EZ2DJ.INI`가 실행 중에 복호화되는지는 **미확정**이다.

*Confirmed — 2026-09-15. The 6th's `EZ2DJ/EZ2DJ.INI` (816 bytes) is not readable INI text, while `bookkeeping.ini` (162 bytes) in the same directory is plaintext and carries `[GAMEASSIGNMENTS]` and `[STATISTICS]` sections. `EZ2DJ6th.EXE` imports `GetPrivateProfileIntA` and `WritePrivateProfileStringA`, so a plaintext-INI path exists; whether that path targets only `bookkeeping.ini`, or `EZ2DJ.INI` is decrypted at runtime, is **unresolved**.*

**미확정.** 5th와 그 이전 제품의 `EZ2DJ.INI`는 모두 평문이다. 6th에서 이 형식이 바뀐 시점과 이유, 그리고 HLE가 이 파일을 어떻게 다뤄야 하는지는 확인되지 않았다.

*Unresolved. `EZ2DJ.INI` is plaintext in 5th and every earlier product. When and why the format changed in 6th, and how the HLE must handle the file, are not established.*

---

## 9. 보조 도구 / Auxiliary tools

9.1과 9.2는 1st SE 덤프에서 측정한 값이고, 9.3부터는 2026-09-15에 1st Tracks 덤프와 6th 동봉 배치에서 추가로 측정한 것이다.

*9.1 and 9.2 are measured on the 1st SE dump; 9.3 onward were added on 2026-09-15 from the 1st Tracks dump and the 6th's bundled layout.*

### 9.1 `Test.exe` — 서비스·테스트 도구 — 확인됨

entry RVA `0x0001ada0`(`.text`), SizeOfImage `0x001de000`, 섹션 여섯 개(`.text .rdata .data .idata .rsrc .reloc`). resource directory(`0x001c9000`)와 **비어 있지 않은** base relocation directory(`0x001ce000`, 크기 `0x0000c8d0`)가 있다. 즉 이 실행 파일은 재배치 가능하다. 캐비닛이 부팅에 쓰지 않는 서비스 도구다([HDD 레이아웃](ez2dj-hdd-layout.md) 5절).

*Entry 0x0001ada0 in `.text`, SizeOfImage 0x001de000, six sections including `.rsrc`, and a non-empty base-relocation directory — this service tool is relocatable and is not the cabinet's boot target.*

### 9.2 `PlzPowerOff.exe` — 종료 화면 — 확인됨

entry RVA `0x00001e6e`(`.text`), SizeOfImage `0x0001b000`, 섹션 네 개(`.text .rdata .data .rsrc`). characteristics가 `0x010f`로 다른 실행 파일(`0x010e`)과 다르다. 전원 종료 화면 표시용 소형 도구다.

*Entry 0x00001e6e in `.text`, SizeOfImage 0x0001b000, four sections, and characteristics 0x010f (vs 0x010e elsewhere) — a small shutdown-screen tool.*

---

### 9.3 1st Tracks의 보조 도구 / The 1st Tracks auxiliary tools — 확인됨

**확인됨 — 2026-09-15.** 1st Tracks 덤프의 `Test.exe`는 266,240바이트, PE TimeDateStamp `0x374d68b1`(1999-05-27)이고 entry RVA `0x0000fa6c`(`.text`), SizeOfImage `0x00056000`, 섹션 넷(`.text .rdata .data .rsrc`)이다. 1st SE의 `Test.exe`(1,859,633바이트, `0x38607297`)와는 **다른 프로그램**이다. 크기가 7배 차이이고 섹션 수와 빌드 시각이 모두 다르므로, 두 제품의 서비스 도구를 같은 것으로 다루지 않는다.

*Confirmed — 2026-09-15. The 1st Tracks dump's `Test.exe` is 266,240 bytes with PE TimeDateStamp `0x374d68b1` (1999-05-27), entry RVA `0x0000fa6c` in `.text`, SizeOfImage `0x00056000`, and four sections (`.text .rdata .data .rsrc`). It is a **different program** from the 1st SE `Test.exe` (1,859,633 bytes, `0x38607297`) — seven times the size apart, with a different section count and build time — so the two products' service tools are not treated as one.*

**확인됨 — 2026-09-15.** 1st Tracks의 `PlzPowerOff.exe`는 1st SE 것과 크기(98,304)·PE TimeDateStamp(`0x3700321a`)·entry RVA(`0x00001e6e`)·섹션 구성이 모두 같지만 해시가 다르다. 공통 특성 절의 해시 표를 참조한다.

*Confirmed — 2026-09-15. The 1st Tracks `PlzPowerOff.exe` matches the 1st SE copy in size (98,304), PE TimeDateStamp (`0x3700321a`), entry RVA (`0x00001e6e`) and section layout, but not in hash. See the hash table in the common-traits section.*

### 9.4 `AllowIo.exe`와 `PortTalk.sys` — legacy I/O 접근 도구 / legacy I/O access tools — 확인됨

**확인됨 — 2026-09-15.** 두 파일은 `roms/ez2dj1st/ez2dj1/`과 `6th.chd`의 `EZ2DJ/Ez2Dj1st/` 양쪽에 있고 바이트 단위로 같다. 이 문서가 다루는 실행 파일 가운데 image base가 `0x00400000`이 아닌 유일한 예이며, 콘솔 subsystem을 쓰는 것도 `AllowIo.exe`뿐이다.

*Confirmed — 2026-09-15. Both files appear in `roms/ez2dj1st/ez2dj1/` and in `EZ2DJ/Ez2Dj1st/` inside `6th.chd`, byte-identical. `AllowIo.exe` is the only executable covered here whose image base is not `0x00400000`, and the only one using the console subsystem.*

| 항목 / item | `AllowIo.exe` | `PortTalk.sys` |
| --- | --- | --- |
| 크기 / size | 40,125 | 3,567 |
| TimeDateStamp | `0x3c3fc787` (2002-01-12) | `0x3c3fdf10` (2002-01-12) |
| image base | `0x01000000` | `0x00010000` |
| entry point RVA | `0x000021a0` | `0x00000328` |
| SizeOfImage | `0x0000c000` | `0x00000c60` |
| subsystem | 3 (console) 4.0 | 1 (native) 5.0 |
| 섹션 / sections | `.text .data` | `.text .rdata .data INIT .rsrc .reloc` |
| section / file alignment | `0x00001000` / `0x00000200` | `0x00000020` / `0x00000020` |

**추정 — 2026-09-15.** `PortTalk.sys`는 커널 드라이버를 통해 사용자 모드 프로세스에 legacy I/O port 접근을 허용하는 공개 도구 계열이고, `AllowIo.exe`는 그 드라이버에 대상 프로세스를 등록하는 콘솔 도구로 보인다. 원본 게임 실행 파일이 이 경로를 실제로 요구하는지, 캐비닛 운영자가 나중에 더한 것인지는 **미확정**이다. 두 파일 모두 2002년 빌드로 1st Tracks(1999)와 6th(2004) 사이에 있다. HLE의 legacy port 경계는 [I/O port map](ez2dj-io-map.md)이 담당한다.

*Inferred — 2026-09-15. `PortTalk.sys` belongs to the well-known family of kernel drivers that grant legacy I/O port access to a user-mode process, and `AllowIo.exe` appears to be the console tool that registers a target process with it. Whether the original game executables actually require this path, or a cabinet operator added it later, is **unresolved**. Both files are 2002 builds, between 1st Tracks (1999) and 6th (2004). The HLE's legacy-port boundary is owned by the [I/O port map](ez2dj-io-map.md).*

---

## 10. 새 실행 파일 추가 절차 / Procedure for a new executable

1. `re2dj_pe_analyzer <file>`로 헤더·섹션·데이터 디렉터리를 확보하고 이 문서에 섹션을 추가한다. 골격은 기존 절 가운데 보호 여부가 같은 것을 따른다. 보호된 빌드는 1·3·4·6·7절, 보호되지 않은 빌드는 2·8절이다.
2. 보호 섹션이 보이면 import directory의 위치(원본 `.idata` 유지 여부, 패커 섹션 이동 여부)를 확인하고, 필요하면 슬롯 VA까지 해석해 [import 표면 분석](ez2dj-import-surface.md)에 기록한다.
3. 데이터 섹션의 문자열·blob 인벤토리를 VA와 함께 남긴다. 런타임 관찰 결과와 정적 값을 대조하는 열을 유지한다(2.4절 형식).
4. 런타임 흐름은 launcher probe 관찰 후 이 문서에 요약하고 근거 작업 로그를 링크한다. 확인됨/추정/미확정 표기를 유지한다.
5. `docs/analysis/README.md` 색인과 이 문서의 공통 특성 표를 같은 작업에서 갱신한다.

*1. Capture headers/sections/directories with `re2dj_pe_analyzer` and add a section here following the protected or unprotected skeleton. 2. For a protection section, locate the import directory and, if moved, resolve slot VAs into the import surface document. 3. Record string/blob inventories with VAs, keeping a runtime-vs-static comparison column. 4. Summarize runtime flow after launcher-probe observation and link the evidence work log. 5. Update the analysis README index and the common-traits table in the same task.*

---

## 관련 문서 / Related documents

* [EZ2DJ import 표면](ez2dj-import-surface.md)
* [HDD 레이아웃과 실행 파일 식별](ez2dj-hdd-layout.md)
* [보호 stub API 관찰 trace 작업 로그](../work-logs/20260823-042-protected-api-observation-trace.md)
* [PE32 실행 형식](../kb/pe32-executable-format.md)
* [원본 실행 파일 분석 (누적)](../EXE_DESIGN.ko.md)

## 4th Trax 실행 분석 — 2026-09-01

### 확인됨

실제 `re2dj ez2dj4th --run` 실행에서 Debug injected runtime 링크 문제를 수정한 뒤 launcher가 CHD staging executable을 직접 전달받는 경로를 사용했습니다. 진단 로그 `logs/windows_x86_launcher_probe/ez2dj4th/20260901-030400-180.jsonl`은 `EZ2DJ/EZ2DJ.EXE`의 PE32/i386 검증, CHD VFS mount 준비, injected runtime 준비, `USER32.dll!LoadImageA` image-loader 준비를 기록합니다.

### 미확정

첫 `CreateFileA` runtime handoff debug message는 제한 시간 안에 관찰되지 않았으며 launcher는 `runtime handoff was not observed before timeout`으로 child를 정리했습니다. 따라서 4th 보호 코드가 요청하는 첫 파일/API, Hardlock 단계, 화면 진입은 이 실행으로 확정하지 않습니다.

### Confirmed

In the real `re2dj ez2dj4th --run` execution, after fixing the Debug injected-runtime link and adding explicit CHD staging executable handoff, the launcher used the staged CHD executable path. Diagnostic log `logs/windows_x86_launcher_probe/ez2dj4th/20260901-030400-180.jsonl` records successful PE32/i386 validation, CHD VFS mount preparation, injected-runtime preparation, and `USER32.dll!LoadImageA` image-loader preparation.

### Unresolved

The first `CreateFileA` runtime handoff debug message was not observed before the bounded timeout, and the launcher cleaned up the child with `runtime handoff was not observed before timeout`. The first protected 4th file/API request, Hardlock stage, and screen entry therefore remain unconfirmed.

### 추가 bounded API trace — 확인됨 / Additional bounded API trace — Confirmed

작업 118에서 <code>ExitProcess</code> 정적 import가 없는 protected target도 API
breakpoint를 제한적으로 처리하도록 launcher 분석 경계를 확장했습니다. 실제 4th
staging 실행은 <code>GetProcAddress(kernel32, "GetVersion")</code>와
<code>GetProcAddress(kernel32, "CreateFileA")</code>를 순서대로 기록했고, 이후
child <code>0xc0000005</code> execute fault와
<code>api_trace_boundary(reason=child_exit)</code>를 남겼습니다. 이는 4th 보호
stub가 최소한 이 두 Win32 API를 동적으로 해석한다는 **확인됨** 사실입니다.
기존 정적 <code>CreateFileA</code> IAT patch만으로는 이 호출을 HLE wrapper에
연결하지 않는다는 점은 소스 구조와 trace를 함께 비교한 **추정**이며, 동적
resolver HLE를 적용한 뒤 정상 반환과 다음 경계를 다시 확인해야 합니다.

*Task 118 extended the launcher diagnostic boundary so a protected target with no
static <code>ExitProcess</code> import can process API breakpoints within a bound.
The real 4th staging run recorded <code>GetProcAddress(kernel32,
"GetVersion")</code> followed by <code>GetProcAddress(kernel32,
"CreateFileA")</code>, then a child <code>0xc0000005</code> execute fault and
<code>api_trace_boundary(reason=child_exit)</code>. This confirms that the 4th
protection stub dynamically resolves at least these two Win32 APIs. The inference
that the current static <code>CreateFileA</code> IAT patch does not cover this call
follows from comparing the source path with the trace; a dynamic-resolver HLE must
be tested before normal return and the next boundary can be confirmed.*

### 추가 동적 VFS resolver — 확인됨 / Additional dynamic VFS resolver — Confirmed

작업 119에서 <code>ez2dj4th</code>의 profile capability
<code>hle_dynamic_vfs</code>를 runtime export flag로 전달하고 원본
<code>GetProcAddress</code> IAT 2개를 injected thunk로 patch했습니다. 실제 CHD
trace의 VFS log는 <code>GetVersion:route=win32</code>와
<code>CreateFileA:route=hle</code>를 기록했습니다. 이는 동적 파일 API 결과가
기존 VFS wrapper로 들어가는 경계를 **확인됨**으로 올립니다.

다만 wrapper를 통한 asset-open 또는 보호 코드의 정상 후속 분기는 관찰되지
않았고 child는 <code>0xc0000005</code>로 종료했습니다. 따라서 보호 응답과
정상 게임 실행은 여전히 **미확정**입니다.

*Task 119 passed the <code>ez2dj4th</code> profile capability
<code>hle_dynamic_vfs</code> through a runtime export flag and patched two
original <code>GetProcAddress</code> IAT slots to the injected thunk. The real
CHD trace's VFS log recorded <code>GetVersion:route=win32</code> and
<code>CreateFileA:route=hle</code>, confirming that the dynamic file-API result
reaches the existing VFS wrapper boundary.*

*No asset-open or normal protected-code continuation was observed, and the
child exited with <code>0xc0000005</code>. The protection response and normal
game execution therefore remain unresolved.*

### 추가 caller instruction window — 확인됨 / Additional caller instruction window — Confirmed

작업 122에서 resolver caller 주변의 실행 중 memory를 읽었습니다.
<code>CreateFileA</code> caller는 <code>0x00af09f6</code>이고 window는
<code>0x00af09ee</code>부터 readable하게 읽혔습니다. caller 직후
<code>89 45 dc</code>는 반환 EAX를 <code>[EBP-0x24]</code>에 저장합니다.
이는 HLE wrapper 호출 자체가 아니라 반환값 저장 경계의 **확인됨** 증거입니다.
저장값을 소비하는 후속 code와 indirect call은 **미확정**입니다.

*Task 122 read the live memory around the resolver caller. The
<code>CreateFileA</code> caller is <code>0x00af09f6</code>, and the readable
window starts at <code>0x00af09ee</code>. The immediate bytes
<code>89 45 dc</code> store the returned EAX at <code>[EBP-0x24]</code>. This
confirms a return-value storage boundary, not an actual HLE-wrapper call. The
later consumer and any indirect call remain unresolved.*

### 추가 resolver 반환 주소 — 확인됨 / Additional resolver return addresses — Confirmed

작업 121에서 <code>GetVersion</code>과 <code>CreateFileA</code>의 resolver
반환 주소 및 원본 caller를 기록했습니다. <code>CreateFileA</code> 반환값
<code>0x62f5350d</code>는 runtime base <code>0x62f50000</code> 안에 있고,
<code>GetVersion</code> 반환값 <code>0x77451c10</code>은 kernel32 base
<code>0x77430000</code> 안에 있습니다. 반환 포인터의 module 범위는
**확인됨**이지만 wrapper request event가 없으므로 실제 호출과 ABI 호환은
**미확정**입니다.

*Task 121 recorded resolver return addresses and original callers for
<code>GetVersion</code> and <code>CreateFileA</code>. The
<code>CreateFileA</code> result <code>0x62f5350d</code> lies within runtime
base <code>0x62f50000</code>, and the <code>GetVersion</code> result
<code>0x77451c10</code> lies within kernel32 base
<code>0x77430000</code>. The module ranges are **confirmed**, but actual
invocation and ABI compatibility remain **unresolved** because no wrapper
request event was observed.*

### 추가 VFS open trace — 확인됨 / Additional VFS open trace — Confirmed

작업 120에서 <code>Re2djVfsCreateFileA</code> 진입 request와 주요 결과 stage를
bounded trace로 추가했지만, 실제 4th CHD 실행의 VFS log에는
<code>create-file:stage=request</code>가 없었습니다. 기존
<code>CreateFileA:route=hle</code>는 resolver 내부 event이므로 실제 wrapper
호출의 증거가 아닙니다. 따라서 protected stub의 반환 포인터 호출 여부와 첫
파일 open 결과는 **미확정**으로 유지합니다.

*Task 120 added bounded request and major-result stages for
<code>Re2djVfsCreateFileA</code>, but the real 4th CHD run produced no
<code>create-file:stage=request</code> event in the VFS log. The existing
<code>CreateFileA:route=hle</code> event is internal to the resolver and is not
evidence of an actual wrapper call. Whether the protected stub calls the
returned pointer and what the first file-open result is therefore remain
**unresolved**.*

### 추가 EIP=0 fault 호출 대상 귀속 — 확인됨 / Additional EIP=0 fault call attribution — Confirmed

작업 123에서 first-chance execute fault의 stack return address 직전
runtime bytes를 제한적으로 해석했습니다. 실제 CHD trace의 첫 stack return
address는 <code>0x00aef7fe</code>이고, 그 직전 bytes는
<code>FF 15 F4 0C AF 00 83 C4</code>였습니다. 따라서 fault 직전 호출은
<code>CALL DWORD PTR [0x00AF0CF4]</code>로 **확인됨**입니다.

해당 pointer slot은 child memory에서 readable했지만 현재 값은
<code>0x00000000</code>이었습니다. 진단 event는 target을 실행 가능한
module이나 section으로 귀속하지 않았고, child는 이어서
<code>EIP=0x00000000</code>, <code>0xc0000005</code> execute fault로
종료했습니다. 이는 zero-pointer indirect call 경계를 확정하지만,
slot이 0으로 남은 원인, 이전 continuation, HLE ABI 문제인지 보호 코드의
별도 초기화 문제인지는 **미확정**입니다.

*Task 123 conservatively decoded the runtime bytes immediately before the
first fault-stack return address. The real CHD trace returned to
<code>0x00aef7fe</code>, with preceding bytes
<code>FF 15 F4 0C AF 00 83 C4</code>, confirming a
<code>CALL DWORD PTR [0x00AF0CF4]</code> immediately before that return.

The pointer slot was readable in child memory and contained
<code>0x00000000</code>. The diagnostic did not attribute a runnable module or
section to the target, and the child then exited with an execute fault at
<code>EIP=0x00000000</code> and code <code>0xc0000005</code>. This confirms a
zero-pointer indirect-call boundary, but the reason the slot stayed zero, the
earlier continuation, and whether this is an HLE ABI issue or separate
protected-code initialization problem remain **unresolved**.*

### 추가 zero slot 참조 추적 — 확인됨 / Additional zero-slot reference trace — Confirmed

작업 124에서 live main image 전체를 bounded scan한 결과
<code>0x00AF0CF4</code> 주소 immediate가 12곳에서 확인됐습니다. 정식 PE
import table은 <code>0x00B16A60</code>, <code>0x00AE0F90</code>,
<code>0x00B193D0</code> 등의 별도 범위에 있으므로 이 slot은 정식 IAT가
아닙니다. HLE와 CHD VFS가 없는 native baseline에서도 slot은 0이었고 같은
<code>EIP=0</code> fault가 발생했으므로 현재 HLE가 zero slot을 직접 만든
것은 아닙니다.

runtime window에는 <code>0x00AEF5F0</code>, <code>0x00AEFE62</code>,
<code>0x00AF061A</code>에서 <code>A3 F4 0C AF 00</code>, 즉
<code>MOV [0x00AF0CF4], EAX</code> 기록 명령이 **확인**됩니다. 또한 여섯
indirect-call site와 세 zero 비교 site가 확인됩니다. 다만 fault 시점의
정적 window만 관찰했으므로 어느 기록 명령이 실제로 실행됐는지와 당시 EAX
값은 **미확정**입니다.

*Task 124 found 12 occurrences of the <code>0x00AF0CF4</code> address immediate
in a bounded scan of the live main image. Formal PE import tables occupy
separate ranges such as <code>0x00B16A60</code>, <code>0x00AE0F90</code>, and
<code>0x00B193D0</code>, so this slot is not a formal IAT entry. A native
baseline without HLE or CHD VFS retained the zero slot and produced the same
<code>EIP=0</code> fault, showing that the current HLE did not directly create
the zero value.

The runtime windows **confirm** <code>A3 F4 0C AF 00</code>, or
<code>MOV [0x00AF0CF4], EAX</code>, at <code>0x00AEF5F0</code>,
<code>0x00AEFE62</code>, and <code>0x00AF061A</code>. Six indirect-call sites
and three zero-comparison sites are also confirmed. Because this observation
only scans windows at fault time, which writer executed and the EAX value at
that moment remain **unresolved**.*

### 추가 slot writer 실행 결과 — 확인됨 / Additional slot-writer execution result — Confirmed

작업 125의 hardware execution breakpoint는 broad API software watch를 끈
CHD/VFS 실행과 native baseline 모두에서 <code>0x00AEFE62</code> writer를
포착했습니다. 두 실행 모두 instruction 실행 직전 EAX는
<code>0x00B17B00</code>, slot 값은 <code>0x00000000</code>이었고,
instruction bytes는 <code>a3f40caf00</code>이었습니다. CHD/VFS 실행의 idle
boundary에서 slot 현재 값은 <code>0x00B17B00</code>으로 확인됐습니다.

같은 CHD/VFS 실행에 40개 broad API software watch를 함께 적용하면 writer
hit은 0회였고 slot이 0인 채 기존 <code>EIP=0</code> fault가 재현됐습니다.
따라서 이전 zero-slot fault는 현재 HLE 자체의 일반 실행 결과가 아니라 broad
API breakpoint 진단이 보호 continuation을 바꾸는 **관찰 교란 조건**으로
확인합니다. 어느 개별 API watch 또는 breakpoint 처리 세부가 분기를 바꾸는지는
**미확정**입니다.

*Task 125's hardware execution breakpoints caught writer
<code>0x00AEFE62</code> in both CHD/VFS and native-baseline runs with broad API
software watches disabled. Immediately before the instruction, both runs had
EAX <code>0x00B17B00</code>, slot value <code>0x00000000</code>, and instruction
bytes <code>a3f40caf00</code>. At the CHD/VFS idle boundary, the current slot
value was <code>0x00B17B00</code>.

Enabling the 40 broad API software watches in the same CHD/VFS run produced no
writer hit and reproduced the earlier <code>EIP=0</code> fault with a zero slot.
The earlier zero-slot fault is therefore confirmed as an **observation
perturbation condition** from broad API-breakpoint tracing rather than the
normal result of the current HLE path. Which individual watch or breakpoint
handling detail changes the branch remains **unresolved**.*

### 1st SE 코인 입력 직후 종료와 directory-backed `FindFirstFileA` 회귀 / 1st SE exit after coin insertion and directory-backed `FindFirstFileA` regression

#### 확인됨 (Confirmed)

사용자 실행 `logs/windows_x86_launcher_probe/ez2dj1stse/20260906-000419-856.jsonl`의 VFS trace는 `SetCurrentDirectoryA("System\\Title")` 성공 직후 `find-first-fallback:name=*.*:success=1`을 기록하고, 이어서 `SetCurrentDirectoryA("logs")`를 `System/Title/logs`로 해석해 실패한다. `System\\Title`에는 실제 제목 화면 자산이 있고 root에는 별도의 `logs` 디렉터리가 있으므로, directory-backed VFS가 상대 검색 패턴을 호스트 작업 디렉터리 `ez2dj`에 그대로 넘긴 현재 동작은 잘못된 root 항목을 반환할 수 있다. 같은 실행의 DDraw trace에는 종료 직전 `DrawPrimitive` 실패나 unsupported blend 기록이 없다.

*Confirmed: The user run `logs/windows_x86_launcher_probe/ez2dj1stse/20260906-000419-856.jsonl` records `SetCurrentDirectoryA("System\\Title")` success, then `find-first-fallback:name=*.*:success=1`, followed by a `SetCurrentDirectoryA("logs")` request resolved as `System/Title/logs` and rejected. The actual `System\\Title` directory contains title-screen assets while the root contains a separate `logs` directory, so passing the relative search pattern unchanged to the host working directory `ez2dj` can return the wrong root entries. The same run has no DDraw `DrawPrimitive` failure or unsupported-blend record before it stops.*

#### 수정 및 판단 (Fix and assessment)

`FindFirstFileA`의 directory-backed 경로는 검색 패턴의 마지막 구성요소를 wildcard로 보존하고, 디렉터리 부분만 기존 게스트 경로 해석기와 HDD root에 통과시킨 뒤 native `FindFirstFileA`에 전달하도록 수정했다. `FindNextFileA`와 `FindClose`의 native handle 전달 및 CHD 합성 열거는 유지했다. 이 수정은 관측된 잘못된 root 열거를 **확인된 코드 결함**으로 확정하지만, 코인 입력 이후 게임이 계속 진행하는지 여부는 사용자 재실행 전까지 **미확정**이다.

*The directory-backed `FindFirstFileA` path now preserves the final wildcard component, resolves only the directory portion through the existing guest path resolver and HDD root, and passes the resulting native pattern to Win32 `FindFirstFileA`. Native `FindNextFileA`/`FindClose` forwarding and CHD synthetic enumeration remain unchanged. This confirms the wrong-root enumeration as a **code defect**, while whether it is the only condition needed for the game to continue after coin insertion remains **unresolved** until the user reruns the product.*

#### 미확정 (Unresolved)

- 새 빌드에서 `ez2dj1stse`가 코인 입력 후 Music Select까지 계속 진행하는지.
- overlay와 HDD 디렉터리 항목을 함께 열거해야 하는지.
- directory enumeration 회귀를 제거한 뒤에도 남는 보호 장치 또는 종료 경계가 있는지.

*Whether the new build reaches Music Select after coin insertion, whether overlay and HDD directory entries must be merged, and whether another protection or termination boundary remains after removing this enumeration regression are unresolved.*

---

## 2026-09-06 ez2dj2nd execution-boundary observations

### 확인됨 (Confirmed)

2nd의 첫 제품 실행 로그 `logs/windows_x86_launcher_probe/ez2dj2nd/20260906-020151-651.jsonl`에서는 1st SE용 `--demo-volume` 준비가 2nd 정적 IAT에 없는 `KERNEL32!GetPrivateProfileIntA`를 요구하여 handoff가 관찰되지 않았습니다. 후속 수정 후 로그 `20260906-022550-689.jsonl`에서 `DirectDrawCreateEx` HLE 준비와 `runtime_detached`가 확인됐지만, output helper가 아직 비어 있어 child가 `0xc0000096`으로 종료했습니다.

Attached diagnostic `20260906-022613-342.jsonl`에서 2nd의 실제 byte I/O helper는 input VA `0x004782d7` (RVA `0x000782d7`, `in al,dx`)와 output VA `0x0047832b` (RVA `0x0007832b`, `out dx,al`)로 확인됐습니다. 후속 attached run `20260906-022933-169.jsonl`은 두 주소의 privileged events를 처리했고 2,643개가 모두 first chance였으며 second chance는 0개였습니다. 이는 두 helper 주소가 현재 debugger/runtime 경계에서 처리되고 있음을 확인하지만, port의 물리적 의미나 2nd 전용 Hardlock 계약을 확정하지는 않습니다.

같은 실행의 DDraw trace는 `has_create_ex=true`, `create_ex_patched=true`와 실제 `DirectDrawCreateEx` HLE 호출을 기록했습니다. `preparation_status`의 준비 항목은 모두 true였습니다. product detached 실행은 child가 계속 실행되는 단계까지 도달했으며, 화면과 coin 이후 게임 진행 여부는 사용자의 실제 확인이 필요합니다.

### 미확정 (Unresolved)

- 2nd의 `id_ref`, `id_verify`, Hardlock device path와 응답 계약
- 2nd의 port별 물리 배선과 입력 의미
- detached product run이 coin 이후 Music Select까지 도달하는지

*Confirmed: `20260906-020151-651.jsonl` stopped before handoff because the 1st SE demo-volume preparation required a `KERNEL32!GetPrivateProfileIntA` import absent from the 2nd static IAT. After the profile correction, `20260906-022550-689.jsonl` prepared the `DirectDrawCreateEx` HLE and recorded `runtime_detached`, but the child exited with `0xc0000096` while the output helper was still unset.*

*The attached diagnostic `20260906-022613-342.jsonl` identifies the actual 2nd byte-I/O helpers as input VA `0x004782d7` (RVA `0x000782d7`, `in al,dx`) and output VA `0x0047832b` (RVA `0x0007832b`, `out dx,al`). The follow-up attached run `20260906-022933-169.jsonl` handled privileged events at both addresses: all 2,643 recorded events were first chance and none were second chance. This confirms the helper addresses for the current debugger/runtime boundary, but does not establish physical port meanings or the 2nd Hardlock contract.*

*The same run records `has_create_ex=true`, `create_ex_patched=true`, and actual `DirectDrawCreateEx` HLE calls. Every preparation item in `preparation_status` is true. The product detached run reached a continuously running child; whether coin insertion reaches Music Select still requires the user's visual confirmation.*
