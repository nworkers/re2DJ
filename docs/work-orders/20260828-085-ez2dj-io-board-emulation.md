# EZ2DJ I/O board 에뮬레이션 작업 지시

관련 설계: [EZ2DJ I/O board 에뮬레이션](../design/20260828-085-ez2dj-io-board-emulation.md)

## 한국어

### 상태

**완료.** 공개 port protocol을 독립적인 semantic board와 configurable Windows keyboard 입력으로 구현하고 build·CTest·원본 실행을 검증했다.

### 작업

1. 2EZConfig-V2 commit과 GPL-3.0 경계를 분석 문서에 기록하고 코드 재사용을 금지한다.
2. 플랫폼 중립 `Ez2DjIoBoard`에 button, turntable, coin state, light state와 byte port 변환을 구현한다. 초기 latch 해석은 작업 087에서 누적 counter로 정정됐다.
3. 기존 `LegacyIoPortBus`가 semantic board를 사용하되 raw input/output 호환 API를 유지하게 한다.
4. Windows INI keyboard adapter와 `--io-config` 제품·launcher·runtime 주입 경계를 추가한다.
5. 공용 unit test, product loader probe와 Windows runtime probe를 확장한다.
6. architecture, EXE design, analysis index, guide, TODO와 implemented 문서를 갱신한다.
7. Windows x86 build·CTest와 실제 원본 idle 실행을 검증하고 작업 로그를 남긴다.

### 완료 조건

- 공개된 0x100..0x106 protocol이 독립 공용 모델로 표현된다.
- 외부 INI keyboard binding으로 원본의 button, coin과 turntable input을 공급할 수 있다.
- 원본 output byte가 semantic light state로 해석되고 향후 host sink에서 조회 가능하다.
- 설정이 없거나 잘못돼도 fail-safe idle input으로 원본 실행을 유지한다.
- GPL source/dependency가 저장소에 유입되지 않는다.

## English

### Status

**Complete.** The public port protocol is independently implemented as a semantic board with configurable Windows keyboard input, and verified by build, CTest, and an original run.

### Work and completion

Record the analyzed 2EZConfig-V2 commit and GPL boundary without reusing code; implement platform-neutral button, turntable, coin-latch, light, and byte-port behavior; retain raw `LegacyIoPortBus` compatibility; add Windows INI keyboard configuration transported by product `--io-config` through the launcher and runtime; extend unit/product/runtime probes; update cumulative documentation; and verify the Windows x86 build, CTest, and a real idle run.

Completion requires usable external keyboard bindings, decoded semantic light state, fail-safe idle behavior, no regression to original execution, and no GPL source or dependency in the repository.
