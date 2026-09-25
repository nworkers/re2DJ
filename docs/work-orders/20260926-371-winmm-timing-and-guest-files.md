# 작업 371 작업 지시서 — winmm 시간과 게스트 파일 / Task 371 work order — winmm timing and guest files

설계: [20260926-371-winmm-timing-and-guest-files.md](../design/20260926-371-winmm-timing-and-guest-files.md)

## 절차 / Steps

1. `winmm_module`을 만들고, 해석 전용 목록과 Linux 등록을 고친다.
   *Add `winmm_module` and adjust the resolve-only list and Linux registration.*
2. `GuestFiles`와 `GuestFileSource`(CHD adapter 포함)를 추가한다. `ImportCallServices::Files()`를 둔다.
   *Add `GuestFiles` and `GuestFileSource` (with the CHD adapter), and `ImportCallServices::Files()`.*
3. kernel32 `CreateFileA`, `ReadFile`, `WriteFile`, `SetFilePointer`, `GetFileSize`, `CloseHandle`을 연결한다. Win32 오류 5, 29, 30, 80, 131, 183을 추가한다.
   *Wire kernel32 `CreateFileA`, `ReadFile`, `WriteFile`, `SetFilePointer`, `GetFileSize`, and `CloseHandle`, adding Win32 errors 5, 29, 30, 80, 131, and 183.*
4. `OriginalRunEnvironment`를 도입한다. CLI의 CHD 실행은 파일 설정(`overlays/<profile>`)을 채운다.
   *Introduce `OriginalRunEnvironment`, with the CLI's CHD run filling the file settings (`overlays/<profile>`).*
5. 단위 테스트, 문서 갱신.
   *Unit tests and documentation.*

## 완료 조건 / Done when

- Linux 두 폭과 Windows x86 build·CTest가 통과하고, 기존 진단·probe가 그대로다.
  *Both Linux widths and Windows x86 build and pass CTest, with existing diagnostics and probes unchanged.*
- 실제 4th가 두 폭에서 `EZ2DJ.ini`를 CHD에서 읽고 `LoadIconA`까지 간다.
  *On both widths the real 4th reads `EZ2DJ.ini` from the CHD and reaches `LoadIconA`.*
