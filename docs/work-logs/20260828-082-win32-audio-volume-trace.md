# Win32 오디오 음량 추적 작업 로그

## 한국어

### 결과

`--audio-volume-trace`를 제품 CLI부터 injected runtime까지 연결했다. DirectSound facade는 buffer 생성 형식, `SetVolume`의 1/100 dB와 linear gain, 최초 재생 및 재생 중 bounded `Unlock` PCM peak/RMS를 기록한다. 원본의 일곱 WINMM mixer import는 호스트 API로 pass-through하면서 line/control 구조와 scalar 값을 기록한다. 로그는 최대 4,096행이며 원본 샘플 바이트를 포함하지 않는다.

### 실제 실행 증거

사용자 제공 HDD 경로를 `--audio-gain-db 0 --audio-volume-trace`로 세 번 실행했고 최종 증거는 `20260828-081711-510.audio.log`다. 증거 채집 뒤 시작한 프로세스는 명시적인 PID 범위로 종료했다.

- WINMM speaker volume control ID 2, 범위 0–65,535, write 57,194가 성공했다.
- `title.wav` streaming buffer는 360,448바이트, 44.1 kHz stereo 16-bit였다.
- 첫 buffer snapshot의 peak/RMS는 WAV 첫 data 청크와 정확히 일치했다.
- 첫 청크 RMS `-22.44 dBFS`와 WAV 전체 RMS `-9.29 dBFS`의 차이는 `13.14 dB`다.
- 원본은 재생 뒤 ring buffer를 계속 갱신하여 peak `0.9793`, RMS `0.2608`까지 올라갔지만 SDL backend는 갱신하지 않았다.

따라서 체감 `+12 dB` 차이의 주원인은 WAV decode 감쇠가 아니라 조용한 첫 streaming 청크만 snapshot으로 반복하는 현재 backend 구조다. 다음 작업은 master gain 상향이 아니라 streaming/ring-buffer 동기화다.

### 검증

- Windows x86 Debug 전체 빌드 통과
- CTest 3/3 통과: runtime probe, product loader probe, unit tests
- runtime probe에서 master gain, `SetVolume`, peak/RMS trace 필드 검증
- 실제 원본 실행에서 WINMM 값과 연속 streaming `Unlock` 관찰

### 통합

TODO의 다음 우선순위를 작업 083 DirectSound streaming/ring-buffer 동기화로 갱신했다. 작업 082는 patch 버전 `0.0.12`로 `main`에 squash 통합한다.

## English

### Result

Connected `--audio-volume-trace` from the product CLI through the injected runtime. The DirectSound facade records buffer formats, hundredth-decibel and linear `SetVolume` values, and bounded PCM peak/RMS at first playback and streaming `Unlock`. Seven original WINMM mixer imports pass through to host APIs while recording line/control structures and scalar values. Logs are limited to 4,096 lines and never include original sample bytes.

### Real-run evidence

Ran the user-supplied HDD three times with `--audio-gain-db 0 --audio-volume-trace`; the final evidence is `20260828-081711-510.audio.log`. Processes started for evidence collection were stopped by explicit PID after capture.

- A write of 57,194 to WINMM speaker volume control ID 2, range 0–65,535, succeeded.
- The `title.wav` streaming buffer was 360,448 bytes, 44.1 kHz stereo 16-bit.
- The first buffer snapshot's peak/RMS exactly matched the WAV's first data chunk.
- The first chunk at -22.44 dBFS RMS differs from the complete WAV at -9.29 dBFS RMS by 13.14 dB.
- The original continued refreshing the ring buffer after playback, reaching peak 0.9793 and RMS 0.2608, but the SDL backend did not refresh its snapshot.

The perceived +12 dB difference is therefore mainly caused by repeating only the quiet first streaming chunk, not by WAV decoding attenuation. The next task is streaming/ring-buffer synchronization rather than a larger master gain.

### Verification

- Windows x86 Debug full build passed.
- CTest passed 3/3: runtime probe, product loader probe, and unit tests.
- Runtime probe verified master gain, `SetVolume`, and peak/RMS trace fields.
- A real original-executable run observed WINMM values and continuous streaming `Unlock` updates.

### Integration

Updated the next TODO priority to Task 083 DirectSound streaming/ring-buffer synchronization. Task 082 is squash-integrated into `main` as patch version `0.0.12`.
