# 작업 로그 343: kernel32 모듈과 Linux i386 facade mapping / Work log 343: kernel32 module and Linux i386 facade mapping

## 결과 / Result

공용 `kernel32` descriptor를 추가하여 `kernel32.dll`/`kernel32` identity와 확인된 `GetModuleHandleA`, `GetProcAddress`, `GetVersion`, `CreateFileA` 네 export의 stdcall ABI를 한 곳에 선언했습니다. 아직 resolver context가 없는 앞의 두 export는 null failure, `GetVersion`은 기존 진단값 0, `CreateFileA`는 `INVALID_HANDLE_VALUE`를 반환합니다. 파일·장치 open이나 성공 stub은 추가하지 않았습니다.

*Added a shared `kernel32` descriptor that declares `kernel32.dll`/`kernel32` identity and the confirmed stdcall ABI for `GetModuleHandleA`, `GetProcAddress`, `GetVersion`, and `CreateFileA` in one place. The first two exports return null failure while resolver context is absent, `GetVersion` returns the existing diagnostic value zero, and `CreateFileA` returns `INVALID_HANDLE_VALUE`. No file/device open behavior or success stub was added.*

Linux i386 `NativeGuestModuleSet`은 gate table staging·registry·bridge dispatch를 소유하고, 별도 `native_guest_module_image` mapper가 64 KiB 정렬 후보에서 facade를 exact address로만 mapping합니다. mapper는 header와 section을 RW mapping에 복사한 뒤 전체 image를 R, executable section을 RX로 바꾸며 writable section은 거절합니다. 성공한 base와 export thunk를 registry에 등록하고, bridge gate에서 bounded stack 인자를 descriptor handler로 전달하여 EAX/EDX와 호출 규약별 cleanup을 반환합니다. build·mapping·registry 등록 실패 전에는 caller gate table을 변경하지 않습니다.

*The Linux i386 `NativeGuestModuleSet` owns gate-table staging, the registry, and bridge dispatch, while a dedicated `native_guest_module_image` mapper accepts only an exact 64-KiB-aligned candidate address. The mapper copies headers and sections into an RW mapping, then changes the complete image to R and executable sections to RX, rejecting writable sections. The successful base and export thunks enter the registry; bridge gates pass bounded stack arguments to descriptor handlers and return EAX/EDX plus calling-convention cleanup. The caller's gate table is unchanged until build, mapping, and registry registration all succeed.*

새 runtime probe는 첫 후보 `0x6F000000`을 먼저 점유하여 mapper가 다음 충돌 없는 base를 선택하는지 확인합니다. registry handle이 실제 facade base이고 thunk가 image 안에 있는지 검사하며, `/proc/self/maps`에서 header/`.edata` R, `.text` RX, W+X 없음도 확인합니다. facade 주소를 함수로 호출한 `GetVersion`은 EAX 0과 cleanup 0, `CreateFileA`는 EAX `0xFFFFFFFF`와 cleanup 28을 반환했고 두 호출 모두 ESP를 보존했습니다.

*The new runtime probe occupies the first `0x6F000000` candidate before registration to verify selection of the next collision-free base. It checks that the registry handle is the real facade base and thunks lie inside the image, then confirms R headers/`.edata`, RX `.text`, and no W+X mapping through `/proc/self/maps`. Calling the facade addresses as functions returned EAX zero and cleanup zero for `GetVersion`, and EAX `0xFFFFFFFF` with cleanup 28 for `CreateFileA`; both calls preserved ESP.*

## 검증 / Validation

- Windows x86 Debug: warnings-as-errors `re2dj_unit_tests` build 통과, targeted CTest 1/1 통과, 직접 실행 `checks: 2105, failures: 0`.
- Windows x64 Debug: warnings-as-errors `re2dj_unit_tests` build 통과, targeted CTest 1/1 통과, 직접 실행 `checks: 2105, failures: 0`.
- Linux x64 Debug: warnings-as-errors build와 CTest 1/1 통과, 직접 실행 `checks: 2105, failures: 0`.
- Linux x86 Debug: warnings-as-errors build와 CTest 2/2 통과. 두 test는 공용 unit test와 `re2dj_linux_native_guest_module_probe`이며, 직접 unit 실행은 `checks: 2105, failures: 0`.
- Windows x86 전체 CTest도 시도했지만 이 변경과 무관한 기존 `re2dj_windows_vfs_runtime_probe`가 완료되지 않아 중단했습니다. 해당 child/CTest process가 남지 않았음을 확인했고, 이 작업 범위의 `re2dj_unit_tests`는 별도로 통과했습니다.

*Validation performed:*

- *Windows x86 Debug: warnings-as-errors `re2dj_unit_tests` build passed, targeted CTest passed 1/1, and direct execution reported `checks: 2105, failures: 0`.*
- *Windows x64 Debug: warnings-as-errors `re2dj_unit_tests` build passed, targeted CTest passed 1/1, and direct execution reported `checks: 2105, failures: 0`.*
- *Linux x64 Debug: warnings-as-errors build and CTest passed 1/1; direct execution reported `checks: 2105, failures: 0`.*
- *Linux x86 Debug: warnings-as-errors build and CTest passed 2/2. The tests were the shared unit suite and `re2dj_linux_native_guest_module_probe`; direct unit execution reported `checks: 2105, failures: 0`.*
- *A full Windows x86 CTest run was also attempted, but the pre-existing, unrelated `re2dj_windows_vfs_runtime_probe` did not complete and the run was interrupted. No child or CTest process remained, and this task's `re2dj_unit_tests` passed separately.*

## 남은 범위 / Remaining scope

`native_create_file_observation.cpp`의 pseudo `0x7F000001` handle과 API별 resolver 조건문은 그대로 유지했습니다. 다음 작업 344에서 이 진단 경로를 module set/registry로 이관하고 static IAT와 dynamic resolver의 facade thunk identity 및 실제 4th CHD `\\.\NTICE` 회귀를 확인합니다.

*The pseudo `0x7F000001` handle and per-API resolver conditionals in `native_create_file_observation.cpp` remain unchanged. Task 344 migrates that diagnostic path to the module set/registry and verifies facade-thunk identity between static IAT and dynamic resolution plus the real 4th CHD `\\.\NTICE` regression.*
