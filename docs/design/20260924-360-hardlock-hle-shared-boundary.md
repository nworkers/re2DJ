# 작업 360 설계 — Hardlock HLE 연결부 공용화와 WTS 이름 정정 / Task 360 design — Sharing the Hardlock HLE wiring and correcting the WTS names

선행: [작업 141 Hardlock HLE 통합](20260902-141-hardlock-hle-consolidation.md), [작업 359 작업 로그](../work-logs/20260924-359-user32-message-box.md), [4th Hardlock runtime 분석](../analysis/ez2dj4th-hardlock-runtime.md)

## 배경 / Background

Linux in-process 실행은 실제 4th CHD에서 `\\.\NTICE`·`\\.\FEnteDev` 열기가 실패한 뒤 Hardlock Error 1009로 끝난다. Windows에서는 같은 경계를 Hardlock HLE로 넘긴다. 이 HLE의 핵심(`HardlockDevice`, 암호 엔진, IOCTL 형태 검증, 응답 map, `cfg/hardlock.ini` 읽기)은 이미 `src/hle/hardlock/`과 `src/config/`의 플랫폼 중립 코드다. 그런데 이것을 Win32에 붙이는 연결부 세 군데가 Windows 전용 파일 안에 있다. Linux가 같은 HLE를 쓰려면 먼저 이 연결부를 공용으로 옮겨야 한다.

*On the real 4th CHD, Linux in-process execution ends in Hardlock Error 1009 after the `\\.\NTICE` and `\\.\FEnteDev` opens fail, while Windows passes the same boundary with the Hardlock HLE. Its core — `HardlockDevice`, the crypto engine, IOCTL shape validation, response maps, and reading `cfg/hardlock.ini` — is already platform-neutral code in `src/hle/hardlock/` and `src/config/`, but the three pieces that attach it to Win32 live in Windows-only files. They must become shared before Linux can use the same HLE.*

| 연결부 / Wiring | 현재 위치 / Now | 공용화 후 / After |
| --- | --- | --- |
| A. 장치 설정 조립(프로파일 정책 + `cfg/hardlock.ini` + 명시 옵션) / device material assembly | `windows_x86_launcher_probe/main.cpp` | `hle/hardlock/device_material` |
| B. `DeviceIoControl` Win32 완료 규칙과 요청 통계 / Win32 completion and request counts | `injected_runtime.cpp`의 `CompleteHardlockRequest` | `hle/hardlock/device_call` |
| C. 게스트 장치 경로 prefix 판정 / guest device-path prefix match | `injected_runtime.cpp`의 `HasDeviceMockPrefix` | `hle/guest_device_path` |

이 작업은 **Windows 동작을 바꾸지 않는** 리팩터다. Linux 연결(게스트 handle, `kernel32`/`wtsapi32` facade)은 작업 361 이후에 한다.

*This is a refactor that **does not change Windows behavior**; the Linux connection (guest handles and the `kernel32`/`wtsapi32` facades) follows in Task 361 onward.*

## A. 장치 설정 조립 / Device material assembly

```cpp
struct HardlockMaterialSources
{
    std::string profile_id;
    bool use_profile_cfg = false;          // TargetLptdiPolicy::hardlock_cfg_material_default
    std::filesystem::path config_path;     // cfg/hardlock.ini
    std::filesystem::path default_map_path;// cfg/hardlock-<profile>.map
    // Explicit launcher options, which outrank cfg. Empty means not given.
    std::string handshake_response_hex;
    std::string descriptor_tail_hex;
    std::string transform_map_path;
    bool device_requested = false;
};

struct HardlockDeviceMaterial
{
    bool device_enabled = false;
    std::optional<HardlockHandshakeResponse> handshake_response;
    std::optional<std::uint16_t> descriptor_tail_word;
    std::optional<HardlockSeeds> seeds;
    HardlockTransformResponseMap transform_map;
    // Which values a profile default took from cfg, for logs; never the values.
    bool cfg_handshake = false, cfg_tail = false, cfg_map = false;
};

bool ResolveHardlockDeviceMaterial(const HardlockMaterialSources&, HardlockDeviceMaterial*, std::string* error);
HardlockDeviceOptions MakeHardlockDeviceOptions(const HardlockDeviceMaterial&);
```

규칙은 launcher의 현재 동작을 그대로 옮긴다.

*The rules move the launcher's current behavior unchanged:*

- 프로파일이 cfg를 허용하면 `cfg/hardlock.ini`의 프로파일 절을 읽는다. 파일이나 절이 없는 것은 오류가 아니다. 파일 형식이 잘못된 것만 오류다.
  *When the profile allows cfg, read its section of `cfg/hardlock.ini`; a missing file or section is not an error, only a malformed file is.*
- 명시 map이 없고 기본 map 파일이 있으면 그것을 쓰고 장치를 켠다.
  *Without an explicit map, an existing default map file is used and enables the device.*
- 네 seed 값이 모두 있고 해석되면 seed를 쓰고 장치를 켠다. 해석에 실패하면 seed를 쓰지 않는다(지금처럼 오류 아님).
  *All four seed values, when present and parseable, are used and enable the device; a parse failure leaves seeds unset, not an error, as today.*
- cfg 절이 있고 map이나 seed가 있을 때만 handshake·tail replay 값을 cfg에서 채운다. 명시 옵션이 있으면 그것이 우선이다.
  *The handshake and tail replay values are filled from cfg only when the section exists and a map or seeds are present, and explicit options take precedence.*
- hex 값과 map 파일은 기존 공용 파서(`ParseHardlockHandshakeResponse`, `ParseHardlockApiTailWordHex`, `ParseHardlockTransformResponseMap`)로 해석한다.
  *Hex values and the map file go through the existing shared parsers.*

주입 runtime의 고정 크기 배열 용량 검사와 원격 프로세스 전달(pack/unpack)은 Windows 전송 방식에 속하므로 launcher와 runtime에 남긴다. Linux는 같은 프로세스이므로 `MakeHardlockDeviceOptions`의 결과를 바로 쓴다.

*The capacity check against the injected runtime's fixed arrays and cross-process transport (pack/unpack) belong to the Windows transport and stay in the launcher and runtime; Linux, in the same process, uses `MakeHardlockDeviceOptions` directly.*

## B. `DeviceIoControl` 완료 규칙 / `DeviceIoControl` completion

```cpp
struct HardlockDeviceCall
{
    bool handled = false;          // false: not a Hardlock request; the caller keeps its behavior
    bool succeeded = false;        // the Win32 BOOL result
    std::uint32_t bytes_returned = 0;
    std::uint32_t win32_error = 0; // value for SetLastError
    HardlockDeviceResult result;
};
HardlockDeviceCall CompleteHardlockDeviceIoControl(HardlockDevice&, std::uint32_t code,
                                                   std::span<const std::uint8_t> input,
                                                   std::span<std::uint8_t> output);

struct HardlockDeviceActivity { /* total, initialize, handshake, descriptor, transform, other, rejected, last_* */ };
void RecordHardlockDeviceCall(const HardlockDeviceResult&, std::uint64_t tick_ms, HardlockDeviceActivity*);
std::string FormatHardlockDeviceTrace(const HardlockDeviceResult&, std::uint64_t tick_ms);
```

`kCompleted`는 성공과 `ERROR_SUCCESS`, 쓴 byte 수가 된다. `kRejectedShape`는 실패, 0 byte, `ERROR_INVALID_DATA`(13)가 된다. `kNotHandled`는 처리하지 않은 것으로 돌려준다. 이것은 현재 `CompleteHardlockRequest`의 규칙이다. trace 한 줄의 형식도 공용으로 옮겨, 두 host의 로그를 같은 도구로 비교할 수 있게 한다.

*`kCompleted` becomes success, `ERROR_SUCCESS`, and the bytes written; `kRejectedShape` becomes failure, zero bytes, and `ERROR_INVALID_DATA` (13); `kNotHandled` is returned unhandled — today's `CompleteHardlockRequest` rules. The trace line format moves too, so both hosts' logs can be compared with the same tools.*

## C. 게스트 장치 경로 / Guest device paths

`MatchesGuestDevicePrefix(name, prefix)`: ASCII 대소문자를 무시하는 prefix 비교다. LPTDI port 번호처럼 끝이 바뀌는 이름 때문에 prefix로 비교한다. prefix가 비어 있을 때 `\\.\lptdi`를 쓰는 기본값은 Windows runtime의 정책이므로 호출자에 남긴다.

*`MatchesGuestDevicePrefix(name, prefix)` is an ASCII case-insensitive prefix test, used because names such as the LPTDI port vary in their tail. The `\\.\lptdi` fallback for an empty prefix is Windows-runtime policy and stays with the caller.*

## D. WTS 이름 정정 / WTS name correction

4th의 보호 코드는 `WTSQuerySessionInformationA(WTS_CURRENT_SESSION, class 4)`를 부른다. 결과를 0으로 바꾸면 `0x468` 뒤로 진행한다(분석 문서의 확인된 사실). 코드와 문서는 class 4를 `WTSConnectState`로, 0을 "active 상태"로 설명해 왔다. 그러나 Windows SDK 10.0.26100 `WtsApi32.h`의 `WTS_INFO_CLASS`에서 4는 `WTSSessionId`이고, `WTSConnectState`는 8이다([WTS_INFO_CLASS](https://learn.microsoft.com/windows/win32/api/wtsapi32/ne-wtsapi32-wts_info_class)). 따라서 이 HLE가 실제로 하는 일은 **현재 세션 번호를 0으로 보고하는 것**이다. 동작은 바꾸지 않고 이름과 설명을 고친다.

*4th's protection calls `WTSQuerySessionInformationA(WTS_CURRENT_SESSION, class 4)`, and rewriting the result to 0 lets it continue past `0x468` (a confirmed fact in the analysis). Code and documents have described class 4 as `WTSConnectState` and 0 as "active". In `WTS_INFO_CLASS` of Windows SDK 10.0.26100 `WtsApi32.h`, however, 4 is `WTSSessionId` and `WTSConnectState` is 8 ([WTS_INFO_CLASS](https://learn.microsoft.com/windows/win32/api/wtsapi32/ne-wtsapi32-wts_info_class)). The HLE therefore **reports the current session ID as 0**; the behavior stays and the names and descriptions are corrected.*

- 코드: 상수 `kWtsConnectState` → `kWtsSessionId`, 프로파일 필드 `hle_wts_active_console` → `hle_wts_console_session`, 관련 주석과 오류 문구.
  *Code: the constant `kWtsConnectState` → `kWtsSessionId`, the profile field `hle_wts_active_console` → `hle_wts_console_session`, and the related comments and error text.*
- 문서: 분석 문서에 정정 절을 추가하고, 현재 상태 문서(`ARCHITECTURE.md`, TODO)를 고친다. 작업 로그는 시간순 증거이므로 고치지 않는다.
  *Documents: add a correction section to the analysis and fix current-state documents (`ARCHITECTURE.md`, TODO); work logs are chronological evidence and stay as written.*
- **추정**: Windows XP 시절 물리 콘솔은 세션 0이었고, Vista 이후 사용자 세션은 1 이상이다. 보호 코드는 "세션 0 = 콘솔"을 검사하는 것으로 보인다.
  ***Inferred**: in the Windows XP era the physical console was session 0, while since Vista user sessions are 1 or higher, so the protection appears to test "session 0 = console".*

## 검증 / Validation

- 단위 테스트: A는 임시 디렉터리의 cfg·map 파일로 검사한다(파일 없음, 절 없음, seed만, map만, 명시 옵션 우선, 잘못된 형식). B는 결과 종류별 Win32 반환값과 통계를 검사한다. C는 대소문자와 prefix를 검사한다.
  *Unit tests: A with cfg and map files in a temporary directory (no file, no section, seeds only, map only, explicit precedence, malformed); B with the Win32 result and counts per outcome; C with case and prefix.*
- Windows x86 build와 CTest(기존 `re2dj_windows_vfs_runtime_probe` 실패 제외), `re2dj_windows_product_loader_probe`.
  *Windows x86 build and CTest (apart from the existing `re2dj_windows_vfs_runtime_probe` failure) and `re2dj_windows_product_loader_probe`.*
- Windows 실제 4th: 사용자 `cfg/hardlock.ini`로 변경 전 build와 후 build를 각각 실행한다. 진단 로그의 `hardlock_cfg_material` 기록과 Hardlock 요청 종류·횟수·결과가 같아야 한다. 로그는 값 없이 종류와 Boolean만 남긴다.
  *Real 4th on Windows: run the before and after builds with the user's `cfg/hardlock.ini`; the diagnostic log's `hardlock_cfg_material` record and the Hardlock request kinds, counts, and outcomes must match (logs carry only kinds and Booleans, never values).*
- Linux x64·x86 build와 CTest(공용 코어 변경 회귀).
  *Linux x64/x86 build and CTest (shared-core regression).*
