# 작업 387 작업 지시서 — DirectSound 2단계: 버퍼 제어 / Task 387 work order — DirectSound phase 2: buffer controls

설계: [20260926-387-directsound-controls.md](../design/20260926-387-directsound-controls.md)

## 절차 / Steps

1. 제어 규칙과 무음 재생을 core에 두고, `LegacyAudioBuffer`와 Windows facade가 쓰게 한다.
   *Put the control rules and silent playback in the core, and use them from `LegacyAudioBuffer` and the Windows facade.*
2. Linux 버퍼에 제어 메서드 12개를 더한다.
   *Add the twelve control methods to Linux buffers.*
3. 호출 한도를 올린다.
   *Raise the call limit.*
4. 검증과 문서.
   - 변경 전 build(`93234e2`)와 Windows 실제 4th를 비교한다(그래픽·오디오 기록).
   - 단위 테스트를 추가한다.
   - 문서를 쓴다.

   *Compare the real 4th on Windows against the pre-change build (`93234e2`) on its graphics and audio records, add unit tests, and write the documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 CTest를 통과한다. Windows 실제 4th의 기록이 변경 전과 같다.
  *Windows x86 and both Linux widths build and pass CTest, and the real 4th's record on Windows matches the pre-change build.*
- Linux 실제 4th가 두 폭에서 효과음 버퍼 설정을 마친다.
  *On both Linux widths the real 4th finishes setting up its sound-effect buffers.*
