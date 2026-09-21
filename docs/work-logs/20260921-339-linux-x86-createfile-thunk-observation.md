# 작업 로그 339: Linux x86 CreateFileA 동적 thunk 관측 / Work log 339: Linux x86 dynamic CreateFileA thunk observation

## 결과 / Result

Linux i386 전용 `--linux-in-process-createfile-call` 진단을 추가했습니다. 확인된 pseudo kernel32 resolver는 GetVersion과 CreateFileA에 서로 다른 process-local executable thunk를 반환합니다. 새 `native_create_file_observation.cpp`가 resolver와 CreateFileA 호출 관측을 소유하여 기존 `original_runner.cpp`에는 기능을 누적하지 않았습니다.

*Added the Linux i386-only `--linux-in-process-createfile-call` diagnostic. The confirmed pseudo-kernel32 resolver returns distinct process-local executable thunks for GetVersion and CreateFileA. The new `native_create_file_observation.cpp` owns resolver and CreateFileA call observation instead of accumulating the feature in the existing `original_runner.cpp`.*

import bridge event에 bootstrap의 guarded guest-stack 범위를 전달했습니다. 진단은 mapped image 또는 guest stack 안에 있는 문자열만 최대 260자로 읽고, CreateFileA의 복귀 주소와 7개 stdcall 인자를 구조화해 보존합니다. 호출 뒤에는 실제 파일·장치 동작을 수행하지 않고 `INVALID_HANDLE_VALUE`와 28바이트 cleanup을 반환하여 원래 caller return breakpoint에서 멈춥니다.

*The import-bridge event now carries the bootstrap's guarded guest-stack range. The diagnostic reads at most 260 characters only from strings inside the mapped image or guest stack and structurally preserves the CreateFileA return address plus seven stdcall arguments. It performs no real file or device operation, returning `INVALID_HANDLE_VALUE` with 28-byte cleanup and stopping at the original caller-return breakpoint.*

실제 4th CHD에서 첫 CreateFileA thunk 호출을 확인했습니다. 이름은 실행별 ASLR 주소를 갖는 guest stack의 `\\.\NTICE`, caller return은 `0x00aeffbc`, breakpoint EIP는 `0x00aeffbd`였습니다. scalar 인자는 access `0xc0000000`, share `0x00000003`, security null, disposition `0x00000003`, flags `0`, template null입니다. 파일/장치 성공 의미와 이후 호출은 이번 범위에서 확정하지 않았습니다.

*Confirmed the first CreateFileA thunk call in the real 4th CHD. The name was `\\.\NTICE` on the guest stack at a per-run ASLR address, the caller return was `0x00aeffbc`, and breakpoint EIP was `0x00aeffbd`. Scalar arguments were access `0xc0000000`, share `0x00000003`, null security, disposition `0x00000003`, flags zero, and null template. File/device success semantics and later calls were not established in this scope.*

## 검증 / Validation

- Linux x86 Debug 제품 `re2dj`와 `re2dj_linux_native_in_process_probe`를 빌드했습니다.
- synthetic probe는 `imports=2 dynamic=2 exit=51 signal=4`를 출력했고, 두 번째 dynamic thunk의 7개 인자, `EAX=0xffffffff`, 호출 전후 ESP 일치를 검증했습니다.
- 실제 `roms/ez2dj4th/ez2dj4th.chd`에서 위 CreateFileA 호출과 stdcall 복귀를 확인했습니다. 원본 CHD는 읽기 전용으로 사용했습니다.
- Linux x64 Debug 제품과 `re2dj_unit_tests`를 빌드했습니다. 단위 테스트는 1,863 checks, 0 failures였습니다.
- Linux x64 제품은 같은 진단을 `Linux in-process CreateFileA diagnostic requires an i386 host` 오류와 예상 exit 3으로 거절했습니다.
- `git diff --check`를 통과했습니다.

*Validation performed:*

- *Built the Linux x86 Debug product `re2dj` and `re2dj_linux_native_in_process_probe`.*
- *The synthetic probe printed `imports=2 dynamic=2 exit=51 signal=4` and verified seven arguments, `EAX=0xffffffff`, and matching ESP before and after the second dynamic thunk.*
- *Confirmed the CreateFileA call and stdcall return above against the real `roms/ez2dj4th/ez2dj4th.chd`; the original CHD remained read-only.*
- *Built the Linux x64 Debug product and `re2dj_unit_tests`; unit tests reported 1,863 checks and zero failures.*
- *The Linux x64 product rejected the same diagnostic with the expected exit 3 and `Linux in-process CreateFileA diagnostic requires an i386 host` error.*
- *Passed `git diff --check`.*
