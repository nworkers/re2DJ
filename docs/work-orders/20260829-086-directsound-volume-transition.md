# DirectSound 데모 음량 설정 HLE 작업 지시

관련 설계: [DirectSound 데모 음량 설정 HLE](../design/20260829-086-directsound-volume-transition.md)

## 한국어

### 목표

확인된 `DemoVolume=0` 원인을 원본 자산 수정 없이 해결하고 DirectSound의 원래 상대 dB 의미를 보존한다.

### 작업

1. caller RVA와 실제 track/master gain을 기록하는 bounded trace를 유지한다.
2. `GAMEASSIGNMENTS/DemoVolume`만 재정의하고 다른 profile 요청은 pass-through하는 runtime thunk를 분리 구현한다.
3. launcher가 `--demo-volume <0..3>`을 검증하고 runtime 설정과 main-image IAT를 준비하도록 한다.
4. 제품 기본 데모 프로필을 3, master gain을 0 dB로 설정하고 CLI와 실행 인자를 갱신한다.
5. runtime/product probe를 확장하고 Windows x86 build와 CTest를 수행한다.
6. 실제 HDD trace에서 `SetVolume(0)`과 streaming queue 상태를 재검증한다.
7. analysis, architecture, README, guide, TODO, IMPLEMENTED와 작업 로그를 갱신한다.

### 완료 조건

- 원본 HDD의 `ez2dj.ini`와 실행 파일을 변경하지 않는다.
- 데모 음량 프로필 `0..3`이 각각 원본 테이블 의미를 유지한다.
- 다른 `GetPrivateProfileIntA` 요청은 기존 동작을 유지한다.
- 제품 기본 실행은 프로필 3과 master 0 dB를 사용한다.
- warnings-as-errors build와 CTest가 통과한다.

## English

### Goal

Resolve the confirmed `DemoVolume=0` cause without modifying original assets, while preserving the original DirectSound relative-dB semantics.

### Work

1. Retain bounded caller-RVA and actual track/master-gain tracing.
2. Implement a separate runtime thunk that overrides only `GAMEASSIGNMENTS/DemoVolume` and passes all other profile reads through.
3. Make the launcher validate `--demo-volume <0..3>`, configure the runtime, and prepare the main-image IAT.
4. Set product defaults to demo profile 3 and master gain 0 dB, updating CLI and launcher arguments.
5. Extend runtime/product probes and run the Windows x86 build and CTest.
6. Revalidate `SetVolume(0)` and streaming queue state with a real-HDD trace.
7. Update analysis, architecture, README, guide, TODO, IMPLEMENTED, and the work log.

### Completion criteria

Do not modify the original HDD INI or executable. Preserve profiles `0..3` and their original table meaning. Keep unrelated `GetPrivateProfileIntA` behavior unchanged. Use profile 3 and master 0 dB by default. Pass the warnings-as-errors build and CTest.
