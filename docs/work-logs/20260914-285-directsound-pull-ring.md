# DirectSound pull ring 작업 로그 / DirectSound pull-ring work log

## 한국어

실행 `20260914-013700-710`을 분석한 결과 JAM 파일 read는 모두 성공했지만 마지막 streaming
track의 cooked PCM은 peak 약 `0.000061`로 고정되어 무음이었습니다. guest의 전체 ring
Unlock은 계속됐으므로 파일시스템이 아니라 DirectSound ring을 SDL FIFO로 변환하는 경계가
원인이었습니다.

Streaming voice에 synchronized shadow ring과 원형 read cursor를 추가했습니다. Play와
Unlock은 guest ring을 shadow에 복사하며, `SDL_AudioStream` get callback은 mixer 요청량만
on-demand로 공급합니다. 기존 committed snapshot 비교와 전체 ring 선큐는 제거했습니다.
Stop 뒤 track 재생성, repeated Play continuation, 실제 소비 frame 기반 cursor 계산은
유지했습니다. 동일 buffer가 여러 곡에 재사용되어도 각 streaming Play의 갱신을 볼 수 있도록
Lock/Unlock 진단 sampling budget도 재설정하고 marker를 `shadow-offset/bytes`로 정리했습니다.

검증 결과:

- Windows x86 Debug 전체 빌드 성공
- `re2dj_audio_stream_progress_test` 통과
- `re2dj_ez2dancer_keyboard_input_test` 통과
- `re2dj_windows_product_loader_probe` 통과
- `re2dj_unit_tests` 통과

실제 JAM 연속 재생은 사용자의 수정 빌드 청취로 최종 확인해야 합니다.

## English

Analysis of run `20260914-013700-710` showed that every JAM file read succeeded, while the
final streaming track's cooked PCM remained effectively silent at a peak near `0.000061`.
Complete-ring Unlock calls continued, identifying the DirectSound-ring-to-SDL-FIFO boundary,
not the filesystem, as the cause.

A synchronized shadow ring and circular read cursor now belong to each streaming voice.
Play and Unlock copy the guest ring into the shadow, and an `SDL_AudioStream` get callback
supplies only the mixer's requested amount on demand. Committed-snapshot comparison and
full-ring prequeueing were removed. Track recreation after Stop, repeated-Play continuation,
and cursor calculation from actually consumed frames remain intact.
The Lock/Unlock diagnostic sampling budget also resets at each streaming Play, and its
markers now use `shadow-offset/bytes`, keeping later songs observable when a buffer is reused.

Verification results:

- Full Windows x86 Debug build passed
- `re2dj_audio_stream_progress_test` passed
- `re2dj_ez2dancer_keyboard_input_test` passed
- `re2dj_windows_product_loader_probe` passed
- `re2dj_unit_tests` passed

Continuous real JAM playback still requires final listening confirmation with this build.
