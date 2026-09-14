# 작업 287 작업 로그 — `roms/` 미분석 실행 파일 추가 분석 / Task 287 work log — Adding the unanalyzed `roms/` executables

작업 지시: [20260915-287-rom-executable-inventory.md](../work-orders/20260915-287-rom-executable-inventory.md)
선행 작업: [작업 286](20260914-286-exe-analysis-refresh.md)

## 한국어

### 측정

사용자 제공 자산을 저장소 도구로 측정했다. 원본 파일은 저장소에 넣지 않았고, 기록한 것은 구조·오프셋·개수·해시뿐이다.

| 측정 | 도구 | 결과 |
| --- | --- | --- |
| 각 실행 파일 PE header·섹션 | `re2dj_pe_analyzer` | 문서의 표에 반영 |
| 원본 `.idata` import | IMAGE_IMPORT_DESCRIPTOR 직접 해석 | 1st Tracks 7/141, 5th 10/161, 6th 게임 7/137, 6th bootstrap 2/68, 6th 동봉 1st 6/138 |
| packed table | `re2dj_pe_loader` | 1st Tracks 22, 5th 36 |
| CHD 실행 파일 추출 | `re2dj_chd_probe --dump` | 1st SE, 3rd, 4th, 6th 두 개, 6th 동봉 1st 다섯 개, `ez2d2m` |
| CHD 디렉터리 열거 | `re2dj_chd_probe --list` | `6th.chd`의 `EZ2DJ`, `EZ2DJ/Ez2Dj1st` |
| 크기·MD5·SHA-1·SHA-256 | Python `hashlib` | 22개 파일 |

`re2dj_pe_loader`와 직접 해석이 4th·5th에서 각각 36과 38로 갈렸다. 원인을 확인한 결과 packed table에 `GetProcAddress`와 `GetModuleHandleA`가 두 번씩 들어 있어, loader는 고유 이름을, 직접 해석은 thunk 슬롯을 세고 있었다. 1st Tracks는 같은 이유로 22와 30이다. 두 경로는 모순되지 않으며, 문서에 이 구분을 적었다.

### 확인된 사실

1. **5th Trax는 어느 분석 문서에도 없었다.** `roms/ez2dj5th/ez2dj/EZ2DJ.exe`는 1,388,544바이트, PE TimeDateStamp `0x3f53377b`(2003-09-01), 진입점이 `.protect`에 있는 보호된 빌드다. 원본 `.idata`는 10 DLL / 161 함수이고, **4th와 DLL 목록·함수 이름 집합이 완전히 같다.** 순서만 다르며 DLL별로 비교해도 한쪽에만 있는 이름이 없다.
2. **6th는 실행 파일이 셋이고 셋 다 보호되지 않는다.** bootstrap `EZ2DJ.EXE`(126,976바이트), 게임 본체 `EZ2DJ6th.EXE`(585,728바이트), 그리고 `6th.chd`의 `EZ2DJ/Ez2Dj1st/Ez2DJ.exe`(360,448바이트)다. 셋 다 섹션이 `.text .rdata .data` 셋뿐이고 진입점이 `.text`에 있다. 따라서 게임 표면을 unpack 없이 정적으로 읽을 수 있는 첫 입력이다.
3. **6th bootstrap은 자식 경로를 둘 가진다.** 기존 문서는 `.\EZ2DJ6TH.EXE`만 기록했으나, `.\EZ2DJ1ST\EZ2DJ.EXE`(raw `0x1b094`)와 `%s\EZ2DJ1ST`(raw `0x1b088`)도 평문으로 있다. `6th.chd`에 실제로 `EZ2DJ/Ez2Dj1st/` 디렉터리가 열여섯 항목으로 존재한다.
4. **6th 동봉 1st Tracks는 보호되지 않은 2004년 재빌드다.** PE TimeDateStamp `0x411bbf5c`(2004-08-12), import 6 DLL / 138 함수다. 1999년 보호 빌드(141)와 비교하면 `ADVAPI32` 전체와 display mode API 네 개가 빠지고 `QueryPerformanceFrequency` 등 넷이 들어온다.
5. **6th 세 실행 파일 모두 Hardlock 장치 문자열을 평문으로 담는다.** `HARDLOCK.VXD`와 `FEnteDev`가 bootstrap(raw `0x1b5c0`), 게임 본체(raw `0x8b748`), 동봉 1st(raw `0x53f18`)에 있다. Hardlock 경계는 자식 프로세스만의 것이 아니다.
6. **1st Tracks의 import는 1st SE의 진부분집합이다.** 141개 전부가 1st SE 144개 안에 있고, 1st SE가 더 가진 것은 `GDI32!BitBlt`, `GDI32!SetBkColor`, `KERNEL32!GetWindowsDirectoryA` 셋뿐이다.
7. **1st SE는 보호 계열이 다른 두 빌드로 존재한다.** 디렉터리 덤프는 `.gtide`/`.gdata`/`.gidata` 8섹션(561,152바이트), CHD는 `.protect` 6섹션(634,880바이트)이며 PE TimeDateStamp는 둘 다 `0x3862df27`이다. 두 빌드의 본체 섹션은 배치가 같고 내용이 다르며, **`.idata`만 바이트 단위로 같다.**
8. **`roms/ez2dj1st/ez2dj`와 `ez2dj1`은 같은 실행 파일의 두 배치다.** 세 실행 파일이 바이트 단위로 같다. `ez2dj1`에만 있는 `AllowIo.exe`·`PortTalk.sys`는 2002-01-12 빌드로 1st Tracks보다 2년 늦고, 6th 동봉 배치의 같은 이름 파일과 SHA-256이 같다.
9. **`RELOCS_STRIPPED` 비트는 relocation 디렉터리 유무와 거의 일치하며 예외가 정확히 둘이다.** 1st SE `.gtide` 빌드는 비트가 없으면서 디렉터리가 비었고, `ez2d2m`는 비트가 있으면서 디렉터리가 남아 있다. 둘 다 packer가 헤더를 다시 쓴 빌드다.
10. **6th `EZ2DJ.INI`는 평문이 아니다.** 같은 디렉터리의 `bookkeeping.ini`는 평문이다. 5th와 그 이전 제품의 `EZ2DJ.INI`는 모두 평문이다.

### 정정 사항

* 공통 특성의 "확인한 실행 파일 전부 image base `0x00400000`, subsystem 2, alignment `0x1000`"은 `AllowIo.exe`(image base `0x01000000`, 콘솔 subsystem)와 `PortTalk.sys`(native 드라이버, image base `0x00010000`, alignment `0x20`)가 반례다. 문장을 게임·도구 실행 파일로 한정하고 두 예외를 명시했다.
* `docs/analysis/ez2dj-hdd-layout.md`에 `## 4.` 제목이 둘 있었다. 뒤의 둘을 5·6으로 다시 매겼다.
* `docs/analysis/ez2dj5th-6th-chd-filesystem.md`의 "5th 내부 실행 파일 경로 미확정"은 디렉터리 배치 확인으로 부분 해소되었다. 이미지 출처는 여전히 미확정이므로 그 구분을 남겼다.

### 문서 변경

* `docs/analysis/ez2dj-exe-structures.md` — 공통 특성에 파일 해시 표(22개 파일의 크기·MD5·SHA-1), `RELOCS_STRIPPED` 대조표, 절 색인을 넣었다. 타임스탬프 표에 1st Tracks·1st SE CHD·5th·6th 셋·보조 도구를 더했다. 1.0절(1st SE 두 빌드), 6절(1st Tracks), 7절(5th), 8절(6th, Mermaid 포함)을 새로 넣고 보조 도구를 9절, 절차를 10절로 다시 매겼다. 9.3·9.4절에 1st Tracks 보조 도구와 `AllowIo.exe`/`PortTalk.sys`를 넣었다.
* `docs/analysis/ez2dj-import-surface.md` — 측정 대상 문단에 1st Tracks·5th·6th를 넣었다. 전체 규모 표에 다섯 행을 더하고 슬롯/고유 이름 구분을 적었다. 1st Tracks·5th·6th 상세 절 셋을 새로 넣었다.
* `docs/analysis/ez2dj-hdd-layout.md` — PE 특성 표를 17행으로 확장하고, 3.3(5th 덤프)·3.4(6th 세 실행 파일) 절을 넣었다. 중복 절 번호를 고쳤다.
* `docs/analysis/ez2dj5th-6th-chd-filesystem.md` — 한국어·영어 양쪽에 2026-09-15 추가 확인 절을 넣고 5th 미확정 항목을 정정했다.
* `docs/analysis/ez2dj1stse-chd-filesystem.md` — 두 보호 빌드의 섹션 대조 결과와 해시 표를 넣었다.
* `docs/analysis/README.md` — 세 문서의 색인 상태를 갱신했다.
* `ARCHITECTURE.md` — import 표면 문단에 5th·6th 차이를 넣었다.
* `docs/EXE_DESIGN.ko.md`, `docs/EXE_DESIGN.en.md` — 결론 절을 더했다.

### 범위에서 뺀 것

* 코드 변경. 이 작업은 문서만 바꿨다. 새로 확인한 5th·6th 구조를 `src/target/target_profile.cpp`의 프로파일에 반영하는 것은 후속 작업 후보다. 특히 6th는 실행 파일이 셋이고 bootstrap이 자식을 둘 중에서 고르므로, 현재 프로파일의 단일 자식 전제를 다시 봐야 한다.
* 새로 확인한 실행 파일의 런타임 실행. 전부 정적 측정이다.
* 서드파티 드라이버와 설치 관리자 바이너리. `roms/ez2d2m/extracted/Drivers`와 `Install` 아래 약 200개는 nVidia·VIA·Creative 배포물이고 게임 코드가 아니므로 대상에서 뺐다.
* `roms/ez2dj6th/extraction-recovery/Panel-flat-files`의 `.abm` 자산. 실행 파일이 아니다.
* `roms` 최상위의 `.rar` 두 개와 `unezw.exe`. 각각 배포 압축 파일과 사용자 제공 변환 도구다.

### 검증

* 코드 변경이 없으므로 빌드 검증은 수행하지 않았다.
* 편집한 9개 문서에 대해 코드펜스 짝, 마크다운 표 구분선의 열 수 일치, 상대 링크 해석, 중복 절 번호를 스크립트로 검사해 문제 0건을 확인했다.
* import 개수는 `re2dj_pe_loader`와 직접 해석 두 경로로 교차 확인했고, 값이 다른 경우 원인(중복 thunk 슬롯)까지 확인한 뒤 기록했다.
* 4th와 5th, 1st Tracks와 1st SE, 4th와 6th의 import 집합 비교는 DLL별 집합 차집합으로 양방향 확인했다.
* CHD와 디렉터리 배치의 동일 여부는 세 해시(MD5·SHA-1·SHA-256)가 모두 일치하는지로 판정했다.

## English

### Measurement

Measured the user-supplied assets with repository tools. No original file entered the repository; only structures, offsets, counts and hashes were recorded.

| Measurement | Tool | Result |
| --- | --- | --- |
| PE headers and sections per executable | `re2dj_pe_analyzer` | reflected in the document tables |
| Original `.idata` imports | direct IMAGE_IMPORT_DESCRIPTOR parse | 1st Tracks 7/141, 5th 10/161, 6th game 7/137, 6th bootstrap 2/68, 6th bundled 1st 6/138 |
| Packed tables | `re2dj_pe_loader` | 1st Tracks 22, 5th 36 |
| CHD executable extraction | `re2dj_chd_probe --dump` | 1st SE, 3rd, 4th, both 6th executables, five files of the 6th's bundled 1st, `ez2d2m` |
| CHD directory listing | `re2dj_chd_probe --list` | `EZ2DJ` and `EZ2DJ/Ez2Dj1st` in `6th.chd` |
| Size, MD5, SHA-1, SHA-256 | Python `hashlib` | 22 files |

`re2dj_pe_loader` and the direct parse disagreed on 4th and 5th — 36 against 38. The cause turned out to be `GetProcAddress` and `GetModuleHandleA` appearing twice in the packed table: the loader counts distinct names, the direct parse counts thunk slots. 1st Tracks is 22 against 30 for the same reason. The two paths do not disagree, and the distinction is now written down.

### Confirmed facts

1. **5th Trax appeared in no analysis document.** `roms/ez2dj5th/ez2dj/EZ2DJ.exe` is 1,388,544 bytes with PE TimeDateStamp `0x3f53377b` (2003-09-01), a protected build whose entry point lies in `.protect`. Its original `.idata` holds 10 DLLs / 161 functions and its **DLL list and function-name set are exactly 4th's** — only the order differs, and a per-DLL comparison finds no name unique to either side.
2. **6th has three executables and none is protected.** The bootstrap `EZ2DJ.EXE` (126,976 bytes), the game body `EZ2DJ6th.EXE` (585,728 bytes), and `EZ2DJ/Ez2Dj1st/Ez2DJ.exe` inside `6th.chd` (360,448 bytes). All three have only `.text .rdata .data` and an entry point in `.text`, making them the first input whose game surface can be read statically without unpacking.
3. **The 6th bootstrap carries two child paths.** The existing document recorded only `.\EZ2DJ6TH.EXE`, but `.\EZ2DJ1ST\EZ2DJ.EXE` (raw `0x1b094`) and `%s\EZ2DJ1ST` (raw `0x1b088`) are present in plaintext too, and `6th.chd` really does hold a sixteen-entry `EZ2DJ/Ez2Dj1st/` directory.
4. **The 6th's bundled 1st Tracks is an unprotected 2004 rebuild.** PE TimeDateStamp `0x411bbf5c` (2004-08-12), imports 6 DLLs / 138 functions. Against the 1999 protected build (141) it drops all of `ADVAPI32` and four display-mode APIs and adds four including `QueryPerformanceFrequency`.
5. **All three 6th executables carry the Hardlock device strings in plaintext** — `HARDLOCK.VXD` and `FEnteDev` at raw `0x1b5c0` (bootstrap), `0x8b748` (game body) and `0x53f18` (bundled 1st). The Hardlock boundary is not the child process's alone.
6. **1st Tracks's imports are a strict subset of 1st SE's.** All 141 lie inside 1st SE's 144, and 1st SE adds only `GDI32!BitBlt`, `GDI32!SetBkColor` and `KERNEL32!GetWindowsDirectoryA`.
7. **1st SE exists as two builds from different protector families.** The directory dump is the eight-section `.gtide`/`.gdata`/`.gidata` arrangement (561,152 bytes) and the CHD copy the six-section `.protect` arrangement (634,880 bytes), both with PE TimeDateStamp `0x3862df27`. Their body sections share layout but differ in content, and **`.idata` alone is byte-identical.**
8. **`roms/ez2dj1st/ez2dj` and `ez2dj1` are two layouts of one executable.** Three executables are byte-identical between them. The `AllowIo.exe` and `PortTalk.sys` present only in `ez2dj1` are 2002-01-12 builds — two years after 1st Tracks — and share their SHA-256 with the same-named files in the 6th's bundled layout.
9. **The `RELOCS_STRIPPED` bit almost always agrees with whether a relocation directory exists, with exactly two exceptions.** The 1st SE `.gtide` build clears the bit with an empty directory, and `ez2d2m` sets the bit while leaving a directory behind. Both are builds whose header a packer rewrote.
10. **The 6th `EZ2DJ.INI` is not plaintext**, while `bookkeeping.ini` in the same directory is, and `EZ2DJ.INI` is plaintext in 5th and every earlier product.

### Corrections

* The common-traits claim that every executable inspected sits at image base `0x00400000` with subsystem 2 and `0x1000` alignment is contradicted by `AllowIo.exe` (image base `0x01000000`, console subsystem) and `PortTalk.sys` (native driver, image base `0x00010000`, `0x20` alignment). The sentence is now scoped to game and tool executables, with both exceptions named.
* `docs/analysis/ez2dj-hdd-layout.md` had two `## 4.` headings; the later two are renumbered 5 and 6.
* The "5th internal executable path unresolved" item in `docs/analysis/ez2dj5th-6th-chd-filesystem.md` is partly answered by the directory layout. The image's provenance is still unresolved, so the distinction is kept.

### Document changes

* `docs/analysis/ez2dj-exe-structures.md` — added a file-hash table to common traits (size, MD5, SHA-1 for 22 files), a `RELOCS_STRIPPED` comparison table, and a section index; extended the timestamp table with 1st Tracks, the 1st SE CHD build, 5th, the three 6th executables and the auxiliary tools; added section 1.0 (the two 1st SE builds), section 6 (1st Tracks), section 7 (5th) and section 8 (6th, with a Mermaid diagram); renumbered auxiliary tools to 9 and the procedure to 10; added 9.3 and 9.4 for the 1st Tracks auxiliary tools and for `AllowIo.exe`/`PortTalk.sys`.
* `docs/analysis/ez2dj-import-surface.md` — widened the scope paragraph to 1st Tracks, 5th and 6th; added five rows to the overview table plus the slot-versus-distinct-name distinction; added three new per-product detail subsections.
* `docs/analysis/ez2dj-hdd-layout.md` — extended the PE characteristics table to 17 rows, added 3.3 (the 5th dump) and 3.4 (the 6th's three executables), and fixed the duplicated section number.
* `docs/analysis/ez2dj5th-6th-chd-filesystem.md` — added a 2026-09-15 additions section to both the Korean and English halves and corrected the 5th unresolved item.
* `docs/analysis/ez2dj1stse-chd-filesystem.md` — added the section-by-section comparison of the two protected builds with a hash table.
* `docs/analysis/README.md` — refreshed the index status for three documents.
* `ARCHITECTURE.md` — folded the 5th and 6th differences into the import-surface paragraph.
* `docs/EXE_DESIGN.ko.md`, `docs/EXE_DESIGN.en.md` — appended a conclusions section.

### Excluded from scope

* Code changes. This task edited documents only. Reflecting the newly confirmed 5th and 6th structures in the profiles in `src/target/target_profile.cpp` is a follow-up candidate — particularly for 6th, where three executables and a bootstrap that chooses between two children call the current single-child assumption into question.
* Running the newly identified executables. Everything here is static measurement.
* Third-party driver and installer binaries. The roughly 200 files under `roms/ez2d2m/extracted/Drivers` and `Install` are nVidia, VIA and Creative redistributables, not game code.
* The `.abm` assets under `roms/ez2dj6th/extraction-recovery/Panel-flat-files`, which are not executables.
* The two `.rar` archives and `unezw.exe` at the top of `roms`, being a distribution archive and a user-supplied conversion tool respectively.

### Verification

* No code changed, so no build verification was run.
* A script checked the nine edited documents for balanced code fences, matching column counts between markdown table headers and separators, resolvable relative links, and duplicate section numbers, reporting zero problems.
* Import counts were cross-checked through `re2dj_pe_loader` and the direct parse; where the two differed, the cause (duplicated thunk slots) was established before recording.
* The 4th-versus-5th, 1st Tracks-versus-1st SE and 4th-versus-6th import comparisons were done as per-DLL set differences in both directions.
* Whether a CHD copy and a directory layout are the same build was decided by requiring all three hashes — MD5, SHA-1 and SHA-256 — to match.
