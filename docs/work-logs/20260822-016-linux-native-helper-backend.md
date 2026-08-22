# Linux Native Helper PE32 Backend

## 한국어

### 결과

Linux x86-64 `NativeHelperBackend`와 Linux i386 helper를 추가하고, `LoadImage` → PE32 mapping → `Start` → entry point → `ProcessExit` 경로를 결합 실행으로 검증했습니다. probe의 결과는 `result=51`입니다.

helper에는 요청 주소 PE32 mapping, `HIGHLOW` relocation, import metadata 수집, executable IAT thunk, import bridge, TLS process-attach callback 경로가 포함됩니다. Linux host는 POSIX pipe/fork/exec만 사용합니다.

### 검증

다음 명령으로 Linux x64 host probe와 Linux i386 helper를 빌드·실행했습니다.

```text
cmake --build --preset linux-x64-debug --target re2dj_linux_native_ipc_host_probe
build/linux-x64-debug/bin/re2dj_linux_native_ipc_host_probe build/linux-x86-native-probe/bin/re2dj_linux_native_ipc_helper_probe
```

관찰 결과:

```text
linux-native-helper-probe: result=51
```

### 제한

현재 host probe fixture는 import 없는 최소 PE32입니다. IAT thunk와 TLS callback 코드는 helper에 구현·컴파일되었지만, named/ordinal import와 non-preferred base, TLS를 함께 검증하는 fixture는 다음 작업에서 추가해야 합니다.

## English

### Result

Added the Linux x86-64 `NativeHelperBackend` and Linux i386 helper, then verified the `LoadImage` → PE32 mapping → `Start` → entry point → `ProcessExit` path as an integration run. The probe returned `result=51`.

The helper includes requested-address PE32 mapping, `HIGHLOW` relocation, import metadata collection, executable IAT thunks, the import bridge, and TLS process-attach callback support. The Linux host uses only POSIX pipe/fork/exec.

### Verification

The Linux x64 host probe and Linux i386 helper were built and run with the commands above. The observed result was `linux-native-helper-probe: result=51`.

### Limitation

The current host fixture is a minimal PE32 without imports. IAT thunk and TLS callback code are implemented and compiled in the helper, but a fixture that jointly verifies named/ordinal imports, a non-preferred base, and TLS remains follow-up work.
