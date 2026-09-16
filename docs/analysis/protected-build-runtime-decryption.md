# 보호 빌드의 런타임 복호화 / Runtime decryption in protected builds

주제: `.protect` packer로 보호된 빌드가 실행 중 어느 시점에 무엇을 복호화하는가, 그리고 그 상태를 어떻게 확보하는가.

*Topic: when and what a `.protect`-packed build decrypts at run time, and how that state is captured.*

근거 작업: [작업 292](../work-logs/20260916-292-decrypted-image-dump.md)

관련 문서: [실행 파일 구조](ez2dj-exe-structures.md), [import 표면](ez2dj-import-surface.md)

절차: [실행 중 주 이미지 덤프](../guides/decrypted-image-dump.md)

---

## 1. 확인됨: 디스크 이미지에는 원본 문자열이 없다 / Confirmed: the disk image holds no original strings

3rd `EZ2DJ.EXE`(CHD 빌드, TimeDateStamp `0x3bca98a3`) 파일 전체를 바이트 검색한 결과다. 이 게임이 자기 INI에 매 실행 기록하는 키 이름들이다.

*Byte-searching the whole of 3rd's `EZ2DJ.EXE` (CHD build, TimeDateStamp `0x3bca98a3`) for the key names that game writes into its own INI on every run.*

| 문자열 / string | 디스크 파일 / disk file | 보호되지 않은 6th `EZ2DJ6th.EXE` / unprotected |
| --- | --- | --- |
| `TotalCoin` | 0 | 1 |
| `UseGameOver` | 0 | 1 |
| `AdvSound` | 0 | 1 |

보호된 빌드는 정적 분석으로는 아무것도 볼 수 없다.

*Nothing in a protected build is visible to static analysis.*

## 2. 확인됨: 진입점 breakpoint는 복호화 **이전**이다 / Confirmed: the entry breakpoint is **before** decryption

이것이 작업 292가 미확정으로 열어 둔 질문이었고, 두 시점의 덤프를 비교해 답이 나왔다.

*This was the open question in task 292's design, answered by comparing dumps from two points.*

3rd를 같은 실행에서 두 번 떴다. 지점 `entry`는 진입점 breakpoint 복원 직후 프로세스가 아직 정지해 있을 때, 지점 `resumed`는 재개 후 8초 뒤다.

*One run, two dumps: `entry` immediately after the entry breakpoint was restored while the process was still stopped, and `resumed` eight seconds after the resume.*

| 측정 / measurement | `entry` | `resumed` |
| --- | --- | --- |
| `TotalCoin` | 0 | **4** |
| `UseGameOver` | 0 | **2** |
| `AdvSound` | 0 | **4** |
| `UseIOCard` | 0 | **1** |
| `TestSongName` | 0 | **2** |
| 0이 아닌 바이트 / non-zero bytes | 1,196,837 | **2,279,626** |

두 덤프는 이미지 6,799,360 바이트 중 **2,469,020 바이트(36.3%)가 다르다.** 0이 아닌 바이트가 거의 두 배가 된다.

*The two dumps differ in **2,469,020 of the image's 6,799,360 bytes (36.3%)**, and the non-zero byte count nearly doubles.*

**따라서 packer stub은 진입점 breakpoint 시점에 아직 실행되지 않았다.** 이 구조는 PE 헤더와도 일치한다. 3rd의 `entry_point_rva`는 `0x00642240`이고 `size_of_image`는 `0x0067c000`이므로, 진입점이 이미지 거의 끝의 stub 섹션에 있다.

***The packer stub therefore has not run at the entry breakpoint.*** *The PE header agrees: 3rd's `entry_point_rva` of `0x00642240` against a `size_of_image` of `0x0067c000` places the entry in a stub section near the end of the image.*

실무상 결론은 명확하다. **분석용 덤프는 `resumed` 지점만 쓴다.** `entry` 덤프는 복호화 여부를 판정하는 대조군으로서의 가치만 있다.

*The practical conclusion is clear: **only the `resumed` point is useful for analysis.** The `entry` dump is worth keeping only as the control that establishes whether decryption happened.*

## 3. 확인됨: 덤프는 파일과 충실하다 / Confirmed: the dump is faithful to the file

덤프 경로 자체가 옳은지를 확인한 결과다. PE 헤더는 보호 계층이 건드리지 않고 파일 오프셋과 RVA가 같으므로, 파일과 덤프가 그 영역에서 일치해야 한다.

*Validating the dump path itself. The PE headers are untouched by the protection and share file offset with RVA, so file and dump must agree there.*

| 비교 / comparison | 결과 / result |
| --- | --- |
| 파일 `[0, 0x400)` == `entry` 덤프 같은 범위 | 일치 / match |
| 파일 `[0, 0x400)` == `resumed` 덤프 같은 범위 | 일치 / match |
| 읽기 실패 범위 / failed ranges | 없음 / none (`gaps: []`) |

덤프는 virtual 레이아웃이므로 **파일 오프셋이 곧 RVA다.** 진단 로그의 주소나 프로파일의 helper RVA를 덤프에서 그대로 찾을 수 있다.

*The dump keeps virtual layout, so **a file offset is the RVA**: an address from a diagnostic log or a profile's helper RVA can be looked up in it directly.*

## 4. 확인됨: `AutoPlay`는 EZ2DJ의 INI 키가 아니다 / Confirmed: `AutoPlay` is not an EZ2DJ INI key

`resumed` 덤프에서도 `AutoPlay` 문자열은 **0건**이다. 같은 덤프가 다른 INI 키 이름들을 2~4건씩 담고 있으므로, 이 0은 덤프의 한계가 아니라 사실이다.

*`AutoPlay` appears **zero times** even in the `resumed` dump, while that same dump carries the other INI key names two to four times each. The zero is a fact, not a limit of the dump.*

원본 INI 세 사본(`roms/ez2dj3rd/EZ2DJ.INI`, `roms/ez2dj3rd/ez2dj/EZ2DJ.INI`, overlay)에도 이 키는 없다. 보호되지 않은 6th `EZ2DJ6th.EXE`에서도 0건이다.

*None of the three original INI copies carries the key either, and the unprotected 6th `EZ2DJ6th.EXE` has zero occurrences as well.*

**INI를 통한 autoplay 제어 경로는 존재하지 않는다.** 게임이 그 키를 파싱하지 않는다.

***There is no autoplay control path through the INI***: the game does not parse that key.

## 5. 확인됨: 이 INI는 Win32 profile API 형식이 아니다 / Confirmed: this INI is not in Win32 profile-API format

2nd·3rd·5th·6th의 INI는 섹션 헤더가 없고 키가 따옴표로 감싸여 있다. `GetPrivateProfileIntA`는 `[section]`을 요구하므로 이 파일을 읽을 수 없다. 게임이 자체 파서를 가진다는 뜻이다.

*The 2nd, 3rd, 5th and 6th INIs have no section headers and quote their keys. `GetPrivateProfileIntA` requires a `[section]` and cannot read them, so the game carries its own parser.*

| 제품 / product | 섹션 수 / sections | 형식 / format |
| --- | --- | --- |
| 1st Tracks | 6 | Win32 profile |
| 1st SE | 9 | Win32 profile |
| 2nd, 3rd, 5th | 0 | 자체 / own |

따라서 이 제품군에는 `ini_profile_hle` 경계가 닿지 않는다. 1st·1st SE의 `DemoVolume` 재정의는 그 두 제품이 진짜 INI 형식을 쓰기 때문에 성립한다.

*The `ini_profile_hle` boundary therefore does not reach this family. The `DemoVolume` override works for 1st and 1st SE because those two do use the real INI format.*

## 6. 미확정 / Unresolved

* 복호화가 단계적인지. `resumed` 덤프가 이미지의 전부를 담았는지는 확인하지 않았다. 지연 시간을 늘려 가며 떠서 더 늘어나는지 보면 알 수 있다.
* `entry`와 `resumed` 사이 어느 시점에 복호화가 끝나는지. 둘 사이를 좁혀 보지 않았다.
* 3rd 외 보호 빌드(1st Tracks·4th·5th·`ez2d2m`)의 동일 여부. 구조가 같을 가능성이 높으나 확인하지 않았다.
* autoplay를 제어하는 실제 변수의 위치. 덤프로 탐색 경로가 열렸을 뿐 탐색은 하지 않았다.

*Whether decryption is staged, and so whether the `resumed` dump holds the whole image — taking dumps at increasing delays would show it. Where between `entry` and `resumed` decryption completes, which was not narrowed. Whether the other protected builds behave identically, which is likely but unverified. And where the variable that controls autoplay actually lives: the dump opens the search but the search was not run.*
