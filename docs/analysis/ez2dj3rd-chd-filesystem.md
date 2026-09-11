# ez2dj3rd CHD 파일시스템 분석

## 한국어

### 확인된 구조

사용자가 제공한 `roms/ez2dj3rd/ez2dj3rd.chd`를 `re2dj_chd_probe`로 읽었습니다. 원본 CHD와 실행 파일은 저장소에 추가하지 않습니다.

- CHD v5, logical bytes `10,262,568,960`, hunk `4096`, unit `512`
- codecs `lzma,zlib,huff,flac`
- GDDD metadata `CYLS:19885,HEADS:16,SECS:63,BPS:512`
- FAT32 partition LBA `63`, partition sectors `20,032,992`
- sectors per cluster `16`, reserved sectors `32`, sectors per FAT `9773`
- data LBA `19641`, root cluster `2`, cluster count `1,250,838`
- volume label `NO NAME`
- 내부 실행 파일 `EZ2DJ/EZ2DJ.EXE`, first cluster `742547`, size `1,216,512`
- PE machine `i386`, magic `PE32`, subsystem `windows-gui`, image base `0x00400000`, entry RVA `0x00642240`, sections `6`

이 CHD는 현재 `roms/ez2dj3rd/ez2dj/EZ2DJ.EXE`를 대상으로 하던 3rd profile과 동일한 대표 실행 파일 경로·크기·PE 식별값을 제공합니다. 전체 바이트 동일성이나 Hardlock 계약의 동일성은 이 probe만으로 확정하지 않습니다.

### 실행 연결

`ez2dj3rd` built-in profile은 이제 CHD shortcut으로 동작합니다. launcher는 CHD의 FAT32에서 `EZ2DJ/EZ2DJ.EXE`를 조회하고, Windows x86 original-process backend에 CHD 경로와 staging 실행 파일 경로를 함께 전달합니다. 기존 3rd HLE/Hardlock 정책은 그대로 유지됩니다.

## English

### Confirmed structure

The user-supplied `roms/ez2dj3rd/ez2dj3rd.chd` was read with `re2dj_chd_probe`. The original CHD and executable are not added to the repository.

- CHD v5, 10,262,568,960 logical bytes, 4,096-byte hunks, 512-byte units
- codecs `lzma,zlib,huff,flac`
- GDDD metadata `CYLS:19885,HEADS:16,SECS:63,BPS:512`
- FAT32 partition LBA 63 with 20,032,992 sectors
- 16 sectors per cluster, 32 reserved sectors, 9,773 sectors per FAT
- data LBA 19,641, root cluster 2, and 1,250,838 clusters
- volume label `NO NAME`
- `EZ2DJ/EZ2DJ.EXE`, first cluster 742,547, size 1,216,512 bytes
- PE i386/PE32 Windows GUI, image base `0x00400000`, entry RVA `0x00642240`, six sections

The CHD exposes the same representative executable path, size, and PE identity used by the previous extracted-directory 3rd profile. Complete byte identity and an identical Hardlock contract are not established by this probe alone.

### Execution connection

The built-in `ez2dj3rd` profile now uses the CHD shortcut. The launcher resolves `EZ2DJ/EZ2DJ.EXE` through the CHD FAT32 view and passes both the CHD path and staging executable path to the Windows x86 original-process backend. The existing 3rd HLE/Hardlock policy remains unchanged.

---

## 2026-09-11 CHD 빌드와 로컬 Hardlock 자료의 불일치 / The CHD build does not match the local Hardlock material

### 한국어

`ez2d2m` 작업 중 `ez2dj3rd`를 byte 경로 회귀 기준선으로 쓰다가 확인한 사실입니다.

#### 확인됨 — 두 3rd 실행 파일은 서로 다릅니다

| 위치 | SHA-256 앞자리 |
| --- | --- |
| CHD 안 `EZ2DJ/EZ2DJ.EXE` | `952fc4c1…` |
| 추출본 `roms/ez2dj3rd/ez2dj/EZ2DJ.EXE` | `e370ca0d…` |

#### 확인됨 — 로컬 map은 추출본 쪽입니다

`cfg/hardlock-ez2dj3rd.map`은 28행이고, 실행별 transform challenge와 이렇게 대응합니다.

| 실행 대상 | transform 요청 | 매핑 |
| --- | --- | --- |
| CHD 빌드 | 32건, 고유 challenge 26개 | **0건 매핑.** map의 28개 challenge와 교집합이 0 |
| 추출본 빌드 | 32건 | **32건 전부 매핑** |

**따라서 `re2dj ez2dj3rd`는 이 머신에서 보호를 통과하지 못합니다.** `ez2dj3rd` 프로파일은 CHD shortcut이므로 제품 경로가 CHD 빌드를 실행하는데, 그 빌드에 맞는 map이 로컬에 없습니다. 3rd를 "해결된 제품"으로 인용할 때는 어느 빌드를 말하는지 구분해야 합니다.

#### 확인됨 — 추출본 빌드도 raw I/O에서 멈춥니다

map이 맞는 추출본 빌드로 실행하면 transform 32건이 모두 매핑되고 Hardlock 요청이 72건까지 가지만, 트랩되지 않은 privileged instruction으로 멈춥니다.

| 항목 | 값 |
| --- | --- |
| 예외 | `0xc0000096` |
| fault RVA | `0x000a96c7` |
| `edx` | `0x00a20103` (port `0x0103`) |
| 프로파일의 설정값 | in `0x000a9887`, out `0x000a98bb` |

fault 지점이 프로파일에 등록된 두 helper 중 어느 것도 아닙니다. **세 번째 helper 지점이거나, 등록된 값이 이 빌드의 것이 아닙니다.** 어느 쪽인지는 미확정입니다.

이 정지는 `main`에서도 같은 RVA로 재현되므로 word 폭 경계 작업의 회귀가 아닙니다. 두 실행의 유일한 차이는 브랜치가 추가한 exit 진단 12줄입니다.

#### 미확정

- `0x000a96c7`이 세 번째 helper인지, 등록된 RVA가 다른 빌드의 것인지.
- CHD 빌드용 map을 만들 수 있는지. 추출본과 CHD 빌드의 관계(리비전 차이인지 별개 릴리스인지)는 확인하지 않았습니다.

### English

This surfaced while using `ez2dj3rd` as a byte-path regression baseline during the `ez2d2m` work.

#### Confirmed — the two 3rd executables differ

The CHD's `EZ2DJ/EZ2DJ.EXE` hashes `952fc4c1…` while the extracted `roms/ez2dj3rd/ez2dj/EZ2DJ.EXE` hashes `e370ca0d…`.

#### Confirmed — the local map belongs to the extracted build

`cfg/hardlock-ez2dj3rd.map` has 28 rows. Against the CHD build, the guest issues 32 transform requests over 26 unique challenges and **none map** — the intersection with the map's challenges is empty. Against the extracted build, **all 32 map**.

**`re2dj ez2dj3rd` therefore does not pass its protection on this machine**: the profile is a CHD shortcut, so the product path runs the CHD build, and no map for that build exists locally. Citing 3rd as a solved product has to say which build is meant.

#### Confirmed — the extracted build stops at raw I/O too

Run against the build its map fits, all 32 transforms map and Hardlock traffic reaches 72 requests, but the run stops on an untrapped privileged instruction: `0xc0000096` at fault RVA `0x000a96c7`, with `edx` `0x00a20103` (port `0x0103`). The profile registers `0x000a9887` for input and `0x000a98bb` for output, so the faulting site is neither. **It is either a third helper site or the registered values belong to a different build**; which is unresolved.

The stop reproduces at the same RVA on `main`, so it is not a regression from the word-width boundary work — the only difference between the two runs is the twelve exit-diagnostic lines the branch adds.

#### Unresolved

Whether `0x000a96c7` is a third helper or the registered RVAs are from another build; and whether a map can be produced for the CHD build, since the relationship between the two builds — revision difference or separate release — was not examined.

---

## 2026-09-11 정정: CHD 빌드가 올바른 대상이고, 실행됩니다 / Correction: the CHD build is the right target, and it runs

### 한국어

바로 위 절의 결론 두 가지를 정정합니다. 저장소 소유자가 CHD 빌드에 맞는 map을 `cfg/hardlock-ez2dj3rd.map`에 배치한 뒤 확인한 결과입니다.

#### 정정 1 — 자료가 없던 것이지 대상이 틀린 것이 아닙니다

| 항목 | 위 절의 서술 | 실제 |
| --- | --- | --- |
| `re2dj ez2dj3rd` | "이 머신에서 보호를 통과하지 못한다" | 맞는 map을 놓으면 **통과합니다** |
| 올바른 대상 | 어느 빌드인지 불명확 | **CHD 빌드가 프로파일의 대상이고 올바른 선택입니다** |

새 map은 26행이고 CHD 빌드의 런타임 고유 challenge 26개와 **전부 일치**합니다. 두 실행 파일은 크기가 1,216,512 바이트로 같지만 내용이 다릅니다.

| 위치 | MD5 |
| --- | --- |
| CHD 안 `EZ2DJ/EZ2DJ.EXE` | `bb447ee2581f77d340d416d2daf090ab` |
| 추출본 `roms/ez2dj3rd/ez2dj/EZ2DJ.EXE` | `58f38d14ffd50d79307775b44c26166a` |

크기가 같고 내용만 다르다는 점은 별개 릴리스보다 같은 빌드의 변형을 시사합니다. `.protect` packer가 실행 파일 내용에서 challenge를 유도하므로 몇 바이트 차이로도 challenge 집합 전체가 바뀝니다. 두 빌드의 관계는 여전히 **미확정**입니다.

#### 정정 2 — `0x000a96c7`은 세 번째 helper가 아닙니다

프로파일에 등록된 `0x000a9887`(in)과 `0x000a98bb`(out)은 **CHD 빌드의 값이고 정상 동작합니다.** 위 절에서 본 `0x000a96c7` fault는 추출본 빌드의 것이었습니다. 빌드가 다르면 helper 주소도 다릅니다.

#### 확인됨 — 3rd가 CHD에서 실행됩니다

| 항목 | 맞지 않는 map | 맞는 map |
| --- | --- | --- |
| transform 매핑 | 0 / 32 | **32 / 32** |
| `asset-open` | 0 | **9** |
| crash | 1 (`0xc0000096`) | **0** |
| 종료 | `ExitProcess` | **없음.** idle timeout까지 계속 실행 |

게스트는 `System/AmuseLogo/LOGO.str`, `System/Title/Title.str`, `System/Title/InsertCoin.str`, `System/Title/Press.str`, `System/Common/2PLAYERInsertCoin.str`, `System/ez2catch/title/title-c.str` 등을 CHD에서 읽습니다. attract 루프입니다.

byte I/O 경로도 완전히 동작합니다. 입력 port `0x101`–`0x106`을 각 26회 읽고 출력 port `0x100`–`0x103`에 각 25회 씁니다. privileged fault는 없습니다.

#### 확인됨 — word 폭 경계 작업의 회귀 없음

이 실행이 byte trap을 실제로 통과하므로, 작업 244의 회귀 검증을 여기서 제대로 할 수 있습니다.

| | trace 줄 | `asset-open` | read | transform 매핑 | crash |
| --- | --- | --- | --- | --- | --- |
| `main` 2회 | 1543, 1545 | 9 | 474 | 32 | 0 |
| 브랜치 2회 | 1797, 1797 | 9 | **474** | 32 | 0 |

read 474건이 정확히 일치하고 자산·매핑·crash가 모두 같습니다. 줄 수 차이는 브랜치가 추가한 io-port trace 256줄입니다.

### English

This corrects two conclusions of the section above, after the repository owner placed a map matching the CHD build in `cfg/hardlock-ez2dj3rd.map`.

#### Correction 1 — the material was missing, not the target wrong

The section above said `re2dj ez2dj3rd` does not pass its protection on this machine. With the right map it **does**, and the CHD build is the profile's target and the correct choice. The new map's 26 rows match all 26 of the CHD build's unique runtime challenges.

The two executables are the same size, 1,216,512 bytes, with different contents: the CHD's is MD5 `bb447ee2581f77d340d416d2daf090ab` and the extracted one `58f38d14ffd50d79307775b44c26166a`. Equal size with differing contents suggests a variant of one build rather than a separate release, and since the `.protect` packer derives challenges from the executable's contents, a few bytes change the whole challenge set. The relationship between the two builds remains **unresolved**.

#### Correction 2 — `0x000a96c7` is not a third helper

The profile's registered `0x000a9887` for input and `0x000a98bb` for output **are the CHD build's values and work correctly**. The `0x000a96c7` fault seen above belonged to the extracted build; different builds place their helpers at different addresses.

#### Confirmed — 3rd runs from its CHD

Where the mismatched map gave 0 of 32 transforms mapped, zero assets, one `0xc0000096` crash and an `ExitProcess`, the matching map gives **32 of 32 mapped, nine `asset-open`s, no crash, and no exit at all** — the guest keeps running until the idle timeout.

It reads `System/AmuseLogo/LOGO.str`, `System/Title/Title.str`, `System/Title/InsertCoin.str`, `System/Title/Press.str`, `System/Common/2PLAYERInsertCoin.str` and `System/ez2catch/title/title-c.str` from the CHD: the attract loop. The byte I/O path runs fully too, reading input ports `0x101` to `0x106` twenty-six times each and writing output ports `0x100` to `0x103` twenty-five times each, with no privileged fault.

#### Confirmed — no regression from the word-width work

Because this run really does exercise the byte trap, task 244's regression check can finally be made here properly. Two `main` runs give 1543 and 1545 trace lines with 9 assets, 474 reads, 32 transforms mapped and no crash; two branch runs both give 1797 lines with the same 9 assets, the same **474** reads, the same 32 mapped and no crash. The line difference is the 256 io-port trace lines the branch adds.
