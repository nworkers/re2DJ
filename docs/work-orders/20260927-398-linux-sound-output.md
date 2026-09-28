# 작업 398 작업 지시서 — Linux 소리 출력 / Task 398 work order — Linux sound output

설계: [20260927-398-linux-sound-output.md](../design/20260927-398-linux-sound-output.md)

## 절차 / Steps

1. `hle::HostAudio` 계약과 서비스 연결을 더한다.
   *Add the `hle::HostAudio` contract and its plumbing through the services.*
2. Linux `dsound.dll`에 voice와 host 사본을 붙이고, Windows facade의 호출 순서를 따른다.
   *Give Linux `dsound.dll` buffers a voice and host copy, following the Windows facade's call order.*
3. Linux에서 SDL 오디오와 SDL3_mixer를 켜고, `LinuxHostAudio`를 공용 backend 위에 만든다.
   *Enable SDL audio and SDL3_mixer on Linux, and build `LinuxHostAudio` on the shared backend.*
4. 단위 테스트, 실제 실행, Windows 전후 비교, 문서.
   *Unit tests, a real run, a Windows before/after comparison, and documentation.*

## 완료 조건 / Done when

- Windows x86과 Linux 두 폭이 build와 CTest를 통과하고, Windows 실제 4th의 그래픽·오디오 기록이 작업 전과 같다.
  *Windows x86 and both Linux widths build and pass CTest, and the real 4th's graphics and audio logs on Windows match the ones before the task.*
- Linux 두 폭이 SDL3_mixer 재생 장치를 열고, 게임의 재생 커서가 장치의 진행을 따른다.
  *Both Linux widths open an SDL3_mixer playback device, and the game's play cursors follow the device.*
