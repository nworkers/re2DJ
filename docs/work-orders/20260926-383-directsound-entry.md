# 작업 383 작업 지시서 — DirectSound 진입과 창 조회 / Task 383 work order — DirectSound entry and window queries

설계: [20260926-383-directsound-entry.md](../design/20260926-383-directsound-entry.md)

## 절차 / Steps

1. `GetForegroundWindow`을 구현한다. `GetWindowLongA`는 32비트 PowerShell로 측정한 뒤 구현한다.
   *Implement `GetForegroundWindow`, and implement `GetWindowLongA` after measuring it with 32-bit PowerShell.*
2. DirectSound core(ABI, 버퍼 생성, caps, 복제, lock)를 만든다. Windows facade와 `LegacyAudioBuffer`가 core를 쓰게 한다.
   *Build the DirectSound core (ABI, buffer creation, caps, duplication, locks), and put the Windows facade and `LegacyAudioBuffer` on it.*
3. Linux `dsound.dll` module을 만들고, resolve-only 목록에서 뺀다.
   *Add the Linux `dsound.dll` module and remove it from the resolve-only list.*
4. 검증과 문서.
   - 변경 전 build(`ea799d1`)와 Windows 실제 4th를 비교한다.
   - 단위 테스트를 추가한다.
   - 문서를 쓴다.

   *Compare the real 4th on Windows against the pre-change build (`ea799d1`), add unit tests, and write the documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 CTest를 통과한다. Windows 실제 4th의 기록이 변경 전과 같다.
  *Windows x86 and both Linux widths build and pass CTest, and the real 4th's record on Windows matches the pre-change build.*
- Linux 실제 4th가 두 폭에서 DirectSound 진입을 지나 DirectInput까지 간다.
  *On both Linux widths the real 4th passes the DirectSound entry and reaches DirectInput.*
