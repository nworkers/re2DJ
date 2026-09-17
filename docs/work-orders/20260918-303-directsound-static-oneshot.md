# 작업 303 작업 지시 — 일회성 DirectSound 버퍼의 스트리밍 오분류 수정 / Task 303 work order — Fixing one-shot DirectSound buffers misclassified as streaming

설계: [20260918-303-directsound-static-oneshot.md](../design/20260918-303-directsound-static-oneshot.md)

## 한국어

### 구현 범위

1. **변경 전 수집.** `ez2dj1st`, `ez2dj1stse`, `ez2dj2nd`, `ez2dj3rd`, `ez2dj4th`, `ez2dj5th`, `ez2d2m`을 `--audio-volume-trace`로 실행해 버퍼 생성·재생 플래그 표를 만든다. 설계 가정과 어긋나면 구현 전에 설계를 고친다.
2. **분류 규칙의 공용화.** 스트리밍 판정을 플랫폼 공용 함수로 옮기고 `DSBCAPS_STATIC` 일회성 버퍼를 제외한다. 360,448 크기 규칙은 유지한다.
3. **스트리밍 경로의 `DSBPLAY_LOOPING` 준수.** 반복 없이 재생된 스트리밍 버퍼는 한 바퀴 뒤 공급을 멈추고 정지를 보고한다.
4. **테스트.** 분류 규칙 단위 테스트, 스트리밍 한 바퀴 정지 테스트.
5. **변경 후 수집과 사용자 청취.**
6. **문서.** `docs/kb/legacy-directsound-buffer.md`에 `DSBPLAY_LOOPING`·`DSBCAPS_STATIC` 배경(링크 확인 후), 분석·작업 로그.

### 범위에서 뺀 것

360,448 크기 규칙 제거, 오디오 로그 줄 수 제한 변경, `ez2dj6th` 자식 프로세스 오디오.

## English

### Scope

1. **Collect before changing**: run `ez2dj1st`, `ez2dj1stse`, `ez2dj2nd`, `ez2dj3rd`, `ez2dj4th`, `ez2dj5th` and `ez2d2m` with `--audio-volume-trace` and tabulate buffer creation and play flags, revising the design before implementing if the table contradicts it.
2. **Move the classification rule into a platform-neutral function** and exclude `DSBCAPS_STATIC` one-shot buffers, keeping the 360,448 size rule.
3. **Make the streaming path honor `DSBPLAY_LOOPING`**: a streaming buffer played without looping stops feeding after one pass and reports itself stopped.
4. **Tests**: classification unit tests and a one-pass streaming stop test.
5. **Collect after changing, and user listening.**
6. **Documents**: `DSBPLAY_LOOPING` and `DSBCAPS_STATIC` background in `docs/kb/legacy-directsound-buffer.md` (after verifying links), plus analysis and the work log.

### Out of scope

Dropping the 360,448 size rule, changing the audio log line limit, and `ez2dj6th` child-process audio.
