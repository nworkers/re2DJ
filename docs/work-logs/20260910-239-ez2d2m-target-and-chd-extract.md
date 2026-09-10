# 작업 로그: ez2d2m target 추가와 CHD 재귀 추출

## 한국어

### 관련 문서

* [설계](../design/20260910-239-ez2d2m-target-and-chd-extract.md)
* [작업 지시서](../work-orders/20260910-239-ez2d2m-target-and-chd-extract.md)
* [ez2d2m CHD 파일시스템과 실행 파일 관찰](../analysis/ez2d2m-chd-filesystem.md)
* [EZ2Dancer I/O 포트 맵](../analysis/ez2dancer-io-map.md)

### 한 일

**분석.** 사용자가 제공한 `roms/ez2d2m/ez2d2m.chd`를 기존 CHD/FAT32 reader로 읽어 볼륨 geometry, Windows 98 SE 배치, `ez2dancer` 게임 디렉터리를 확인했습니다. `ez2dancer/EZ2Dancer.exe`를 꺼내 PE 구조와 packed import directory를 읽었습니다. 결과는 위 분석 문서 두 개에 있습니다. 실행은 하지 않았으므로 모든 관찰은 정적입니다.

이 target의 성격을 정한 사실 셋입니다.

1. 보호 계층이 EZ2DJ와 같은 `.protect` Hardlock envelope입니다. `\\.\FEnteDev`, `HLW32Proc`, `API_1LNM.DLL`, `WTSQuerySessionInformationA`가 모두 있습니다.
2. 그래픽 진입점이 `DirectDrawCreate`가 아니라 `DirectDrawCreateEx`입니다. launcher가 이미 그 slot을 다루므로 추가 작업 없이 연결됩니다.
3. I/O 보드가 다릅니다. 공개 구현이 서술하는 EZ2Dancer 계약은 16비트 폭 `0x300`~`0x30c`이고, 현재 `LegacyIoPortBus`는 byte 폭 `0x100`~`0x106`입니다.

**프로파일.** `ez2d2m`을 `GetBuiltInTargetProfiles()`에 추가했습니다. `MakeChdCompatibilityProfile()`을 쓰지 않고 직접 작성했습니다. 그 helper가 심는 실행 파일 이름, sibling, raw I/O RVA 중 이 제품에 맞는 것이 없기 때문입니다. 각 HLE 항목은 이 실행 파일 자신의 packed import directory를 따릅니다. `legacy_io_ports`는 껐습니다. 켜면 fault handler가 `IN AX,DX`를 byte helper로 오인해 지원되지 않는 port에 답하고 `EIP`를 1만 증가시켜 명령 중간으로 복귀합니다.

**추출.** `re2dj_chd_probe`에 `--extract <내부 경로> <출력 디렉터리>`를 추가하고, 재귀 부분은 `src/tools/chd_probe/chd_extract.{h,cpp}`로 분리했습니다. `main.cpp`에는 인자 해석과 보고만 남겼습니다.

### 도중에 나온 것

**UTF-8 이름에서 무한 대기.** 첫 전체 추출이 `Program Files/Common Files/Microsoft Shared` 아래에서 멈췄습니다. 파일도 쓰이지 않고 진행 보고도 없는데 CPU만 올라갔습니다.

임시 추적을 넣어 위치를 좁혔습니다. 멈춘 지점은 한국어 이름 디렉터리 항목의 `output / entry.name`, 즉 `std::filesystem::path` 구성이었습니다.

`Fat32Entry::name`은 long-name decoder가 만든 **UTF-8**입니다. 그것을 `std::string`으로 `std::filesystem::path`에 넘기면 MSVC는 호스트의 narrow 인코딩, 즉 활성 ANSI 코드 페이지로 해석합니다. 해당 이름의 UTF-8 바이트는 그 코드 페이지에서 유효하지 않고, 변환이 실패로 끝나지 않고 **돌아오지 않았습니다.**

고친 내용입니다.

* 이름을 `std::u8string`을 거쳐 경로로 만드는 `ChdEntryHostName()`을 두었습니다. `std::u8string`은 모든 플랫폼에서 UTF-8이므로 native 인코딩 변환이 올바르게 일어납니다.
* 파일 열기를 `std::fopen(path.string().c_str(), ...)`에서 `std::ofstream(path, ...)`로 바꿨습니다. 앞의 것은 표현할 수 없는 narrow 문자열을 거치지만, 뒤의 것은 Windows에서 native wide 경로로 엽니다.
* 메시지용 경로 문자열도 `path.string()` 대신 UTF-8 렌더링을 씁니다.

이 규칙에 대한 회귀 시험 `tests/unit/chd_extract_name_test.cpp`를 추가했습니다. 문제를 일으킨 그 이름을 UTF-8 바이트로 적어 round-trip을 확인하므로, 시험 파일 자체는 ASCII로 남습니다.

이 한 건이 이 작업에서 가장 값이 큰 결과일 수 있습니다. 추출은 진단 도구지만, 같은 UTF-8 대 ANSI 경계는 VFS가 게스트에게 이름을 돌려줄 때도 존재합니다. 이 작업에서 그쪽은 확인하지 않았습니다.

### 검증

| 항목 | 결과 |
| --- | --- |
| Windows x86 Debug build (`re2dj_chd_probe`, `re2dj_unit_tests`) | 통과, 변경 파일 경고 0건 |
| `re2dj_unit_tests` | checks 1481, failures 0 (`ez2d2m` 프로파일과 이름 변환 포함) |
| CTest, `re2dj_windows_vfs_runtime_probe` 제외 | 3/3 통과 |
| `--extract "" roms/ez2d2m/extracted` | directories 523, files 15081, bytes 2,713,626,028, failures 0 |
| 추출본 무결성 | `ez2dancer/EZ2Dancer.exe`가 `--dump`로 직접 꺼낸 것과 MD5 동일 |
| 한국어 이름 보존 | `Program Files/Common Files/Microsoft Shared/웹 폴더`가 그대로 생성됨 |
| 추출본 스캔 | `re2dj --hdd roms/ez2d2m/extracted --list-targets`가 273개 실행 파일을 보고하고 `ez2dancer/EZ2Dancer.exe`를 후보로 나열. CHD 프로파일은 설계대로 디렉터리 스캔에서 주장되지 않으므로 detected 항목입니다 |

**보고할 것:** `re2dj_windows_vfs_runtime_probe`는 이 작업 전부터 멈춥니다. 해당 실행 파일은 2026-09-07 빌드로 이 작업에서 다시 링크되지 않았고, 단독 실행에서도 60초 안에 끝나지 않습니다. 이 작업의 변경과 무관하며 여기서 고치지 않았습니다. 나머지 세 시험은 1초 안에 통과합니다.

### 남은 것

* 16비트 폭 legacy I/O bus와 `0x300` 대역. 이것 없이는 `ez2d2m`이 cabinet 입력을 받지 못합니다.
* 이 실행 파일의 raw I/O helper RVA. 실행해 privileged fault 주소를 봐야 얻어집니다.
* Hardlock 응답과 descriptor.
* 암호화된 `EZ2DANCER.ini`, `song.ini`, `upgrade.ini` 형식.
* VFS가 게스트에게 돌려주는 이름에도 같은 UTF-8 경계 문제가 있는지 확인.
* `re2dj_windows_vfs_runtime_probe` 정지 원인.

## English

### Related documents

* [Design](../design/20260910-239-ez2d2m-target-and-chd-extract.md)
* [Work order](../work-orders/20260910-239-ez2d2m-target-and-chd-extract.md)
* [ez2d2m CHD filesystem and executable observations](../analysis/ez2d2m-chd-filesystem.md)
* [EZ2Dancer I/O port map](../analysis/ez2dancer-io-map.md)

### What was done

**Analysis.** The user-supplied `roms/ez2d2m/ez2d2m.chd` was read with the existing CHD/FAT32 reader to establish its volume geometry, its Windows 98 SE layout and its `ez2dancer` game directory, and `ez2dancer/EZ2Dancer.exe` was pulled out so its PE structure and packed import directory could be read. The results are in the two analysis documents above. Nothing was executed, so every observation is static.

Three facts settled what this target is:

1. The protection is the same `.protect` Hardlock envelope as EZ2DJ — `\\.\FEnteDev`, `HLW32Proc`, `API_1LNM.DLL` and `WTSQuerySessionInformationA` are all present.
2. The graphics entry point is `DirectDrawCreateEx` rather than `DirectDrawCreate`, which the launcher's IAT lookup already handles, so it connects with no further work.
3. The I/O board differs. The public implementation describes EZ2Dancer as 16-bit-wide access over `0x300` to `0x30c`, while `LegacyIoPortBus` is byte-wide over `0x100` to `0x106`.

**The profile.** `ez2d2m` was added to `GetBuiltInTargetProfiles()`, written out rather than built from `MakeChdCompatibilityProfile()` because none of that helper's executable name, siblings or raw-I/O RVAs apply to this product. Each HLE setting follows this executable's own packed import directory. `legacy_io_ports` is off: with it on, the fault handler would mistake `IN AX,DX` for the byte helper, answer a port the bus does not serve, and advance `EIP` by one into the middle of an instruction.

**Extraction.** `re2dj_chd_probe` gained `--extract <inner path> <output directory>`, with the recursion in its own `src/tools/chd_probe/chd_extract.{h,cpp}` and only argument handling and reporting left in `main.cpp`.

### What came up along the way

**A hang on UTF-8 names.** The first full extraction stopped under `Program Files/Common Files/Microsoft Shared`: no files were written and no progress was reported, but CPU kept climbing.

Temporary tracing narrowed it to `output / entry.name` — the `std::filesystem::path` construction — for a Korean-named directory entry.

`Fat32Entry::name` is **UTF-8**, because that is what the long-name decoder produces. Handed to `std::filesystem::path` as a `std::string`, MSVC reads it in the host's narrow encoding, which is the active ANSI code page. That name's UTF-8 bytes are not valid there, and the conversion did not fail — **it never returned.**

The fix:

* `ChdEntryHostName()` builds the path component through a `std::u8string`, which is UTF-8 on every platform, so the conversion to the native encoding is the correct one.
* File opening moved from `std::fopen(path.string().c_str(), ...)` to `std::ofstream(path, ...)`, which opens through the native wide path on Windows instead of a narrow rendering that cannot represent every name.
* Path text in messages uses a UTF-8 rendering rather than `path.string()`.

`tests/unit/chd_extract_name_test.cpp` pins the rule. It spells the offending name as its UTF-8 bytes and checks the round trip, so the test file itself stays ASCII.

This may be the most valuable result of the task. Extraction is a diagnostic tool, but the same UTF-8-versus-ANSI boundary exists wherever the VFS hands a name back to the guest. That side was not examined here.

### Verification

| Item | Result |
| --- | --- |
| Windows x86 Debug build (`re2dj_chd_probe`, `re2dj_unit_tests`) | Passed, zero warnings from the changed files |
| `re2dj_unit_tests` | 1481 checks, 0 failures, covering the `ez2d2m` profile and the name conversion |
| CTest excluding `re2dj_windows_vfs_runtime_probe` | 3/3 passed |
| `--extract "" roms/ez2d2m/extracted` | 523 directories, 15,081 files, 2,713,626,028 bytes, 0 failures |
| Extraction integrity | `ez2dancer/EZ2Dancer.exe` has the same MD5 as the copy pulled directly with `--dump` |
| Non-ASCII names | `Program Files/Common Files/Microsoft Shared/웹 폴더` is created intact |
| Scanning the extraction | `re2dj --hdd roms/ez2d2m/extracted --list-targets` reports 273 executables and lists `ez2dancer/EZ2Dancer.exe` as a candidate. It appears as a detected entry because, by design, CHD profiles are not claimed by a directory scan |

**To report:** `re2dj_windows_vfs_runtime_probe` hangs, and did so before this task. Its executable is a 2026-09-07 build that this task did not relink, and it does not finish within 60 seconds when run on its own. It is unrelated to these changes and was not fixed here. The other three tests pass in about a second.

### What remains

* A 16-bit-wide legacy I/O bus and the `0x300` band. Without it `ez2d2m` receives no cabinet input.
* This executable's raw-I/O helper RVAs, which require running it and reading the privileged-fault address.
* The Hardlock response and descriptor.
* The format of the encrypted `EZ2DANCER.ini`, `song.ini` and `upgrade.ini`.
* Whether the same UTF-8 boundary affects names the VFS returns to the guest.
* The cause of the `re2dj_windows_vfs_runtime_probe` hang.
