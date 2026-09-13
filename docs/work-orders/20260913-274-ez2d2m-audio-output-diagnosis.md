# 작업 지시서: EZ2Dancer SDL 오디오 출력 경계 진단
# Work Order: Diagnose the EZ2Dancer SDL Audio Output Boundary

## 한국어

### 목표

JAM의 CHD/VFS read와 DirectSound streaming 공급 이후 실제 SDL_mixer output 및
playback device에서 음원이 소비되는지 확인합니다.

### 범위

- SDL playback device 생성 성공 여부와 memory-only fallback 여부를 trace합니다.
- SDL_mixer mixer output format과 post-mix PCM peak/RMS를 제한적으로 trace합니다.
- streaming track의 queued bytes와 playing/stopped lifecycle을 대조합니다.
- SDL_mixer streaming exhaustion 정책이 설계와 일치하는지 검증합니다.
- 원본 바이너리와 원본 자산은 수정하지 않습니다.

### 현재 판단

- **확인됨:** JAM 파일 open/read와 DirectSound buffer 공급은 성공했습니다.
- **확인됨:** streaming Play가 exhaustion false 정책을 설정하지 않습니다.
- **확인됨:** playback device 실패 시 memory-only mixer fallback이 가능합니다.
- **미확정:** 사용자가 제공한 JAM 실행이 어느 후보를 실제로 사용했는지입니다.

### 검증 기준

- 출력 device 상태와 post-mix callback 관측을 bounded audio trace에 남깁니다.
- stream queue가 비어도 track이 stopped로 고정되지 않는지 확인합니다.
- 실제 playback device가 없는 환경에서 성공처럼 보이는 실행을 구분합니다.
- 진단 결과에 따라 별도 구현 설계를 갱신합니다.

## English

### Objective

Determine whether JAM audio is consumed by the SDL_mixer output and playback device after
the successful CHD/VFS read and DirectSound streaming upload.

### Scope

- Trace playback-device creation and the memory-only fallback.
- Add bounded mixer output-format and post-mix PCM peak/RMS observations.
- Compare streaming queued bytes with the track playing/stopped lifecycle.
- Verify that the SDL_mixer streaming exhaustion policy matches the design.
- Do not modify the original executable or original assets.

### Current assessment

- **Confirmed:** JAM file open/read and DirectSound buffer delivery succeed.
- **Confirmed:** streaming Play does not set the exhaustion-false policy.
- **Confirmed:** a failed playback-device creation can fall back to a memory-only mixer.
- **Unresolved:** which candidate was active in the user's JAM run.

### Verification criteria

- Record device state and post-mix callback observations in bounded audio diagnostics.
- Confirm that a temporary empty stream queue does not permanently stop the track.
- Distinguish a successful-looking run with no physical playback device.
- Update the implementation design based on the diagnostic result.

## 구현 결과

- [x] 스트리밍 트랙에 `MIX_PROP_PLAY_HALT_WHEN_EXHAUSTED_BOOLEAN=false` 적용
- [x] SDL mixer 준비 상태, 실제 playback device 여부, 출력 포맷 기록
- [x] 초기 무음과 non-zero 구간을 분리한 제한적 post-mix PCM peak/RMS 기록
- [x] streaming voice별 cooked PCM peak/RMS 기록
- [x] Windows x86 Debug 빌드 및 관련 테스트 검증
- [ ] 실제 JAM 재생 로그에서 post-mix 출력과 track 상태 확인

## Implementation result

- [x] Set `MIX_PROP_PLAY_HALT_WHEN_EXHAUSTED_BOOLEAN=false` for streaming tracks.
- [x] Trace SDL mixer readiness, physical playback-device availability, and output format.
- [x] Trace post-mix PCM peak/RMS for a bounded number of callbacks.
- [x] Verify the Windows x86 Debug build and related tests.
- [ ] Confirm post-mix output and track state in a new real JAM playback log.
