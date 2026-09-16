# 작업 294 작업 로그 — 보호 빌드 덤프로 helper RVA 대조 / Task 294 work log — Cross-checking helper RVAs against protected-build dumps

작업 지시: [20260917-294-port-helper-dump-crosscheck.md](../work-orders/20260917-294-port-helper-dump-crosscheck.md)
선행: [작업 293](20260916-293-port-helper-signature-scan.md), [작업 292](20260916-292-decrypted-image-dump.md)
분석: [EZ2DJ I/O 포트 맵](../analysis/ez2dj-io-map.md)

## 한국어

### 실행

보호 빌드 여섯 개를 각각 `re2dj <target> --image-dump --image-dump-delay 6000`으로 실행했다. 3rd는 작업 292의 덤프를 썼다. 모든 실행에서 `entry`·`resumed` 두 덤프가 `gaps: 0`, `bytes_read = size_of_image`로 떠졌다.

| 타깃 | 이미지 크기 (바이트) |
| --- | --- |
| `ez2dj1st` | 26,959,872 |
| `ez2dj1stse` | 28,229,632 |
| `ez2dj3rd` | 6,799,360 |
| `ez2dj4th` | 7,446,528 |
| `ez2dj5th` | 7,626,752 |
| `ez2d2m` | 4,435,968 |

### 결과 — 바이트 폭 다섯 빌드 모두 일치 — 확인됨

| 타깃 | 덤프 `inportb` / `outportb` | 프로파일 in / out |
| --- | --- | --- |
| `ez2dj1st` | `0x00035757` / `0x0003577b` | 일치 |
| `ez2dj1stse` | `0x00038987` / `0x000389ab` | 일치 |
| `ez2dj3rd` | `0x000a9887` / `0x000a98bb` | 일치 |
| `ez2dj4th` | `0x000c3817` / `0x000c384b` | 일치 |
| `ez2dj5th` | `0x000ca067` / `0x000ca09b` | 일치 |
| `ez2d2m` | 없음 | 해당 없음 (아래) |

`entry` 덤프는 여섯 빌드 모두 0건이다. **작업 292의 "진입점은 복호화 이전" 결론이 3rd 한 빌드에서 여섯 빌드로 넓어졌다.**

어떤 프로파일 값도 바꿀 필요가 없다.

### 작업 293 가설의 판정 — 설명할 대상이 없어졌다

작업 293은 "1st SE와 5th의 값이 그 빌드에서 확인되지 않았고, 두 빌드가 `0xc0000096`에서 멈추므로 Hardlock이 아니라 I/O 경계 문제일 수 있다"고 세웠다. 두 전제가 모두 현재 사실과 다르다.

* **5th는 4th 값을 물려받지 않았다.** 프로파일 값 `0x000ca067`/`0x000ca09b`는 4th의 값과 다르고, 프로파일 주석과 blame(2026-09-10)이 보여 주듯 5th 자신의 fault window에서 읽은 값이다. 덤프가 재확인했다.
* **`0xc0000096` 정지는 과거 기록이다.** [1st·5th Hardlock descriptor 분석](../analysis/ez2dj1st-5th-hardlock-descriptors.md)의 해당 표는 2026-09-10 Hardlock 후보 판별 도중의 관찰이고, 그 직후 helper RVA가 설정되며 해소됐다. 이번 실행에서 1st SE는 572프레임, 5th는 618프레임을 크래시 없이 그렸다.

작업 293의 로그는 시간순 증거이므로 고치지 않고, 정정은 누적 문서인 I/O 맵과 이 로그에 둔다.

### 부수 확인 — helper 묶음의 출력 쪽 간격은 두 갈래다 — 확인됨

`inportb` 기준 간격은 `inportw` +7, `inportl` +15로 다섯 빌드가 같다. 출력 쪽은 1st·1st SE가 +36·+48, 3rd·4th·5th가 +52·+64다. 시그니처 탐색은 간격에 의존하지 않으므로 둘 다에서 동작한다. 처음 분석 문서에 "상대 배치가 모두 같다"고 적었다가 숫자를 다시 계산해 바로 고쳤다.

### `ez2d2m` — 시그니처 방식의 대상이 아니다 — 확인됨

확인된 출력 지점 `0x0000b565`는 `66 EF`(`out dx, ax`)로, CRT helper가 아니라 게임 함수 끝에 인라인된 명령이다. 0건은 스캐너 결함이 아니다.

같은 덤프에 `66 ED`(`in ax, dx`)가 `0x0000b169`와 `0x0000b4cb` 두 곳 있고, 둘 다 출력 지점과 같은 포트 표 `0x0044d410`을 순회하는 루프다. 입력 지점 **후보**로 기록했다(추정). 프로파일의 `legacy_io_in_rva = 0`이 opcode 판정으로 둘 다 처리하므로 값 변경은 필요 없다.

### 코드 변경

`src/target/target_profile.cpp`의 **설명 문자열 두 곳**만 고쳤다. `ez2dj1st`와 `ez2dj1stse`(CHD) 노트가 "helper RVA가 .gtide에서 옮겨 왔고 이 실행 파일에서 확인되지 않았다"고 적고 있었는데, 덤프 대조로 둘 다 확인됐다. 동작은 바뀌지 않는다.

### 검증

* Windows x86 Debug 빌드: 오류 0건.
* 단위 테스트: `checks: 1783, failures: 0`.
* CTest: `re2dj_windows_vfs_runtime_probe`를 제외한 5개 통과. 그 probe는 작업 293이 기록한 hang이다.

### 남은 것

* `ez2d2m` 입력 지점의 런타임 확인.
* 시그니처로 잡히지 않는 인라인 port I/O를 찾는 방법. 지금은 `66 ED`/`66 EF` 바이트 검색으로 후보를 냈을 뿐이다.

## English

Work order: [20260917-294-port-helper-dump-crosscheck.md](../work-orders/20260917-294-port-helper-dump-crosscheck.md)
Prerequisites: [Task 293](20260916-293-port-helper-signature-scan.md), [Task 292](20260916-292-decrypted-image-dump.md)
Analysis: [EZ2DJ I/O port map](../analysis/ez2dj-io-map.md)

### Runs

Each of six protected builds was run as `re2dj <target> --image-dump --image-dump-delay 6000`, with 3rd reusing task 292's dump. Every run produced both `entry` and `resumed` dumps with `gaps: 0` and `bytes_read = size_of_image`.

### Result — all five byte-width builds match — confirmed

The `inportb` and `outportb` addresses from each `resumed` dump equal the profile's `legacy_io_in_rva` and `legacy_io_out_rva` for `ez2dj1st` (`0x00035757` / `0x0003577b`), `ez2dj1stse` (`0x00038987` / `0x000389ab`), `ez2dj3rd` (`0x000a9887` / `0x000a98bb`), `ez2dj4th` (`0x000c3817` / `0x000c384b`) and `ez2dj5th` (`0x000ca067` / `0x000ca09b`). `ez2d2m` has no hit, explained below.

Every `entry` dump yields zero, which **widens task 292's "the entry point precedes decryption" from one build to six.** No profile value needs to change.

### Task 293's hypothesis — nothing left to explain

Task 293 proposed that the 1st SE and 5th values were unconfirmed for their builds and that, since both stop at `0xc0000096`, the cause might be the I/O boundary rather than Hardlock. Both premises are out of date. **5th never inherited 4th's values**: its `0x000ca067` / `0x000ca09b` differ from 4th's and, per its profile comment and blame (2026-09-10), were read from 5th's own fault window, which the dump reconfirms. And **the `0xc0000096` stop is history**: that table in the [1st/5th Hardlock descriptor analysis](../analysis/ez2dj1st-5th-hardlock-descriptors.md) was observed during the 2026-09-10 Hardlock candidate search and resolved when the helper RVAs were set right after. In these runs 1st SE drew 572 frames and 5th 618 without a crash.

Task 293's log is chronological evidence and is left as is; the correction lives in the cumulative I/O map and in this log.

### Incidental — the output side of the block comes in two layouts — confirmed

From `inportb`, `inportw` sits at +7 and `inportl` at +15 in all five builds; the output helpers sit at +36 and +48 in 1st and 1st SE but at +52 and +64 in 3rd, 4th and 5th. The signature search does not depend on the gap. The analysis first said the layout was identical everywhere; recomputing the numbers caught it and it was corrected immediately.

### `ez2d2m` — outside the signature method — confirmed

Its confirmed output site `0x0000b565` is `66 EF` (`out dx, ax`) inlined at the tail of a game function, not a CRT helper, so zero hits is not a scanner defect. The same dump holds `66 ED` (`in ax, dx`) at `0x0000b169` and `0x0000b4cb`, both loops over the same port table `0x0044d410` the output site uses; they are recorded as input **candidates** (inferred). The profile's `legacy_io_in_rva = 0` already handles both by opcode, so no value changes.

### Code change

Only **two note strings** in `src/target/target_profile.cpp`: the `ez2dj1st` and CHD `ez2dj1stse` notes said the helper RVAs were carried over from `.gtide` and unconfirmed for that executable, and the cross-check confirms both. No behavior changes.

### Verification

* Windows x86 Debug build: no errors.
* Unit tests: `checks: 1783, failures: 0`.
* CTest: 5 pass excluding `re2dj_windows_vfs_runtime_probe`, the hang task 293 recorded.

### Remaining

* Run-time confirmation of the `ez2d2m` input site.
* A way to find inlined port I/O the signatures miss; today's candidates come from a plain `66 ED` / `66 EF` byte search.
