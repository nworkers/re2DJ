# 스트리밍 보충 순서 수정 작업 로그 / Streaming refill order correction log

## 한국어

`011631-574`의 1~2초 반복은 소비량 기반 코드가 guest가 갱신하지 않은 ring 시작점을
다시 queue에 넣어서 발생했습니다. committed snapshot 방식으로 되돌린 뒤 합성 테스트는
변경 없는 Unlock에서 queue가 한 ring을 넘지 않는 것과 변경 tone의 cooked output 전달을
확인했습니다.

Windows x86 Debug 빌드와 오디오 회귀 테스트, 기존 단위/입력/loader 테스트 4개가 모두
통과했습니다. 실제 JAM 연속 재생은 새 빌드로 사용자 확인이 필요합니다.

## English

The 1–2 second repetition in `011631-574` came from the consumption-based code requeueing
the ring beginning before the guest refreshed it. Restoring committed-snapshot refill makes
the synthetic test keep unchanged Unlock calls within one ring and deliver changed tone data
to cooked output.

The Windows x86 Debug build, audio regression, and four existing unit/input/loader tests all
pass. Continuous real JAM playback still needs confirmation with the new build.
