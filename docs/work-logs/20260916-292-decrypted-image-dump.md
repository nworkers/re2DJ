# 작업 292 작업 로그 — 복호화된 주 이미지 덤프 / Task 292 work log — Decrypted main-image dump

설계: [20260916-292-decrypted-image-dump.md](../design/20260916-292-decrypted-image-dump.md)
작업 지시: [20260916-292-decrypted-image-dump.md](../work-orders/20260916-292-decrypted-image-dump.md)
분석: [보호 빌드의 런타임 복호화](../analysis/protected-build-runtime-decryption.md)
절차: [실행 중 주 이미지 덤프](../guides/decrypted-image-dump.md)

## 한국어

### 구현

| 계층 | 변경 |
| --- | --- |
| Windows | `process_image_dump.h/.cpp` 신규 — 페이지 단위 `ReadProcessMemory`, virtual 레이아웃 출력, 구멍 기록, 귀속 sidecar, FNV-1a 64 다이제스트 |
| 런처 probe | `--image-dump [path]`, `--image-dump-delay <ms>`, 지점 `entry`·`resumed` 두 곳과 진단 한 줄씩 |
| 프로파일·backend·제품 CLI | `image_dump`, `image_dump_delay_ms` 전달과 `--image-dump` |
| CMake | 새 소스와 backend 라이브러리의 `RE2DJ_VERSION` 정의 |

### 설계에서 바꾼 것 — 덤프 주체

설계는 처음에 주입 런타임이 자기 주소 공간을 뜨는 쪽이었다. HLE 경계 어디에나 시점을 둘 수 있다는 것이 이유였다. **구현 지점을 확인하다 그 전제가 깨졌다.**

프로파일마다 활성화되는 HLE 경계가 다르다. 3rd는 `DirectDrawCreate`를 IAT에 갖고 있지 않아 그 hook을 아예 주입하지 않는다. **모든 프로파일에서 확실히 불리는 in-process 경계가 없다.** 경계별로 분기를 두면 프로파일이 늘 때마다 덤프가 조용히 동작하지 않는 조합이 생긴다.

런처는 두 지점 모두에서 `child.hProcess`를 쥐고 있고 프로파일과 무관하다. 설계를 고치고 근거를 그 문서에 남겼다.

### 실험 결과 — 진입점은 복호화 **이전**이다 — 확인됨

설계가 미확정으로 열어 둔 핵심 질문이 답을 얻었다.

3rd를 한 실행에서 두 번 떴다. `--image-dump-delay 8000`이다.

| 측정 | `entry` | `resumed` |
| --- | --- | --- |
| `TotalCoin` | 0 | **4** |
| `UseGameOver` | 0 | **2** |
| `AdvSound` | 0 | **4** |
| `UseIOCard` | 0 | **1** |
| `TestSongName` | 0 | **2** |
| 0이 아닌 바이트 | 1,196,837 | **2,279,626** |

두 덤프는 6,799,360 바이트 중 **2,469,020 바이트(36.3%)가 다르다.**

`resumed` 덤프가 담은 문자열들은 **디스크 파일에 0건**이다. 따라서 이 덤프는 복호화된 상태를 떴다. 그리고 `entry` 덤프에는 하나도 없으므로 **진입점 breakpoint 시점에는 packer stub이 아직 실행되지 않았다.**

PE 헤더도 같은 말을 한다. 3rd의 `entry_point_rva`는 `0x00642240`, `size_of_image`는 `0x0067c000`으로 진입점이 이미지 거의 끝의 stub 섹션에 있다.

**실무 결론: 분석에는 `resumed`만 쓴다.**

### 덤프 충실성 — 확인됨

| 비교 | 결과 |
| --- | --- |
| 파일 `[0, 0x400)` == `entry` 덤프 | 일치 |
| 파일 `[0, 0x400)` == `resumed` 덤프 | 일치 |
| 읽기 실패 범위 | 없음 (`gaps: []`) |
| `bytes_read` | 6,799,360 = `size_of_image` 전부 |

PE 헤더는 보호 계층이 건드리지 않고 파일 오프셋과 RVA가 같으므로, 이 일치가 덤프 경로의 충실성을 직접 증명한다.

### 부수 확인 — `AutoPlay`는 EZ2DJ의 INI 키가 아니다 — 확인됨

이 작업의 발단이 된 질문에 대한 답이 덤프에서 바로 나왔다. `resumed` 덤프에서 `AutoPlay`는 **0건**이다. 같은 덤프가 다른 INI 키 이름을 2~4건씩 담고 있으므로 이 0은 덤프의 한계가 아니다.

원본 INI 세 사본에도 없고 보호되지 않은 6th 실행 파일에도 없다. **INI를 통한 autoplay 제어 경로는 존재하지 않는다.** 사용자가 직접 테스트해 "동작하지 않는다"고 알려 준 결과와 일치하며, 이제 그 이유까지 확인됐다.

### 검증

* Windows x86 Debug·Release 전체 빌드: 오류 0건.
* 단위 테스트: `checks: 1752, failures: 0`.
* CTest 6개 중 5개 통과. 남은 `re2dj_windows_vfs_runtime_probe`는 작업 291에서 clean tree 대조로 확인한 **기존 실패**다.
* 옵션 없는 실행 전후로 `*.image.bin` 파일 수가 2개로 동일했다. 스위치가 게이트 역할을 한다.
* 위 실험 결과가 검증 2·3에 해당한다.

### 작업 지시에서 바꾼 것

두 가지이며 지시서에 근거와 함께 기록했다.

* **제품 CLI 노출을 범위에 넣었다.** 보호된 타깃은 모두 CHD 기반이라 런처 probe를 직접 부르려면 CHD 추출 경로까지 손으로 재현해야 한다. 제외하면 이 작업이 존재하는 이유인 대상에서 기능을 실행할 수 없다.
* **6th 대조군을 PE 헤더 대조로 대체했다.** 6th는 게임 본체가 자식 프로세스이고 본체를 직접 띄우면 런타임 주입이 실패한다. 이는 이번 작업과 무관한 기존 bring-up 미완이다. 실제로 쓰는 타깃에서 충실성을 증명한다는 점에서 대체 쪽이 낫다.

### 무엇이 열렸는가

`docs/analysis/`의 여러 항목이 "보호되어 정적 분석 불가"에 걸려 있었다. 이제 그 대상에 검색 경로가 생겼다. 덤프는 virtual 레이아웃이라 **파일 오프셋이 곧 RVA**이므로, 진단 로그의 주소나 프로파일의 helper RVA를 그대로 대조할 수 있다.

보류 중인 legacy I/O helper RVA 문제도 여기에 해당한다. 1st·2nd의 값이 다른 빌드에서 베껴 온 미확인 값인데, 덤프가 있으면 빌드별로 실제 확인이 가능하다.

### 범위에서 뺀 것

* 실행 가능한 PE 복원. import 재구성이 필요하며 정적 분석에는 불필요하다.
* 덤프를 이용한 실제 분석. autoplay 변수 추적을 포함해 별도 작업이다.
* 두 지점 외의 시점.

## English

Design: [20260916-292-decrypted-image-dump.md](../design/20260916-292-decrypted-image-dump.md)
Work order: [20260916-292-decrypted-image-dump.md](../work-orders/20260916-292-decrypted-image-dump.md)
Analysis: [Runtime decryption in protected builds](../analysis/protected-build-runtime-decryption.md)
Procedure: [Dumping the decrypted main image](../guides/decrypted-image-dump.md)

### Implementation

| Layer | Change |
| --- | --- |
| Windows | New `process_image_dump.h/.cpp` — page-wise `ReadProcessMemory`, virtual-layout output, recorded gaps, attribution sidecar, FNV-1a 64 digest |
| Launcher probe | `--image-dump [path]`, `--image-dump-delay <ms>`, the `entry` and `resumed` points, and one diagnostic line each |
| Profile, backend, product CLI | `image_dump` and `image_dump_delay_ms` forwarding, plus `--image-dump` |
| CMake | The new source and `RE2DJ_VERSION` for the backend library |

### Changed from the design — who takes the dump

The design first had the injected runtime dump its own address space, on the grounds that it could sit at any HLE boundary. **Checking the implementation sites broke that premise.**

Which HLE boundaries a profile enables varies: 3rd has no `DirectDrawCreate` in its IAT and never gets that hook at all. **There is no in-process boundary certain to be called under every profile**, and branching per boundary would leave combinations where the dump silently does nothing as profiles are added.

The launcher holds `child.hProcess` at both points and is independent of the profile. The design was corrected and the rationale recorded there.

### Result — the entry point is **before** decryption — confirmed

The design's central open question is answered. Two dumps from one 3rd run at `--image-dump-delay 8000`:

| Measurement | `entry` | `resumed` |
| --- | --- | --- |
| `TotalCoin` | 0 | **4** |
| `UseGameOver` | 0 | **2** |
| `AdvSound` | 0 | **4** |
| `UseIOCard` | 0 | **1** |
| `TestSongName` | 0 | **2** |
| Non-zero bytes | 1,196,837 | **2,279,626** |

The two differ in **2,469,020 of 6,799,360 bytes (36.3%)**.

The strings the `resumed` dump carries appear **zero times in the disk file**, so that dump captured decrypted state. None of them appears in the `entry` dump, so **the packer stub has not run at the entry breakpoint.** The PE header agrees: an `entry_point_rva` of `0x00642240` against a `size_of_image` of `0x0067c000` puts the entry in a stub section near the end of the image.

**Practical conclusion: use only `resumed` for analysis.**

### Dump fidelity — confirmed

The file's first `0x400` bytes match both dumps over the same range, no ranges failed to read (`gaps: []`), and `bytes_read` equals the whole `size_of_image`. The PE headers are untouched by the protection and share file offset with RVA, so that agreement proves the dump path faithful directly.

### Incidental — `AutoPlay` is not an EZ2DJ INI key — confirmed

The question that prompted this task was answered by the dump itself: `AutoPlay` appears **zero times** in the `resumed` dump, while that same dump carries other INI key names two to four times each, so the zero is not a limit of the dump. The key is absent from all three original INI copies and from the unprotected 6th executable too. **There is no autoplay control path through the INI**, which matches the user's own test that setting it does nothing — and now explains why.

### Verification

* Windows x86 Debug and Release full builds: no errors.
* Unit tests: `checks: 1752, failures: 0`.
* CTest: 5 of 6 pass; the remaining `re2dj_windows_vfs_runtime_probe` is the **pre-existing failure** confirmed against a clean tree in task 291.
* The `*.image.bin` count was 2 before and after a run without the option, so the switch gates it.
* The results above are verifications 2 and 3.

### Changed from the work order

Both are recorded with their rationale in the order itself. **Product CLI exposure was brought into scope**, because every protected target is CHD-based and excluding it made the feature impossible to exercise on the targets this task exists for. **The 6th control was replaced by a PE header comparison**, because 6th runs its game body as a child and launching that body directly fails at runtime injection — a pre-existing bring-up gap unrelated to this work; the substitute is the better control anyway, since it tests a target actually in use.

### What this opens

Several items in `docs/analysis/` were stuck on "protected, so static analysis is impossible". Those targets now have a search path. The dump is in virtual layout, so **a file offset is the RVA**, and an address from a diagnostic log or a profile's helper RVA can be checked against it directly. The deferred legacy-I/O helper RVA question is one of these: the 1st and 2nd values are unverified copies from another build, and a dump makes per-build confirmation possible.

### Excluded from scope

* Reconstructing a runnable PE, which needs import rebuilding and is unnecessary for static analysis.
* Analyzing the dump, including the autoplay variable hunt, which is its own task.
* Points beyond the two.
