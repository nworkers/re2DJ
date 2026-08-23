# LPTDI 가상 개방 모킹 작업 로그

관련 설계: [병렬포트 디바이스 검사 HLE 계획](../design/20260823-047-lptdi-device-hle-plan.md)  
관련 작업 지시: [LPTDI 가상 개방 모킹](../work-orders/20260824-048-lptdi-mock-open.md)

## 결과

`--device-mock-lptdi`로 `CreateFileA("\\.\LPTDI*")`를 import thunk에서 가로채 synthetic handle `0xFEED0001`을 반환하는 최소 HLE를 구현했다. mock-off/on을 각 2회 비교한 결과, off는 기존 private RW page #UD를 재현했고 on은 두 번 모두 원본 entry `0x0043a640`과 원본 `.text` API caller에 도달했다. 따라서 LPTDI 개방 실패가 기존 실패 경로 선택의 원인이라는 인과를 확인했다.

성공 경로는 continuation page를 복호화해 실행하지 않고 `.gtide`에서 원본 entry로 직접 넘어갔다. 그러므로 "개방 실패 때문에 continuation 복호화를 생략했다"는 더 좁은 메커니즘은 확정하지 않는다.

## 구현

1. runtime export `g_re2dj_device_mock`이 활성화되면 대소문자를 무시한 `\\.\lptdi` 접두사를 인식한다.
2. `Re2djVfsCreateFileA`는 host/VFS 경로 변환 전에 synthetic handle을 반환한다.
3. file wrapper는 synthetic handle에 EOF형 read, `ERROR_ACCESS_DENIED` write, `ERROR_INVALID_FUNCTION` seek/size, `FILE_TYPE_CHAR`, 성공 close를 제공한다.
4. launcher 옵션 `--device-mock-lptdi`는 runtime 주입과 `--hle-vfs`를 함께 활성화하고 export 정책 값을 remote process에 기록한다. launch diagnostic에도 정책 상태를 남긴다.
5. 기존 runtime probe에 mock-off 거부와 mock-on handle 계약 검증을 추가했다.
6. runtime 주입 뒤 primary thread는 suspended 상태인데 API-trace 경로가 이미 해제된 entry debug event에 `ContinueDebugEvent`를 다시 호출하던 결합 오류를 수정했다. 주입 시 `ResumeThread`, 미주입 시 `ContinueDebugEvent`를 선택한다.

## 비교 관찰

| 구성 | 실행 1 | 실행 2 | 공통 관찰 |
| --- | --- | --- | --- |
| mock off | #UD `0x00393004` | #UD `0x0023f004` | 실행별 private RW page, 기존 미복호화 데이터 형태 |
| mock on | 원본 entry 후 AV `0x19d521bd` | 원본 entry 후 AV `0x19d521bd` | `CreateFileA` kernel32 기록 없음, handle `0xFEED0001`, IOCTL 2개, 원본 `.text` 도달 |

mock-on에서 새로 관찰한 보호 스텁 요청은 다음과 같다.

* `DeviceIoControl(0xFEED0001, 0x9c406410, ..., in=4)` — caller `0x01ed4240`
* `DeviceIoControl(0xFEED0001, 0x9c406414, ..., in=0x18)` — caller `0x01ed4dba`

두 호출은 아직 HLE하지 않아 host에서 실패하지만 스텁은 원본 entry로 진행했다. 원본 진입 뒤 `GetVersion` caller `0x0043a66c`, `VirtualAlloc` caller `0x00447f09`·`0x0044801d`, `GetProcAddress("IsProcessorFeaturePresent")` caller `0x0043a8b9`가 관찰됐다. 그 직후 두 번 모두 할당되지 않은 `0x19d521bd` 실행으로 access violation이 발생했으며 stack 선두는 `0x0043b688`, `0x0043b4c1`, `0x0045c008`로 동일했다. 이는 작업 48의 개방 모킹과 별개인 다음 원본 초기화 장애다.

비교 증거 로그:

* mock off: `20260824-003254-300.jsonl`, `20260824-003412-585.jsonl`
* mock on: `20260824-003319-890.jsonl`, `20260824-003440-022.jsonl`

로그에는 사용자 제공 원본 자산 자체를 포함하지 않았고 저장소에도 추가하지 않았다.

## 검증

```text
cmake --build --preset windows-x86-debug --config Debug
ctest --test-dir build\windows-x86 -C Debug --output-on-failure
```

Windows x86 Debug 전체 빌드가 통과했다. CTest는 `re2dj_windows_vfs_runtime_probe`, `re2dj_unit_tests` 2/2가 통과했다. runtime DLL과 launcher staged binary가 함께 재빌드됐다.

## 해석 경계와 후속 작업

확인됨: synthetic open이 CreateFileA host 경계를 우회하고, 성공 분기에서 두 IOCTL을 거쳐 원본 entry로 진입한다. LPTDI 개방 실패는 mock-off private-page 실패 경로의 원인이다.

미확정: `[0x01ed7074]`의 정확한 의미, 실패 경로 private page의 원래 목적, early XOR loop 대상, 원본 초기화의 `0x19d521bd` 간접 실행 원인. 다음 작업은 원본 `.text`의 간접 호출 대상과 IAT/함수 포인터 상태를 좁힌다.

---

# LPTDI Mock Open Work Log

Related design: [Parallel-Port Device Check HLE Plan](../design/20260823-047-lptdi-device-hle-plan.md)  
Related work order: [LPTDI Mock Open](../work-orders/20260824-048-lptdi-mock-open.md)

## Result

Implemented minimal import-thunk HLE in which `--device-mock-lptdi` intercepts `CreateFileA("\\.\LPTDI*")` and returns synthetic handle `0xFEED0001`. Two matched runs per configuration showed mock-off reproducing private-RW-page #UD, while mock-on reached original entry `0x0043a640` and API callers in original `.text` both times. Failed LPTDI open is therefore confirmed as the cause selecting the old failure path.

Success transferred directly from `.gtide` to the original entry instead of decrypting and executing the continuation page. The narrower mechanism "failed open caused skipped continuation decryption" is not confirmed.

## Implementation

The runtime recognizes the case-insensitive `\\.\lptdi` prefix only when exported policy `g_re2dj_device_mock` is enabled. Its file wrappers provide EOF read, denied write, unsupported seek/size, character-device type, and successful close semantics for the reserved handle range. Launcher option `--device-mock-lptdi` implies runtime injection and VFS hookup, writes the policy export remotely, and records it in launch diagnostics. The existing runtime probe now covers disabled and enabled policy contracts.

The comparison exposed and required fixing an existing launcher composition bug: runtime injection had already continued the entry debug event and returned with the primary thread suspended, but API trace tried to continue the same event again. Runtime-injected observation now uses `ResumeThread`; non-injected observation continues the pending debug event.

## Comparison

Mock-off failed at run-varying private pages `0x00393004` and `0x0023f004`. Mock-on suppressed the kernel32 CreateFileA record, passed handle `0xFEED0001` to IOCTLs `0x9c406410` and `0x9c406414`, reached original entry and original `.text`, then failed consistently at execute access violation `0x19d521bd` after resolving `IsProcessorFeaturePresent`. This stable later failure is separate follow-up work.

Evidence logs are `20260824-003254-300.jsonl` and `20260824-003412-585.jsonl` for mock-off, and `20260824-003319-890.jsonl` and `20260824-003440-022.jsonl` for mock-on. The logs contain no original asset payload and were not added to the repository.

## Verification

The Windows x86 Debug build passed. CTest passed 2/2: `re2dj_windows_vfs_runtime_probe` and `re2dj_unit_tests`. The staged runtime DLL and launcher were rebuilt.

## Interpretation boundary and follow-up

Confirmed: synthetic open bypasses the host CreateFileA boundary, the success branch performs two IOCTLs, and the stub reaches original entry. Failed LPTDI open causes the mock-off private-page failure path.

Unresolved: exact semantics of `[0x01ed7074]`, intended purpose of the failure-path page, early XOR-loop targets, and the indirect execute target causing `0x19d521bd` during original initialization. The next task should narrow original `.text` indirect-call and IAT/function-pointer state.
