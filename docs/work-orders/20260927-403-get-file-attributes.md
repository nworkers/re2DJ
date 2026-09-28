# 작업 403 작업 지시서 — GetFileAttributesA / Task 403 work order — GetFileAttributesA

설계: [20260927-403-get-file-attributes.md](../design/20260927-403-get-file-attributes.md)

## 절차 / Steps

1. 게임이 묻는 경로를 API 기록에 남기고 CHD에서 대상 파일을 확인한다.
   *Record the path the game asks about in the API log and find the file on the CHD.*
2. Windows 11에서 `GetFileAttributesA`의 성공 값과 오류를 측정한다.
   *Measure `GetFileAttributesA`'s success values and errors on Windows 11.*
3. 공용 규칙(`DescribeGuestFileAttributes`), Linux `GuestFiles::Attributes`와 kernel32 export, Windows VFS hook을 구현한다.
   *Implement the shared rule, Linux `GuestFiles::Attributes` and the kernel32 export, and the Windows VFS hook.*
4. 단위 테스트, Windows VFS runtime probe, Windows 실제 실행 비교, Linux 실제 실행, 문서.
   *Unit tests, the Windows VFS runtime probe, a Windows real-run comparison, a Linux real run, and documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 테스트를 통과한다.
  *Windows x86 and both Linux widths build and pass their tests.*
- Windows 제품의 60초 기록이 의도한 resolver 경로 변경 말고는 기준과 같다.
  *The Windows product's 60-second logs match the baseline apart from the intended resolver route change.*
- Linux 실제 4th가 `GetFileAttributesA`를 지난다.
  *On Linux the real 4th gets past `GetFileAttributesA`.*
