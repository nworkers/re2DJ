# Linux Native Helper Extended Probe

## 한국어

### 결과

Linux x64 host probe가 Linux i386 helper에서 확장 합성 PE32를 실행했습니다. fixture는 requested base `0x11000000`에서 relocation, TLS process-attach callback, named import와 ordinal import를 검증합니다.

관찰 결과:

```text
native-ipc-host-probe: load=0x11000000 entry=0x11001000 imports=2 arguments=41,42 result=51 child=0
```

처음 실패한 원인은 process-exit event ID가 고정값 1이었던 것입니다. import bridge가 이미 event ID 1과 2를 사용하므로, exit event를 공유 counter에서 발급하도록 수정해 fixture의 event ID 3 기대값과 일치시켰습니다. helper 오류 packet도 adapter가 보존하도록 수정했습니다.

### 검증

Linux i386 helper와 Linux x64 host probe를 warnings-as-errors 구성으로 빌드하고 함께 실행했습니다.

## English

### Result

The Linux x64 host probe executed the extended synthetic PE32 in the Linux i386 helper. The fixture verifies relocation at requested base `0x11000000`, a TLS process-attach callback, and named plus ordinal imports.

Observed result:

```text
native-ipc-host-probe: load=0x11000000 entry=0x11001000 imports=2 arguments=41,42 result=51 child=0
```

The initial failure came from a fixed process-exit event ID of 1. Since import bridges already use event IDs 1 and 2, the exit event now uses the shared counter and matches the fixture's expected ID 3. Helper error packets are also preserved by the adapter.

### Verification

The Linux i386 helper and Linux x64 host probe were built with warnings treated as errors and run together.
