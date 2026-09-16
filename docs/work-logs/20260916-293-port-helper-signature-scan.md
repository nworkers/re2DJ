# 작업 293 작업 로그 — legacy I/O helper 시그니처 탐색 / Task 293 work log — Locating legacy I/O helpers by signature

설계: [20260916-293-port-helper-signature-scan.md](../design/20260916-293-port-helper-signature-scan.md)
작업 지시: [20260916-293-port-helper-signature-scan.md](../work-orders/20260916-293-port-helper-signature-scan.md)
선행: [작업 292 복호화된 주 이미지 덤프](20260916-292-decrypted-image-dump.md)
분석: [EZ2DJ I/O 포트 맵](../analysis/ez2dj-io-map.md)

## 한국어

### 발단

작업 292가 "여러 `docs/analysis/` 항목이 보호되어 정적 분석 불가에 걸려 있었고 이제 검색 경로가 생겼다"고 남겼고, 그 예로 legacy I/O helper RVA 문제를 지목했다. 이 작업은 그 경로를 실제로 만든다.

### 구현

| 계층 | 변경 |
| --- | --- |
| `re2dj::exe` | `PortHelperKind`, `PortHelperSite`, `PortHelperKindName`, `ScanPortHelpers` |
| 도구 | `src/tools/port_helper_scan/main.cpp` — 오프라인 CLI |
| 테스트 | `tests/unit/port_helper_scan_test.cpp` 6개 블록 |
| CMake | 도구와 테스트 등록 |

기존 `code_scan` 모듈에 넣었다. 이 모듈은 이미 "구문적 탐색이며 결과는 후보"라는 계약과 `max_sites`/`capped`/`total_sites` 관례를 갖고 있어 새 함수가 그대로 맞았다.

### 확인된 사실 — helper는 묶음이고 시그니처가 있다 — 확인됨

1st SE 정식 빌드(`.gtide`, 디스크 평문 `.text`)의 프로파일 값 주변을 직접 읽었다.

```
0x00038980:  33 c0 66 8b 54 24 04 ec c3
             xor eax,eax; mov dx,[esp+4]; in al,dx; ret
0x000389a2:  66 8b 54 24 04 8a 44 24 08 ee c3
             mov dx,[esp+4]; mov al,[esp+8]; out dx,al; ret
```

두 helper는 고립된 함수가 아니라 컴파일러 런타임 `inp`/`outp` 계열이 폭별로 연속 배치된 한 묶음의 일부였다. 다섯 가지 형태를 확인했고 표는 [I/O 포트 맵](../analysis/ez2dj-io-map.md)에 두었다.

opcode 위치는 명령의 **시작**으로 잡았다. `66` operand-size 접두사가 있으면 그 접두사를 가리킨다. privileged fault가 보고하는 주소와 프로파일이 저장하는 값이 그것이기 때문이다. 구현 중 `outportw`를 `ef` 바이트 기준 +11로 잘못 잡았다가 단위 테스트가 잡아냈고, 접두사 기준 +10으로 정정했다. 같은 이유로 설계 표의 `inportw` 주소도 `0x0003898f`에서 `0x0003898e`로 고쳤다.

### 검증 3 — 프로파일 값을 되찾았다 — 확인됨

```
$ re2dj_port_helper_scan roms/ez2dj1stse/ez2dj/ez2dj.exe
helpers found : 5
  inportb    0x00038980         0x00038987
  inportw    0x00038989         0x0003898e
  inportl    0x00038991         0x00038996
  outportb   0x000389a2         0x000389ab
  outportw   0x000389ad         0x000389b7
```

`inportb`의 `0x00038987`과 `outportb`의 `0x000389ab`는 `ez2dj1stse` 프로파일의 `legacy_io_in_rva`·`legacy_io_out_rva`와 **정확히 일치**한다. 값을 알고 시작한 탐색이 아니라 시그니처만으로 같은 주소에 도달했으므로 회귀 기준으로 쓸 수 있다.

### 검증 4 — 보호 빌드는 디스크에서 0건 — 확인됨

| 빌드 | 시그니처 |
| --- | --- |
| 3rd `EZ2DJ.EXE` (CHD) | 0건 |
| 4th `EZ2DJ.exe` (CHD) | 0건 |
| `ez2d2m` `EZ2Dancer.exe` | 0건 |

`.text`가 디스크에서 암호문이라는 뜻이며 작업 292의 결론과 일치한다. 도구는 이 경우를 오류가 아니라 사실로 보고하고 종료 코드 `1`을 준다.

### 무엇이 드러났는가

프로파일 값의 확인 상태를 정리하니 **`ez2dj1stse`와 `ez2dj5th`의 값이 그 빌드에서 확인된 것이 아니다.** 1st SE는 추출된 `.gtide` 빌드의 값을 CHD `.protect` 빌드에 쓰고 있고, 5th는 4th 값을 물려받았다.

[1st·5th Hardlock descriptor 분석](../analysis/ez2dj1st-5th-hardlock-descriptors.md)은 두 빌드가 transform loop를 넘긴 뒤 정확히 `0xc0000096`, 곧 트랩되지 않은 port I/O에서 멈춘다고 기록한다. 두 사실을 겹치면 **Hardlock 응답이 틀린 것이 아니라 I/O 경계가 준비되지 않은 것일 수 있다**는 가설이 선다. 이는 **추정**이며, 각 빌드의 `resumed` 덤프 대조로 확인해야 한다.

### 검증

* Windows x86 Debug 빌드: 오류 0건.
* Windows x86 Release 전체 빌드: 오류 0건. 서드파티 SDL 헤더의 기존 `C4819` 코드 페이지 경고는 이 작업과 무관하게 그대로다.
* 단위 테스트: `checks: 1783, failures: 0`. 작업 292 시점의 1752에서 31개 늘었다.
* Release CTest에서 `re2dj_windows_vfs_runtime_probe`를 제외한 **5개 전부 통과**했다.
* **관찰 보정 — 그 probe는 실패가 아니라 hang이다.** 작업 291·292는 이를 "기존 실패"로 기록했으나, 단독 실행에서 60초 안에 종료하지 않았고 CTest 안에서는 10분 넘게 진행되지 않았다. 이 작업의 변경은 `code_scan`에 함수를 추가하기만 했고 기존 동작을 바꾸지 않으며 그 probe는 새 함수를 쓰지 않으므로, 원인은 이 작업 밖에 있다. 별도 확인 대상으로 남긴다.
* 검증 3·4는 위에 기록했다.

### 범위에서 뺀 것

* **프로파일 RVA 값 변경.** 불일치를 발견해도 기록만 한다. 값 교체는 런타임 확인을 거쳐 별도 작업으로 한다.
* **실제 덤프 수집과 보호 빌드 대조.** 실행이 필요하므로 분리했다. `docs/TODO.md`의 다음 작업에 올렸다.
* 역어셈블러 도입.
* helper가 읽고 쓰는 port의 의미.

### 남은 한계

시그니처 탐색은 구문적이므로 결과는 후보다. 게임이 실제로 그 helper를 부르는지는 런타임 fault 관찰이 확정한다. 시그니처 자체도 1st SE 한 빌드에서 확인한 형태이므로 다른 컴파일러나 최적화 수준에서는 다를 수 있고, 맞지 않는 빌드가 나오면 그 빌드의 실제 바이트를 근거로 항목을 추가한다.

## English

Design: [20260916-293-port-helper-signature-scan.md](../design/20260916-293-port-helper-signature-scan.md)
Work order: [20260916-293-port-helper-signature-scan.md](../work-orders/20260916-293-port-helper-signature-scan.md)
Prerequisite: [Task 292, decrypted main-image dump](20260916-292-decrypted-image-dump.md)
Analysis: [EZ2DJ I/O port map](../analysis/ez2dj-io-map.md)

### Where this came from

Task 292 noted that several `docs/analysis/` items had been stuck on "protected, so static analysis is impossible" and now had a search path, naming the legacy I/O helper RVA question as one. This task builds that path.

### Implementation

| Layer | Change |
| --- | --- |
| `re2dj::exe` | `PortHelperKind`, `PortHelperSite`, `PortHelperKindName`, `ScanPortHelpers` |
| Tool | `src/tools/port_helper_scan/main.cpp`, an offline CLI |
| Tests | Six blocks in `tests/unit/port_helper_scan_test.cpp` |
| CMake | Tool and test registration |

It went into the existing `code_scan` module, which already carries the "syntactic search, results are candidates" contract and the `max_sites`/`capped`/`total_sites` convention, so the new function fit as it was.

### Confirmed — the helpers come as a block with a signature

Reading around the profile values in the 1st SE canonical build (`.gtide`, plaintext `.text` on disk):

```
0x00038980:  33 c0 66 8b 54 24 04 ec c3
             xor eax,eax; mov dx,[esp+4]; in al,dx; ret
0x000389a2:  66 8b 54 24 04 8a 44 24 08 ee c3
             mov dx,[esp+4]; mov al,[esp+8]; out dx,al; ret
```

Neither helper is an isolated function: both belong to one block of the compiler runtime's `inp`/`outp` family laid out by width. Five shapes were confirmed and the table lives in the [I/O port map](../analysis/ez2dj-io-map.md).

The opcode offset points at the **start** of the instruction, including a `66` operand-size prefix when present, because that is what a privileged fault reports and what a profile stores. `outportw` was first written as +11 against the `ef` byte; the unit test caught it and it was corrected to +10 against the prefix. The design table's `inportw` address was corrected from `0x0003898f` to `0x0003898e` for the same reason.

### Verification 3 — the profile values come back

```
$ re2dj_port_helper_scan roms/ez2dj1stse/ez2dj/ez2dj.exe
helpers found : 5
  inportb    0x00038980         0x00038987
  inportw    0x00038989         0x0003898e
  inportl    0x00038991         0x00038996
  outportb   0x000389a2         0x000389ab
  outportw   0x000389ad         0x000389b7
```

`inportb`'s `0x00038987` and `outportb`'s `0x000389ab` match the `ez2dj1stse` profile's `legacy_io_in_rva` and `legacy_io_out_rva` **exactly**. The search did not start from those values, so reaching the same addresses from the signature alone makes this a usable regression baseline.

### Verification 4 — protected builds yield nothing on disk

Zero signatures in 3rd `EZ2DJ.EXE`, 4th `EZ2DJ.exe` and `ez2d2m` `EZ2Dancer.exe`, meaning `.text` is ciphertext on disk, consistent with task 292. The tool reports this as a fact rather than an error and exits `1`.

### What it exposed

Laying out the confirmation status showed that **the `ez2dj1stse` and `ez2dj5th` values were never confirmed against their own build**: 1st SE runs the CHD `.protect` build with values from the extracted `.gtide` build, and 5th inherited 4th's.

The [1st and 5th Hardlock descriptor analysis](../analysis/ez2dj1st-5th-hardlock-descriptors.md) records both builds stopping at exactly `0xc0000096` — untrapped port I/O — past the transform loop. Together these suggest that **the Hardlock response may not be wrong at all and the I/O boundary may simply not be prepared**. That is **inferred** and has to be settled by comparing each build's `resumed` dump.

### Verification

* Windows x86 Debug build: no errors.
* Windows x86 Release full build: no errors. The pre-existing `C4819` code-page warnings in third-party SDL headers are unchanged and unrelated.
* Unit tests: `checks: 1783, failures: 0`, up 31 from the 1752 at task 292.
* Release CTest passed **all five** tests with `re2dj_windows_vfs_runtime_probe` excluded.
* **Corrected observation — that probe hangs rather than failing.** Tasks 291 and 292 recorded it as a pre-existing failure, but it did not exit within 60 seconds standalone and made no progress for over ten minutes inside CTest. This task only adds a function to `code_scan` without changing existing behavior, and that probe does not use the new function, so the cause lies outside this work. It is left as a separate item to confirm.
* Verifications 3 and 4 are recorded above.

### Excluded from scope

* **Changing profile RVA values.** A mismatch is recorded only; replacing a value needs run-time confirmation and its own task.
* **Collecting real dumps and comparing protected builds**, separated because it needs a run and queued in `docs/TODO.md`.
* Introducing a disassembler.
* The meaning of the ports these helpers read and write.

### Remaining limits

The search is syntactic, so its results are candidates, and run-time fault observation still settles whether the game calls a given helper. The signatures are the shape confirmed in one build and may differ under another compiler or optimization level; a build that does not match earns a new entry grounded in its own bytes.
