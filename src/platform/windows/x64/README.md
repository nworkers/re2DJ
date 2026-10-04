# src/platform/windows/x64

Windows x64(64비트 host 실행 파일) 전용 코드입니다. 제품은 아직 Windows x86만 빌드하며, 이 디렉터리에는 x64 backend를 정하기 위한 조사 probe만 있습니다.

*Code for Windows x64, a 64-bit host executable. The product still builds Windows x86 only; this directory holds only the research probe for deciding on an x64 backend.*

- `native_compat_mode_probe.cpp`(작업 452, `re2dj_windows_x64_compat_mode_probe`): 일반 x64 프로세스에서 CS `0x23`으로 32비트 코드를 실행하고, 게스트 FS(`wrfsbase`), 문맥 전환 때의 FS base, 호환 모드 예외의 VEH 전달과 재개를 질문별 자식 프로세스로 확인합니다. 전환 코드는 4 GiB 아래 고정 페이지(0x10000000)에 복사하는 기계어 바이트이며, 어셈블리를 옆에 주석으로 둡니다. 제품과 CTest에는 들어가지 않습니다. 결과: [Windows x64 compatibility mode](../../../../docs/kb/windows-x64-compatibility-mode.md), 근거: [작업 452 설계](../../../../docs/design/20261004-452-windows-x64-compat-mode-probe.md).

*`native_compat_mode_probe.cpp` (task 452, `re2dj_windows_x64_compat_mode_probe`) runs 32-bit code in CS `0x23` inside a plain x64 process and checks, one child process per question, the guest FS through `wrfsbase`, the FS base across context switches, and the VEH delivery and resumption of compatibility-mode exceptions. Its transition code is machine-code bytes copied to a fixed page below 4 GiB (0x10000000) with the assembly beside them in comments; it joins neither the product nor CTest. Results: [Windows x64 compatibility mode](../../../../docs/kb/windows-x64-compatibility-mode.md); rationale: [task 452 design](../../../../docs/design/20261004-452-windows-x64-compat-mode-probe.md).*

빌드 / Build:

```powershell
cmake -S . -B build\windows-x64-probe -G "Visual Studio 18 2026" -A x64 -DRE2DJ_BUILD_TESTS=OFF
cmake --build build\windows-x64-probe --config Debug --target re2dj_windows_x64_compat_mode_probe
.\build\windows-x64-probe\bin\Debug\re2dj_windows_x64_compat_mode_probe.exe
```
