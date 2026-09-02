# VFS 호스트 절대 경로 재해석 작업 지시

관련 설계: [VFS 호스트 절대 경로 재해석 설계](../design/20260903-142-ez2dj4th-vfs-absolute-path.md)

*Related design: [VFS host-absolute path remapping design](../design/20260903-142-ez2dj4th-vfs-absolute-path.md).*

## 작업 내용 / Work items

1. `src/platform/windows/injected_runtime.cpp`에 설정된 VFS root 하위 경로를
   찾는 case-insensitive, separator-tolerant suffix helper를 추가합니다.
2. `MapVfsPath`가 HDD root와 HLE Windows root 아래의 host absolute path를
   기존 상대 suffix mapping으로 연결하도록 수정합니다.
3. `GuestHddSuffix`가 같은 HDD absolute path를 CHD 상대 경로로 변환하도록
   수정합니다.
4. `windows_vfs_runtime_probe`에 absolute read와 copy-on-write write 회귀 검증을
   추가합니다.
5. 설계와 구현 결과를 반영한 작업 로그를 남깁니다.

*Work items: add a case-insensitive, separator-tolerant suffix helper for paths
under configured VFS roots; route host absolute paths under the HDD and HLE
Windows roots through the existing relative-suffix mapping; make
`GuestHddSuffix` convert the same HDD absolute paths for CHD lookup; add
absolute read and copy-on-write write regression coverage to
`windows_vfs_runtime_probe`; and leave a work log covering the design and result.*

## 완료 조건 / Completion criteria

- `C:\...\EZ2DJ\EZ2DJ.ini`가 `C:\...\EZ2DJ\C:\...`로 변환되지 않습니다.
- HDD root 아래 absolute read가 성공합니다.
- HDD root 아래 absolute write가 overlay에만 기록되고 원본 HDD는 변하지 않습니다.
- 기존 VFS runtime probe, unit test, Windows x86 build, CTest가 통과합니다.
- `ez2dj4th` 실행에서 Hardlock은 계속 통과하고, `EZ2DJ.ini` 이후 로그가 새
  경계를 보여줍니다.

*Completion requires that `C:\...\EZ2DJ\EZ2DJ.ini` is not transformed into
`C:\...\EZ2DJ\C:\...`; absolute reads under the HDD root succeed; absolute
writes affect only the overlay; the existing runtime probe, unit tests, Windows
x86 build, and CTest pass; and the `ez2dj4th` run continues to pass Hardlock and
shows a new boundary after `EZ2DJ.ini`.*

## 검증 명령 / Verification commands

```powershell
cmake --build build\windows-x86 --config Debug
ctest --test-dir build\windows-x86 -C Debug --output-on-failure
.\build\windows-x86\bin\Debug\re2dj.exe ez2dj4th
```

*The same commands are used for the Windows x86 Debug build, CTest, and the
4th product run.*
