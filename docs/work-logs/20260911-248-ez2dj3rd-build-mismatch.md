# 작업 로그: ez2dj3rd 기준선 검증과 빌드 불일치

## 한국어

### 관련 문서

- 분석: [ez2dj3rd CHD 파일시스템 관찰](../analysis/ez2dj3rd-chd-filesystem.md)의 빌드 불일치 절
- 배경: [16비트 폭 legacy I/O 경계](20260910-244-word-width-legacy-io.md)

### 요구사항

`ez2dancer-word-io` 브랜치 검토 중 드러난 `ez2dj3rd`의 transform 전건 unmapped 현상을 확인합니다.

코드는 바꾸지 않았습니다. 관측과 문서 갱신뿐입니다.

### 확인한 것

**CHD 빌드와 추출본 빌드는 서로 다른 실행 파일입니다.** CHD 안 `EZ2DJ/EZ2DJ.EXE`는 `952fc4c1…`, 추출본은 `e370ca0d…`입니다.

**로컬 map은 추출본 쪽입니다.** `cfg/hardlock-ez2dj3rd.map` 28행은 CHD 빌드의 고유 challenge 26개와 **교집합이 0**이고, 추출본 빌드에서는 32건 전부 매핑됩니다.

따라서 `re2dj ez2dj3rd`는 이 머신에서 보호를 통과하지 못합니다. 프로파일이 CHD shortcut이라 제품 경로가 CHD 빌드를 실행하는데, 그 빌드용 map이 없습니다.

**추출본 빌드도 raw I/O에서 멈춥니다.** 트랩되지 않은 `0xc0000096`이 RVA `0x000a96c7`에서 발생하며, 이는 프로파일의 `0x000a9887`/`0x000a98bb` 어느 쪽도 아닙니다.

### 회귀 여부 — 회귀 아님

`main`을 빌드해 추출본 빌드를 두 번 실행한 기준선과 브랜치를 대조했습니다.

| | trace 줄 | transform 매핑 | `0xc0000096` RVA |
| --- | --- | --- | --- |
| `main` | 342 | 32 | `0x000a96c7` |
| 브랜치 | 354 | 32 | `0x000a96c7` |

12줄 차이는 브랜치가 추가한 exit 진단(`exit-stack-chain` 1줄, `exit-stack-code` 11줄)이 전부입니다. 정지 지점과 매핑 결과는 동일합니다.

### 앞선 회귀 검증의 한계 정정

작업 244에서 byte 경로 회귀를 `ez2dj3rd`로 확인했다고 적었습니다. 그때 쓴 것은 **CHD 빌드**인데, 그 빌드는 보호를 통과하지 못해 raw I/O에 도달하지 않습니다. 즉 그 검증은 byte trap 경로를 실제로 실행하지 않았습니다. 결과가 변하지 않았다는 관측 자체는 유효하지만, "byte 경로에 회귀가 없다"는 근거로는 약했습니다.

이번에 추출본 빌드로 다시 확인했고, 그 실행은 실제로 privileged fault 경로를 지납니다. 두 빌드 모두 `main`과 브랜치가 동일하므로 회귀 없음은 이제 근거를 갖췄습니다.

### 미확정

- `0x000a96c7`이 세 번째 helper인지, 등록된 RVA가 다른 빌드의 것인지.
- CHD 빌드용 map 확보 가능성. 두 빌드의 관계는 확인하지 않았습니다.

## English

### Related documents

- Analysis: the build-mismatch section of [ez2dj3rd CHD filesystem observations](../analysis/ez2dj3rd-chd-filesystem.md)
- Background: [the word-width legacy I/O boundary](20260910-244-word-width-legacy-io.md)

### Requirement

Check the all-transforms-unmapped behaviour of `ez2dj3rd` noticed while reviewing the `ez2dancer-word-io` branch. No code changed; this is observation and documentation.

### What was established

The CHD and extracted 3rd executables differ — `952fc4c1…` against `e370ca0d…` — and the local `cfg/hardlock-ez2dj3rd.map` belongs to the extracted one: its 28 rows share no challenge at all with the CHD build's 26 unique challenges, while all 32 transforms map on the extracted build.

`re2dj ez2dj3rd` therefore does not pass its protection here, because the profile is a CHD shortcut and no map for that build exists locally.

The extracted build stops at raw I/O as well, on an untrapped `0xc0000096` at RVA `0x000a96c7`, which is neither of the profile's registered helper RVAs.

### Regression status — not a regression

`main` was built and the extracted build run twice as a baseline: 342 trace lines, 32 transforms mapped, faulting at `0x000a96c7`. The branch gives 354 lines with the same 32 mapped and the same fault RVA, the twelve extra lines being exactly the exit diagnostic the branch adds.

### Correcting the limits of the earlier regression check

Task 244 recorded a byte-path regression check against `ez2dj3rd`. That check used the **CHD build**, which never passes its protection and so never reaches raw I/O — it did not exercise the byte trap at all. The observation that nothing changed was still valid, but it was weak evidence for "the byte path did not regress".

Re-checking against the extracted build, whose run does go through the privileged-fault path, gives that claim real backing: `main` and the branch agree on both builds.

### Unresolved

Whether `0x000a96c7` is a third helper site or the registered RVAs belong to another build; and whether a map can be produced for the CHD build, since the relationship between the two builds was not examined.

---

## 추가 2026-09-11: 정정 / Addendum 2026-09-11: correction

### 한국어

이 로그의 결론 두 가지가 뒤집혔습니다. 저장소 소유자가 CHD 빌드에 맞는 map을 배치했습니다.

**"`re2dj ez2dj3rd`는 보호를 통과하지 못한다"는 자료가 없었던 것이지 대상 선택이 틀린 것이 아닙니다.** CHD 빌드가 프로파일의 대상이고 올바른 선택입니다. 새 map 26행이 CHD 빌드의 런타임 고유 challenge 26개와 전부 일치하며, 그 상태에서 transform 32건이 모두 매핑되고 자산 9건을 열며 crash 없이 attract 루프를 돕니다.

**`0x000a96c7`은 세 번째 helper가 아니었습니다.** 프로파일의 `0x000a9887`/`0x000a98bb`는 CHD 빌드의 값이고 정상 동작합니다. 그 fault는 추출본 빌드의 것이었습니다.

두 실행 파일의 MD5는 CHD가 `bb447ee2581f77d340d416d2daf090ab`, 추출본이 `58f38d14ffd50d79307775b44c26166a`이고 크기는 1,216,512 바이트로 같습니다.

### 회귀 검증을 제대로 마쳤습니다

이 실행은 byte trap을 실제로 통과하므로, 이 로그가 "근거가 약했다"고 적은 작업 244의 검증을 여기서 완결합니다.

| | trace 줄 | `asset-open` | read | transform 매핑 | crash |
| --- | --- | --- | --- | --- | --- |
| `main` 2회 | 1543, 1545 | 9 | 474 | 32 | 0 |
| 브랜치 2회 | 1797, 1797 | 9 | 474 | 32 | 0 |

read 474건이 정확히 일치합니다. 줄 수 차이는 브랜치가 추가한 io-port trace 256줄입니다.

### 남은 것

- 두 3rd 빌드의 관계. 크기가 같고 내용만 다른 이유는 확인하지 않았습니다.
- 추출본 빌드의 helper RVA. 이제 프로파일 대상이 아니므로 우선순위는 낮습니다.

## English

Two conclusions in this log are reversed, after the repository owner placed a map matching the CHD build.

"`re2dj ez2dj3rd` does not pass its protection" was a missing-material problem, not a wrong target: the CHD build is the profile's target and the correct choice. The new map's 26 rows match all 26 of the CHD build's unique runtime challenges, and with it all 32 transforms map, nine assets open, and the guest runs its attract loop without a crash.

`0x000a96c7` was not a third helper. The profile's `0x000a9887` and `0x000a98bb` are the CHD build's values and work; that fault belonged to the extracted build.

The two executables are MD5 `bb447ee2581f77d340d416d2daf090ab` (CHD) and `58f38d14ffd50d79307775b44c26166a` (extracted), both 1,216,512 bytes.

### The regression check is now complete

This run really exercises the byte trap, so it completes the task 244 check this log described as weakly evidenced. Two `main` runs give 1543 and 1545 lines with 9 assets, 474 reads, 32 mapped and no crash; two branch runs both give 1797 lines with the same 9 assets, the same 474 reads, the same 32 mapped and no crash. The line difference is the branch's 256 io-port trace lines.

### What remains

The relationship between the two 3rd builds — equal size, differing contents — was not examined. The extracted build's helper RVAs matter less now that it is not the profile's target.
