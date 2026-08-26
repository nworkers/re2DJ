# DirectSound duplicate buffer HLE 작업 지시

관련 설계: [DirectSound duplicate buffer HLE](../design/20260826-071-directsound-duplicate-buffer-hle.md)

## 상태

**완료.** shared PCM, 독립 상태와 SDL voice, Windows facade, probe와 단위 테스트를 구현했다. x86/x64 검증과 canonical 두 실행이 통과했고 원본은 실패 종료 대신 메인 루프를 유지한다.

## 작업

1. `LegacyAudioBuffer`에 PCM shared ownership과 독립 상태 duplicate를 추가하고 단위 테스트한다.
2. `DirectSoundFacade::DuplicateSoundBuffer`가 새 facade와 SDL voice를 만들고 관찰 marker를 남기도록 구현한다.
3. Windows x86 runtime probe에 duplicate와 독립 control/play 검증을 추가한다.
4. warnings-as-errors x86/x64 빌드와 CTest를 통과시킨다.
5. canonical 실행 두 번으로 기존 KSND 종료 소멸과 다음 경계를 확인한다.
6. architecture, TODO, analysis, implemented와 작업 로그를 결과에 맞춰 갱신한다.

---

# DirectSound Duplicate Buffer HLE Work Order

Related design: [DirectSound Duplicate Buffer HLE](../design/20260826-071-directsound-duplicate-buffer-hle.md)

**Complete.** Shared PCM, independent state and SDL voices, the Windows facade, probe, and unit tests are implemented. x86/x64 verification and two canonical runs pass, and the original executable remains in its main loop instead of taking the former failure exit.

Tasks: add shared PCM plus independent duplicated state to LegacyAudioBuffer; implement the Windows facade with a new SDL voice and markers; extend the x86 probe; pass warnings-as-errors x86/x64 builds and CTest; run the canonical executable twice; then update cumulative documentation and the work log.
