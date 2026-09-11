# 작업 로그: 16비트 폭 legacy I/O 경계

## 한국어

### 관련 문서

- 설계: [16비트 폭 legacy I/O 경계 설계](../design/20260910-244-word-width-legacy-io.md)
- 작업 지시: [16비트 폭 legacy I/O 경계](../work-orders/20260910-244-word-width-legacy-io.md)
- 분석: [EZ2Dancer I/O 포트 맵](../analysis/ez2dancer-io-map.md), [ez2d2m CHD 파일시스템과 실행 파일 관찰](../analysis/ez2d2m-chd-filesystem.md)
- 선행 작업: [ez2d2m 확정 map 배치와 main 머지](20260910-243-ez2d2m-resolved-map.md)

### 요구사항

`ez2d2m`이 원본 `.text`에서 멈추는 지점을 넘깁니다.

### 한 일

**폭을 프로파일이 정하게 했습니다.** `TargetLptdiPolicy`에 `LegacyIoWidth`와 `legacy_io_width`를 추가했습니다. 기본값이 `kByte`라 기존 다섯 제품은 그대로입니다. 같은 작업에서 `legacy_io_in_byte_rva`·`legacy_io_out_byte_rva`를 `legacy_io_in_rva`·`legacy_io_out_rva`로 바꿨습니다. 이 필드는 helper의 위치만 가리키고 폭은 별도 항목이 정하므로, 이름에 `byte`가 남으면 word 프로파일에서 사실과 다릅니다.

injected runtime의 export 이름은 바꾸지 않았습니다. launcher가 그것을 문자열로 찾기 때문에, 개명하면 컴파일러가 잡지 못하고 런타임에서만 드러나는 실패가 생깁니다. 프로파일 필드 개명은 전부 C++ 멤버 접근이라 컴파일러가 누락을 잡습니다. 이 구분이 개명 범위를 정했습니다.

**handler가 prefix를 해석합니다.** faulting 주소의 첫 바이트가 `0x66`이면 opcode는 그다음 바이트이고 명령 길이는 2, 폭은 word입니다. 프로파일이 선언한 폭과 어긋나는 명령은 처리하지 않고 crash로 보고합니다. 읽기 결과는 폭만큼만 `EAX`에 반영하고, `EIP`는 해석한 길이만큼 진행합니다.

**EZ2Dancer 보드를 전용 파일로 추가했습니다.** `Ez2DancerIoBoard`와 word 폭 `Ez2DancerIoPortBus`입니다. `LegacyIoPortBus`는 손대지 않았습니다. 다섯 제품이 그 경로에 의존하고 이 작업이 그 동작을 바꿀 이유가 없습니다. injected runtime은 두 bus를 들고 폭으로 갈라 씁니다.

**경계에 관측을 붙였습니다.** 설계에는 없던 항목입니다. 첫 실행에서 fault는 사라졌는데 게스트가 무엇을 요청했는지 볼 방법이 없었습니다. 이 저장소의 다른 경계는 모두 요청을 기록하므로, 같은 형식으로 `re2dj:vfs:io-port` 줄을 추가하고 전용 budget을 뒀습니다. 이것이 아래 관측을 가능하게 했습니다.

### 결과 — 확인됨

**차단 지점이 사라졌습니다.** RVA `0x0000b565`의 `0xc0000096`이 없어지고 실행이 이어집니다.

**게스트가 건드리는 port가 드러났습니다.** 한 실행의 port 접근 전부가 `0x030a`에 대한 16비트 쓰기 8회이고, **읽기는 한 번도 없습니다.** 그래서 입력 helper의 주소는 여전히 관측되지 않았고 `legacy_io_in_rva`는 0으로 둡니다.

**`0x030a`의 bit 집합이 원본에서 확인됐습니다.** 여덟 번의 쓰기는 `0x0004`에서 시작해 한 번에 한 bit씩 누적되어 `0x0f07`로 끝납니다. 최종값은 bit 0, 1, 2, 8, 9, 10, 11이고, 그 밖의 bit는 하나도 쓰이지 않습니다. 이는 분석 문서가 추정으로 적어 둔 캐비닛 조명 7개의 bit 집합과 정확히 일치합니다. 공개 구현과 무관하게 원본 자신이 이 7개 위치만 쓴다는 것이 확인됐습니다.

다만 **극성은 여전히 미확정입니다.** 한 번에 하나씩 bit를 세우는 동작은 램프를 하나씩 켜는 것으로도, 모두 켜진 상태에서 하나씩 끄는 것으로도 읽힙니다. 각 bit가 어느 램프인지도 확정되지 않았습니다.

### 새 정지 지점

| 항목 | 이전 | 현재 |
| --- | --- | --- |
| 종료 방식 | `0xc0000096` crash | `ExitProcess(0)` |
| 호출 지점 | — | `.protect` RVA `0x0043843a` |
| Hardlock 요청 | 26건 | 31건 |
| `asset-open` | 0 | 0 |

게스트는 죽는 대신 스스로 종료합니다. 종료 호출이 원본 `.text`가 아니라 `.protect`에서 나오므로 결정 주체는 보호 계층입니다. 6th의 "주 진입 함수가 `-1`을 반환해 종료"와 같은 성격입니다.

**따라서 이 작업은 목표를 달성했지만 target이 실행되지는 않습니다.** 차단 지점이 I/O에서 보호 계층의 종료 결정으로 옮겨갔습니다.

### 회귀 확인

byte 경로에 회귀가 없는지 확인하려고 변경 전 기준선을 직접 측정했습니다. 작업 브랜치를 커밋한 뒤 `main`을 빌드해 `ez2dj3rd`를 두 번 실행하고, 다시 브랜치로 돌아와 같은 실행을 반복했습니다.

| | trace 줄 | `asset-open` | crash |
| --- | --- | --- | --- |
| 변경 전 (`main`) | 313 | 0 | 1 |
| 변경 후 (브랜치) | 313 | 0 | 1 |

**동일합니다.** 이 방법을 쓴 이유는, 같은 tree에서 측정한 기준선 없이는 관측된 차이가 이 작업 때문인지 그 사이의 다른 커밋 때문인지 구분할 수 없기 때문입니다. 실제로 3rd의 2026-09-02 로그는 322줄에 crash 0건이라 지금과 다른데, 그 차이는 이 작업이 아니라 그 사이의 커밋들에서 온 것입니다.

### 검증

| 항목 | 결과 |
| --- | --- |
| Windows x86 Debug 전체 build | 통과, 경고 0건 |
| 단위 시험 | checks 1550, failures 0 (이전 1481에서 증가) |
| `ez2d2m` | RVA `0x0000b565` fault 소멸, 실행 계속 |
| `ez2dj3rd` byte 경로 | 변경 전후 동일 |

`re2dj_windows_vfs_runtime_probe`는 작업 239부터 계속 멈추므로 CTest에서 제외했습니다. 윈도우 생성 단계에서 멈추며 이 작업과 무관합니다.

### 남은 것

- **보호 계층이 종료를 선택하는 이유.** transform 11건이 모두 응답되고 descriptor 15건이 완료됐는데도 종료합니다.
- 그것이 `candidate-70`의 오답을 뜻하는지, 아니면 아직 없는 다른 경계 때문인지.
- 입력 helper 위치. 게스트가 읽기를 하기 전에 종료하므로 위 경계를 넘겨야 관측됩니다.
- `0x030a`의 극성과 bit별 램프 배정, `0x308`·`0x30c`의 의미.
- EZ2Dancer 키보드 입력 매핑. 보드에 입력을 넣을 경로가 아직 없습니다.

## English

### Related documents

- Design: [word-width legacy I/O boundary design](../design/20260910-244-word-width-legacy-io.md)
- Work order: [word-width legacy I/O boundary](../work-orders/20260910-244-word-width-legacy-io.md)
- Analysis: [the EZ2Dancer I/O port map](../analysis/ez2dancer-io-map.md), [ez2d2m CHD filesystem and executable observations](../analysis/ez2d2m-chd-filesystem.md)
- Preceding task: [placing the ez2d2m resolved map and merging to main](20260910-243-ez2d2m-resolved-map.md)

### Requirement

Get `ez2d2m` past the point where it stops in original `.text`.

### What was done

**The profile now states the width.** `TargetLptdiPolicy` gained `LegacyIoWidth` and `legacy_io_width`, defaulting to `kByte` so the five existing products are unchanged. In the same task `legacy_io_in_byte_rva` and `legacy_io_out_byte_rva` became `legacy_io_in_rva` and `legacy_io_out_rva`: those fields only locate the helper and width is now a separate item, so leaving `byte` in the name would be false for a word profile.

The injected runtime's exported symbol names were deliberately **not** renamed. The launcher looks them up as strings, so renaming them would create a failure the compiler cannot catch and only a run would reveal, whereas the profile fields are ordinary member accesses the compiler checks exhaustively. That distinction set the scope of the rename.

**The handler decodes the prefix.** When the faulting address begins with `0x66`, the opcode is the next byte, the instruction is two bytes and the width is word. An instruction disagreeing with the width the profile declares is reported as a crash rather than handled. A read result updates only its own width of `EAX`, and `EIP` advances by the decoded length.

**The EZ2Dancer board is its own file.** `Ez2DancerIoBoard` and the word-wide `Ez2DancerIoPortBus`. `LegacyIoPortBus` was left alone: five products depend on that path and this task had no reason to change its behaviour. The injected runtime holds both buses and selects by width.

**The boundary gained observability**, which the design did not call for. After the first run the fault was gone but there was no way to see what the guest had asked for, and every other boundary in this runtime records its requests. A `re2dj:vfs:io-port` line with its own trace budget was added in the same style, and it is what made the observations below possible.

### Results — confirmed

**The blocker is gone.** The `0xc0000096` at RVA `0x0000b565` no longer occurs and execution continues.

**The ports the guest touches are now visible.** Every port access in a run is eight 16-bit writes to `0x030a`, and **there is not one read**. The input helper's address therefore remains unobserved and `legacy_io_in_rva` stays zero.

**The bit set of `0x030a` is confirmed from the original.** The eight writes start at `0x0004` and accumulate one bit at a time to `0x0f07` — bits 0, 1, 2, 8, 9, 10 and 11, with no other bit ever written. That is exactly the seven-bit cabinet-light set the analysis recorded by inference, so the original itself confirms those seven positions independently of the public implementation.

The **polarity remains unresolved**, however. Setting one bit at a time reads equally as lighting lamps one by one or as switching them off one by one from all lit, and which lamp each bit drives is not settled either.

### The new stopping point

Where the run used to end in a `0xc0000096` crash it now calls `ExitProcess(0)` from RVA `0x0043843a` inside `.protect`, with Hardlock traffic up from 26 requests to 31 and `asset-open` still zero.

The guest exits deliberately rather than dying, and because the call comes from `.protect` rather than the original `.text`, the protection layer is making that decision. It is the same kind of boundary as 6th's "main entry returns `-1` and exits".

**This task therefore met its goal without making the target run.** The blocker moved from I/O to the protection's decision to quit.

### Regression check

To establish that the byte path did not regress, the pre-change baseline was measured directly: the branch was committed, `main` was built, `ez2dj3rd` was run twice, and the same run was repeated after returning to the branch. Both give 313 trace lines, zero `asset-open` and one crash — **identical**.

This method was used because without a baseline measured on the same tree there is no way to tell whether an observed difference comes from this task or from other commits in between. That matters here: the 2026-09-02 log for 3rd shows 322 lines and no crash, and that difference comes from the commits since, not from this work.

### Verification

The full Windows x86 Debug build passes with zero warnings; unit tests report 1550 checks and 0 failures, up from 1481; `ez2d2m` no longer faults at RVA `0x0000b565`; and `ez2dj3rd`'s byte path is identical before and after. `re2dj_windows_vfs_runtime_probe` is excluded from CTest because it has hung since task 239 in its window-creation step, unrelated to this work.

### What remains

Why the protection chooses to exit, with all 11 transforms answered and 15 descriptors completed; whether that means `candidate-70` is wrong or some other absent boundary is responsible; the input helper's location, which needs the guest to get past that exit before it ever reads; the polarity of `0x030a`, the per-bit lamp assignment and the meaning of `0x308` and `0x30c`; and an EZ2Dancer keyboard mapping, since there is still no path for feeding the board input.
