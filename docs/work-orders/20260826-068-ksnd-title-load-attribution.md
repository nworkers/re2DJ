# KSND title.wav 로드 실패 귀속 작업 지시

관련 설계: [KSND title.wav 로드 실패 귀속](../design/20260826-068-ksnd-title-load-attribution.md)

## 상태

**완료**

## 작업 범위

1. 기존 로그와 원본 WAV header로 path/open/read 성공 범위를 문서화한다.
2. launcher에 target 한정 `--ksnd-load-trace`와 네 return-stage 진단을 추가한다.
3. Windows x86 build와 CTest를 수행한다.
4. canonical 실행 두 번에서 동일 실패 stage/HRESULT와 access violation 유무를 확인한다.
5. TODO, 누적 분석과 작업 로그를 갱신하고 커밋한다.

## 완료 조건

- `title.wav` 실패가 parser, CreateSoundBuffer, Lock, Unlock 중 한 단계로 반복 귀속된다.
- 진단은 원본 흐름과 반환값을 바꾸지 않는다.
- 원본 자산은 읽기 전용으로 유지된다.

---

# KSND title.wav Load-Failure Attribution Work Order

Related design: [KSND title.wav Load-Failure Attribution](../design/20260826-068-ksnd-title-load-attribution.md)

## Status and scope

**Complete.** Documented the confirmed path/open/read boundary, added the target-limited rearming `--ksnd-load-trace` diagnostic, passed Windows x86 build and tests, and reproduced parser success followed by ten CreateSoundBuffer E_NOTIMPL results with no access violation in two canonical runs.
