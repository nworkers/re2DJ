# Hardlock HLE 정리 작업 지시

관련 설계: [Hardlock HLE 정리](../design/20260902-141-hardlock-hle-consolidation.md)

*Related design: [Hardlock HLE consolidation](../design/20260902-141-hardlock-hle-consolidation.md).*

## 범위

1. Hardlock 흉내 코드를 `include/re2dj/hle/hardlock/`와 `src/hle/hardlock/`로 옮기고 namespace를 `re2dj::hle::hardlock`으로 바꿉니다. 파일 이름에서 `hardlock_` 접두사를 뗍니다.
2. `HardlockStubDevice`를 `HardlockDevice`로 바꾸고, 옵션·결과 타입과 launcher 옵션 `--hardlock-device`, trace 접두사 `hardlock-device`를 함께 정리합니다.
3. transform payload XOR probe를 제거합니다. 스텁 옵션, runtime export, launcher 옵션, 단위 테스트를 함께 지웁니다.
4. `--hardlock-transform-inputs`와 challenge 기록 경로를 제거합니다.
5. `--hardlock-descriptor-ids`와 descriptor trace를 제거합니다.
6. `HardlockProtocolTracker`, `HardlockRequestObservation`, protocol trace, `0x450` packet trace를 제거합니다. IOCTL 상수와 `ClassifyHardlockRequest`는 남깁니다.
7. seed·module address 비밀 경로를 제거합니다. runtime export 다섯 개, launcher와 제품 CLI의 `--hardlock-config`, profile flag `hardlock_secret_config_required`, ini의 `modad`/`seed1..3` 키와 strict 로더를 함께 지웁니다.
8. `windows_hardlock_descriptor_probe` 도구와 CMake target·test 등록을 제거합니다.
9. `ARCHITECTURE.md` 계층 표에 HLE Hardlock 행을 넣고 관련 서술을 갱신합니다.
10. 설정 가이드에서 사라진 키와 옵션을 정리합니다.
11. 제품 실행으로 두 제품이 통과 지문을 재현하는지 확인하고 작업 로그를 남깁니다.

*Move the emulation into `include/re2dj/hle/hardlock/` and `src/hle/hardlock/` under the `re2dj::hle::hardlock` namespace, dropping the `hardlock_` file prefix; rename `HardlockStubDevice` to `HardlockDevice` along with its option and result types, the `--hardlock-device` option, and the `hardlock-device` trace prefix; remove the transform payload XOR probe with its option, export, and tests; remove `--hardlock-transform-inputs` and the challenge recorder; remove `--hardlock-descriptor-ids` and the descriptor trace; remove `HardlockProtocolTracker`, `HardlockRequestObservation`, and the protocol and `0x450` packet traces while keeping the control-code constants and classifier; remove the seed and module-address secret path including its five runtime exports, `--hardlock-config` in both the launcher and the product CLI, the `hardlock_secret_config_required` profile flag, and the ini's `modad` and `seed1..3` keys with the strict loader; remove the `windows_hardlock_descriptor_probe` tool and its CMake target and test registration; add an HLE Hardlock row to the `ARCHITECTURE.md` layer table and update the surrounding prose; clean the vanished keys and options out of the configuration guide; and confirm by product run that both products reproduce the passing fingerprint, with a work log.*

## 비범위

- LPTDI 장치 코드 이동. 1st SE의 다른 장치이며 별도 판단입니다.
- Function `0x0e` 변환 구현.
- 보호 통과 이후의 HLE 공백 수정.

*Out of scope: moving the LPTDI device code, which belongs to 1st SE and is a separate judgement; implementing the Function `0x0e` transform; and fixing the HLE gaps after the protection.*

## 완료 조건

- 두 제품의 제품 실행이 Task 139의 통과 지문을 재현합니다. 3rd 377줄·`mapped=32`, 4th 468줄·`mapped=36`, 양쪽 `EZ2DJ.ini` 열기와 종료 코드 `0x00000000`입니다.
- 재료가 없으면 이전과 같은 지점에서 멈춥니다.
- `hardlock`을 언급하는 소스 파일이 흉내 경로와 그 호출부만 남습니다.
- 어떤 코드도 읽지 않는 export와 설정 키가 남지 않습니다.
- unit test 전체와 Windows x86 build, CTest가 통과합니다.
- 문서가 사라진 옵션과 키를 더 이상 안내하지 않습니다.

*Completion requires both product runs reproducing Task 139's fingerprint — 377 lines with `mapped=32` for 3rd, 468 with `mapped=36` for 4th, both opening `EZ2DJ.ini` and exiting `0x00000000`; unchanged stopping behavior without material; only the emulation path and its callers still mentioning `hardlock`; no export or configuration key that no code reads; passing unit tests, Windows x86 build, and CTest; and documents that no longer describe the removed options and keys.*
