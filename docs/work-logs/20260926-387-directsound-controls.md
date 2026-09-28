# 작업 387 작업 로그 — DirectSound 2단계: 버퍼 제어 / Task 387 work log — DirectSound phase 2: buffer controls

설계: [20260926-387-directsound-controls.md](../design/20260926-387-directsound-controls.md)
작업 지시서: [20260926-387-directsound-controls.md](../work-orders/20260926-387-directsound-controls.md)

## 진행 / Progress

제어 메서드를 더하자 Linux 실행은 4,096번 호출 한도에 닿았다. api.log에 따르면 그 구간은 효과음 로딩이다. 버퍼 생성 56번, 복제 168번, 버퍼마다 `Stop`·`SetCurrentPosition`·`SetPan`·`SetVolume`, 그리고 event와 heap 할당이 이어진다.

한도를 32,768로 올리자, 게임은 약 12,000번 호출에서 로딩을 마치고 `user32!GetAsyncKeyState(VK_TAB)`에서 멈췄다.

*With the control methods in place, the Linux run hit the 4,096-call limit. The api.log shows that stretch is sound-effect loading: 56 buffer creations, 168 duplications, `Stop`, `SetCurrentPosition`, `SetPan`, and `SetVolume` on each buffer, plus events and heap allocations.*

*With the limit raised to 32,768, the game finished loading at about 12,000 calls and stopped at `user32!GetAsyncKeyState(VK_TAB)`.*

## 변경 / Changes

- **core**:
  - `WrapPosition`, `ClampVolume`, `ClampPan`, `ResolveFrequency`, `BufferStatus`, `AdvanceSilentPlayback`.
  - play·status·주파수·범위 상수.

  ***Core:***
  - *`WrapPosition`, `ClampVolume`, `ClampPan`, `ResolveFrequency`, `BufferStatus`, and `AdvanceSilentPlayback`.*
  - *Play, status, frequency, and range constants.*
- **Windows**:
  - `LegacyAudioBuffer`의 위치·볼륨·pan, facade의 주파수·상태가 core를 쓴다.
  - SDK 검사를 추가했다.

  ***Windows:***
  - *`LegacyAudioBuffer`'s position, volume, and pan, and the facade's frequency and status, use the core.*
  - *SDK checks are added.*
- **Linux**:
  - `IDirectSoundBuffer` 제어 메서드 12개를 더했다(구현 21개 중 21개).
  - 복제본의 제어값 복사를 더했다.
  - 호출 한도를 32,768로 올렸다.

  ***Linux:***
  - *Twelve `IDirectSoundBuffer` control methods are added, so all 21 are implemented.*
  - *Duplicates copy their original's controls.*
  - *The call limit is raised to 32,768.*
- **단위 테스트**:
  - core: 범위, 주파수, 상태, 무음 재생의 wrap과 끝.
  - dsound: 볼륨 자르기, 주파수, 위치, looping 재생 cursor 진행, 정지, 한 번 재생의 끝, 복제본의 볼륨.

  ***Unit tests:***
  - *Core: ranges, frequency, status, and silent playback's wrap and end.*
  - *dsound: volume clamping, frequency, position, the looping play cursor's progress, stopping, a one-shot play's end, and a duplicate's volume.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Windows x86 build, CTest | exit 0, 우리 코드 경고·오류 없음, 6/6 / exit 0, no warnings or errors from this project, 6/6 |
| Windows 실제 4th, 변경 전(`93234e2`)·후 각 30초 / real 4th on Windows, pre-change (`93234e2`) and post-change, 30 s each | ddraw 기록 1,410줄이 같다. 오디오 기록은 변경 전 1,364줄이 순서까지 모두 같다. 변경 후에는 streaming ring 재충전(lock/unlock) 3줄이 끝에 더 있다. 30초 안에 몇 번 재충전되는지는 타이밍에 따라 달라진다. streaming이 아닌 줄은 모두 같다. / *The 1,410 ddraw lines match. All 1,364 pre-change audio lines match in order. The post-change run has 3 more streaming-ring refill lines (lock/unlock) at the end; how many refills fit in 30 s depends on timing. Every non-streaming line matches.* |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 / no warnings or errors, 3/3 each |
| 실제 4th, Linux 두 폭 / real 4th, both Linux widths | 호출 12,004번이며, 주소와 시계 값을 정규화하면 두 폭이 같다. `#12004 user32.dll!GetAsyncKeyState(9)`에서 멈춘다. / *12,004 calls, identical on both widths after address and clock normalization, stopping at `#12004 user32.dll!GetAsyncKeyState(9)`.* |

## 다음 / Next

입력 조회(`GetAsyncKeyState`)다. Linux host 입력은 아직 연결되지 않았다. 그래서 먼저 Windows 11 동작을 측정하고, "눌린 키 없음"을 측정한 형식대로 답한다. 이후 host 입력 단계에서 DirectInput과 같은 `InputSnapshot`으로 연결한다.

*Next is the input query (`GetAsyncKeyState`). Linux host input is not connected yet, so the Windows 11 behavior is measured first and "nothing held" is answered in that measured form. The host-input phase will later connect it through the same `InputSnapshot` DirectInput uses.*
