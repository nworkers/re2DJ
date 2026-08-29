# 키보드 코인 누적 카운터 수정 작업 지시

관련 설계: [키보드 코인 누적 카운터 수정](../design/20260829-087-coin-counter-keyboard-input.md)

## 한국어

### 목표

키보드 coin rising edge를 원본 port `0x105`의 modulo-256 누적 카운터로 전달하여 press 한 번에 credit 한 개만 증가하게 한다.

### 작업

1. `Ez2DjIoBoard`의 consume-on-read coin pulse를 stable 8-bit counter로 교체한다.
2. false→true 전이만 counter를 증가시키고 hold/release/read는 값을 유지한다.
3. 초기값, hold, release, 다음 press와 wrap 회귀 테스트를 추가한다.
4. 기존 작업 085 설계·로그와 I/O 분석에서 잘못된 pulse 설명을 정정한다.
5. Windows x86 전체 build와 CTest를 수행하고 작업 로그를 작성한다.

### 완료 조건

- F3 hold와 반복 polling이 추가 coin delta를 만들지 않는다.
- 각 새 press는 정확히 counter 1 증가를 만든다.
- 기존 button, turntable과 light test가 회귀하지 않는다.
- warnings-as-errors build와 CTest가 통과한다.

## English

### Goal

Translate each keyboard coin rising edge into the original port `0x105` modulo-256 cumulative counter so one press adds one credit.

### Work and completion criteria

Replace consume-on-read coin pulses with a stable eight-bit counter, increment only on false-to-true transitions, add initial/hold/release/repress/wrap regression coverage, correct Task 085 and I/O analysis documentation, pass the complete Windows x86 warnings-as-errors build and CTest, and leave a work log. Holding or polling must not create another delta, while every new press increments exactly once without regressing other controls or lights.
