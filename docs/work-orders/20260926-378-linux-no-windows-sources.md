# 작업 378 작업 지시서 — Linux가 Windows 소스를 참조하지 않게 / Task 378 work order — Linux references no Windows sources

설계: [20260926-378-linux-no-windows-sources.md](../design/20260926-378-linux-no-windows-sources.md)

## 절차 / Steps

1. `src/`, `include/`, `tests/`와 CMake의 Linux 블록에서 `src/platform/windows` 참조를 찾는다.
   *Find references to `src/platform/windows` in `src/`, `include/`, `tests/`, and CMake's Linux blocks.*
2. fixture를 `src/platform/native_probe_fixture`로 옮기고, Linux probe 3개와 Windows 파일을 고친다. Linux probe target에 fixture 소스를 더한다.
   *Move the fixture to `src/platform/native_probe_fixture`, fix the three Linux probes and the Windows file, and add the fixture source to the Linux probe targets.*
3. 두 폭의 helper script, probe, CTest, 실제 4th 회귀와 Windows build·CTest를 확인한다.
   *Check both widths' helper script, probes, CTest, and the real 4th regression, and the Windows build and CTest.*

## 완료 조건 / Done when

- Linux 코드와 Linux target에서 `src/platform/windows` 참조가 없고, 모든 probe 출력이 전과 같다.
  *No Linux code or Linux target references `src/platform/windows`, and every probe prints what it did before.*
