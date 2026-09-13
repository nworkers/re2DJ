# 작업 로그: EZ2Dancer streaming voice별 PCM 진단

## 한국어

### 분석 결과

`20260914-001047-224` 로그에서 SDL playback device와 aggregate mixer output은 정상으로
확인되었다. 초기 효과음에서 `sdl3:postmix` peak가 약 `0.6`으로 기록되었지만, 전역
post-mix 샘플 한도 때문에 JAM streaming voice의 출력 여부는 분리할 수 없었다.

### 변경 내용

각 SDL_mixer voice에 cooked callback을 연결하고, streaming 재생 직후 해당 voice만
bounded trace하도록 수정했다. 새 로그에는 다음 형식의 항목이 추가된다.

```text
sdl3:track-cooked:streaming=1:...:peak=...:rms=...
```

이 값은 입력 stream이 mixer track 출력 포맷으로 변환된 뒤의 PCM이다. 따라서 값이
non-zero이면 JAM 데이터 로딩 및 track 변환은 통과한 것이고, 0이면 stream 공급 또는
track 변환 경계를 계속 조사해야 한다.

### 검증

- `cmd /c scripts\build_win32.bat` 성공
- `re2dj_ez2dj_keyboard_input_test` 성공
- `re2dj_ez2dancer_keyboard_input_test` 성공
- `re2dj_windows_product_loader_probe` 성공
- `re2dj_unit_tests` 성공
- 새 실제 JAM 실행 로그는 아직 필요하다.

## English

### Analysis

The `20260914-001047-224` log confirms that the SDL playback device and aggregate mixer
output work. An initial sound effect reached `sdl3:postmix` with a peak of about `0.6`,
but the global post-mix sample limit did not isolate the JAM streaming voice.

### Change

A cooked callback is now attached to each SDL_mixer voice and armed only after streaming
playback starts for that voice. The next log will include records in this form:

```text
sdl3:track-cooked:streaming=1:...:peak=...:rms=...
```

This PCM is measured after the input stream has been converted to the mixer track output
format. A non-zero value means JAM asset loading and track conversion have succeeded; a zero
value keeps the investigation at stream supply or track conversion.

### Verification

- `cmd /c scripts\\build_win32.bat` succeeded.
- `re2dj_ez2dj_keyboard_input_test` passed.
- `re2dj_ez2dancer_keyboard_input_test` passed.
- `re2dj_windows_product_loader_probe` passed.
- `re2dj_unit_tests` passed.
- A new real JAM playback log is still required.
