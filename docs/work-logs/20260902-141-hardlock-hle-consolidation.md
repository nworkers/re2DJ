# Hardlock HLE 정리 작업 로그

관련 설계: [Hardlock HLE 정리](../design/20260902-141-hardlock-hle-consolidation.md), 관련 작업 지시: [작업 지시](../work-orders/20260902-141-hardlock-hle-consolidation.md)

*Related design: [Hardlock HLE consolidation](../design/20260902-141-hardlock-hle-consolidation.md); related work order: [work order](../work-orders/20260902-141-hardlock-hle-consolidation.md).*

## 결과

Hardlock 흉내 코드를 HLE 계층으로 옮기고, 계약을 알아내려고 만들었던 진단 코드를 제거했습니다. 제품 실행 동작은 그대로입니다.

*The emulation moved into the HLE layer and the contract-discovery diagnostics were removed, with product behavior unchanged.*

## 성격 정의

이번 작업에서 이 계층의 성격을 문서로 못 박았습니다. **원본이 dongle에게 묻는 네 요청에 규격대로 답하는 장치 경계이며, 암호 연산은 하지 않습니다.**

| 나눔 | 담당 |
| --- | --- |
| 프로토콜, packet framing, descriptor 판독, 형태 검증 | re2DJ |
| 응답 값 계산 | 저장소 밖 별도 프로그램 |
| 값 보관 | 사용자의 `cfg/` |

dongle 에뮬레이터가 아닙니다. 성립하는 이유는 challenge 집합이 실행 파일에서 결정되는 고정값(3rd 32개, 4th 36개)이라 오프라인 표 하나로 충분하기 때문이며, 한계도 여기서 나옵니다. 표에 없는 challenge는 추측하지 않고 통과시키고, 실행 파일이 바뀌면 표를 다시 만들어야 하며, 임의 challenge를 던지는 프로그램은 지원하지 못합니다. `ARCHITECTURE.md`에 이 정의를 넣었습니다.

*The layer's character is now pinned in the documents: a device boundary that answers the original's four dongle requests to specification and performs no cryptography. re2DJ owns protocol, framing, descriptor reading, and shape validation; a separate program computes the values and the user keeps them under `cfg/`. It is not a dongle emulator — it works only because the challenge set is fixed by the executable, 32 for 3rd and 36 for 4th, so one offline table suffices, and the limits follow: an uncovered challenge passes through unguessed, a different executable needs a new table, and arbitrary challenges cannot be served.*

## 위치

| 이전 | 이후 |
| --- | --- |
| `include/re2dj/device/hardlock_*.h` | `include/re2dj/hle/hardlock/{device,protocol,api_descriptor,handshake_response,transform_responses}.h` |
| `src/device/hardlock_*.cpp` | `src/hle/hardlock/*.cpp` |
| `re2dj::device` | `re2dj::hle::hardlock` |

디렉터리가 한정자를 제공하므로 파일 이름에서 `hardlock_` 접두사를 뗐습니다. `include/re2dj/platform/windows/`가 이미 같은 방식입니다. 계층 표에 HLE Hardlock 행을 넣었습니다. LPTDI는 1st SE의 다른 장치라 `src/device/`에 남겼고, 그 디렉터리의 지위는 후속 판단입니다.

*Location: the headers and sources moved under `hle/hardlock/` with the `re2dj::hle::hardlock` namespace, dropping the `hardlock_` file prefix because the directory supplies the qualifier — the same shape `include/re2dj/platform/windows/` already uses — and the layer table gained an HLE Hardlock row. LPTDI stays in `src/device/` as 1st SE's separate device, whose standing is left to a later judgement.*

## 제거

| 제거 | 답이 남은 곳 |
| --- | --- |
| transform payload XOR probe | [Task 131](20260901-131-hardlock-bypass-stub.md): 게스트가 출력을 소비함 |
| `--hardlock-transform-inputs`와 challenge 기록기 | [Task 138](20260902-138-ez2dj3rd-transform-challenge-observation.md): 규칙이 PE에서 유도되고 두 제품 확정 |
| `--hardlock-descriptor-ids`와 descriptor trace | [3rd 분석](../analysis/ez2dj3rd-hardlock-function-0e.md), [4th 분석](../analysis/ez2dj4th-hardlock-runtime.md): ID 값 기록됨 |
| `HardlockProtocolTracker`, protocol trace, `0x450` packet trace | 계약 확정. 요청 종류와 결과는 device trace가 이미 기록 |
| seed·module address 비밀 경로 전체 | 어떤 코드도 seed를 소비하지 않음 |
| `windows_hardlock_descriptor_probe` | descriptor 판독은 unit test가 고정 |

**seed 경로 제거가 핵심입니다.** export 다섯 개가 게스트 프로세스까지 전달됐지만 이를 읽어 무언가를 계산하는 코드는 없었습니다. 함께 사라진 것은 launcher와 제품 CLI의 `--hardlock-config`, profile flag `hardlock_secret_config_required`, ini의 `modad`/`seed1..3` 키와 strict 로더입니다. 이제 re2DJ 설정에는 **re2DJ가 실제로 쓰는 값만** 남습니다. 사용자 `cfg/hardlock.ini`에서 seed 네 줄을 지웠습니다. 값 자체는 reSoftlock 산출물에 그대로 있고 re2DJ는 매핑 파일만 쓰므로 손실이 없습니다.

이름도 바로잡았습니다. `HardlockStubDevice` → `HardlockDevice`, `--hardlock-stub` → `--hardlock-device`, trace 접두사 `hardlock-stub` → `hardlock-device`입니다. "stub"은 이 코드가 실제로 보호를 통과시키는 응답을 전달하므로 더는 맞지 않는 이름이었습니다.

*Removed: each discovery tool whose answer is already recorded. **The seed path removal is the core** — five exports reached the guest process while no code read them to compute anything, so `--hardlock-config` in both the launcher and the product CLI, the `hardlock_secret_config_required` profile flag, and the ini's `modad` and `seed1..3` keys with the strict loader went with them, leaving re2DJ's configuration holding only values re2DJ uses. The four seed lines were deleted from the user's `cfg/hardlock.ini`; the values remain in reSoftlock's artifacts and re2DJ uses only the map, so nothing is lost. Naming was corrected as well, from stub to device throughout, because this code does deliver the responses that pass the protection.*

## 검증

제품 실행이 통과 지문을 그대로 냅니다.

| | ez2dj3rd | ez2dj4th |
| --- | --- | --- |
| transform 주입 | `mapped=32:unmapped=0` | `mapped=36:unmapped=0` |
| `EZ2DJ.ini` 열기 | 2회 | 2회 |
| 종료 | `0x00000000` | `0x00000000` |
| vfs trace | 322줄 (이전 377) | 341줄 (이전 468) |

trace 줄 수 감소는 제거한 진단 줄 그대로입니다. 판별에 쓰는 표시(주입 완전성, `EZ2DJ.ini` 열기, 종료 코드)는 변하지 않았습니다.

- Windows x86 Debug build 통과, 경고 없음
- unit test `checks: 1184, failures: 0`
- `re2dj_windows_product_loader_probe` 통과
- `git diff --check` 통과

*Verification: both product runs reproduce the passing fingerprint with complete injection, two `EZ2DJ.ini` opens, and a `0x00000000` exit; the trace shrank by exactly the diagnostic lines removed while every marker used for judgement is unchanged. The Windows x86 Debug build passed without warnings, unit tests reported 1,184 checks with no failures, and the product loader probe passed.*

## 확인되지 않은 항목

`re2dj_windows_vfs_runtime_probe`는 `windowed client size policy failed`로 실패한 뒤 멈춥니다. 실패 지점은 창 client 영역이 1280×960인지 보는 **그래픽 검사**이며 Hardlock과 무관합니다. 이 파일은 이번 작업에서 수정하지 않았고 Hardlock 심볼을 하나도 import하지 않습니다. 표시 환경에 의존하는 검사로 보이지만 이번 작업에서 원인을 확인하지 않았으므로 **미확인 상태로 기록합니다.**

*Unverified: `re2dj_windows_vfs_runtime_probe` fails at `windowed client size policy failed` and then hangs. The failing assertion checks a 1280×960 window client area — a graphics check unrelated to Hardlock — in a file this task did not modify and that imports no Hardlock symbol. It looks display-environment dependent, but the cause was not investigated here, so it is recorded as unverified.*

## 다음 단계

1. `MapVfsPath`가 절대 host 경로를 그대로 통과시키도록 고칩니다. 두 제품이 `EZ2DJ.ini`에서 막혀 있습니다.
2. `re2dj_windows_vfs_runtime_probe`의 창 크기 검사 실패 원인을 확인합니다.
3. `src/device/`에 남은 LPTDI의 계층 위치를 정합니다.

*Next: fix `MapVfsPath` for absolute host paths, since both products stop at `EZ2DJ.ini`; investigate the vfs runtime probe's window-size failure; and decide where the LPTDI code left in `src/device/` belongs.*
