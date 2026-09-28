# 작업 384 작업 지시서 — DirectInput 진입 / Task 384 work order — DirectInput entry

설계: [20260926-384-directinput-entry.md](../design/20260926-384-directinput-entry.md)

## 절차 / Steps

1. DirectInput core(`directinput.h/.cpp`)를 만든다.
   *Build the DirectInput core (`directinput.h/.cpp`).*
2. Windows DirectInput facade가 core를 쓰게 하고, SDK 검사를 더한다.
   *Put the Windows DirectInput facade on the core and add the SDK checks.*
3. Linux `dinput.dll` module을 만들고, resolve-only 목록에서 뺀다.
   *Add the Linux `dinput.dll` module and remove it from the resolve-only list.*
4. 검증과 문서.
   - 변경 전 build(`86c8d3e`)와 Windows 실제 4th를 비교한다(그래픽·입력 기록).
   - 단위 테스트를 추가한다.
   - 문서를 쓴다.

   *Compare the real 4th on Windows against the pre-change build (`86c8d3e`) on its graphics and input records, add unit tests, and write the documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 CTest를 통과한다. Windows 실제 4th의 기록이 변경 전과 같다.
  *Windows x86 and both Linux widths build and pass CTest, and the real 4th's record on Windows matches the pre-change build.*
- Linux 실제 4th가 두 폭에서 DirectInput 설정을 모두 지난다.
  *On both Linux widths the real 4th passes the whole DirectInput setup.*
