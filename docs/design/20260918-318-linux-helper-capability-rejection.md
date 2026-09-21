# Linux native-helper capability rejection / Linux native-helper 기능 거부

## 목적 / Purpose

작업 317은 같은 기능 집합을 가진 Linux host와 i386 helper가 시작 협상을 완료하는 경로를 검증했습니다. 이 설계는 helper가 필수 feature를 하나라도 제공하지 않을 때 host가 `LoadImage`와 원본 이미지 바이트를 보내지 않고 실패하는 계약을 자동 검증으로 고정합니다.

*Task 317 verified the startup handshake between a Linux host and an i386 helper with the same feature set. This design fixes the complementary contract in an automated check: when the helper omits any required feature, the host fails without sending `LoadImage` or original-image bytes.*

## 거부 계약 / Rejection contract

검증 전용 fixture helper는 유효한 `Hello`를 받고 `kFeatureImportMetadata`만 포함한 `HelloResult`를 보냅니다. 현재 host가 요구하는 bounded memory transfer와 guest-memory lifecycle bit가 빠져 있으므로 host는 명확한 오류를 반환합니다. 이어서 host는 입력·출력 pipe를 닫고 짧게 child 종료를 기다립니다. child가 그 시간 안에 종료하지 않으면 기존 강제 종료 경로를 사용합니다.

*The verification-only fixture helper accepts a valid `Hello` and returns a `HelloResult` containing only `kFeatureImportMetadata`. The current host requires bounded memory transfer and guest-memory lifecycle too, so it returns a descriptive error. It then closes both pipes and briefly waits for the child to exit. If the child does not exit within that bound, the existing forced-stop path applies.*

fixture는 host pipe에서 EOF를 받은 경우에만 임시 상태 파일에 `eof-before-load-image`를 기록합니다. host probe는 `PrepareImage()` 실패, 필수 기능 오류, 이 상태 값을 모두 확인합니다. 따라서 fixture가 `LoadImage` header를 받았다면 검증은 실패합니다. 상태 파일 경로는 test process가 환경 변수로 전달하며 제품 helper나 제품 protocol에는 포함하지 않습니다.

*The fixture writes `eof-before-load-image` to a temporary status file only after receiving EOF from the host pipe. The host probe checks `PrepareImage()` failure, the required-feature error, and this status. The verification therefore fails if the fixture receives a `LoadImage` header. The test process supplies the status-file path through an environment variable; neither the product helper nor the product protocol uses it.*

```mermaid
sequenceDiagram
    participant H as Linux x64/x86 host probe
    participant F as rejection fixture helper
    H->>F: Hello(required features)
    F-->>H: HelloResult(import metadata only)
    H->>H: reject missing required bits
    H-xF: close stdin/stdout; no LoadImage
    F->>F: observe EOF and record status
```

## 범위와 한계 / Scope and limits

이 작업은 실제 i386 helper의 supported feature 집합을 바꾸거나 Win32 API binding을 추가하지 않습니다. fixture는 protocol rejection의 관찰 장치이며 guest 코드를 실행하거나 원본 자산을 읽지 않습니다. 종료 대기 시간은 정상 종료를 수거하기 위한 제한된 정리 단계일 뿐, 비협조 helper에 대한 무기한 대기가 아닙니다.

*This task does not change the real i386 helper's supported feature set or add Win32 API bindings. The fixture observes protocol rejection; it does not run guest code or read original assets. The termination wait is a bounded cleanup step for a cooperative child, not an unbounded wait for an uncooperative helper.*

## 검증 / Verification

WSL Ubuntu에서 Linux x64 host와 Linux x86 host가 각각 자신의 rejection fixture를 실행합니다. 두 경우 모두 기존 i386 production helper에 대한 성공·fault·terminal-stop probe와 CTest를 유지하고, 추가로 missing-feature 거부와 `eof-before-load-image` 상태를 확인합니다. 마지막으로 `git diff --check`를 실행합니다.

*On WSL Ubuntu, both the Linux x64 and Linux x86 hosts run their own rejection fixture. Both retain the existing success, fault, and terminal-stop probes against the production i386 helper, and additionally verify missing-feature rejection and the `eof-before-load-image` status. Finally run `git diff --check`.*
