# 작업 로그: EZ2Dancer SDL 오디오 출력 경계 진단
# Work Log: EZ2Dancer SDL Audio Output Boundary Diagnosis

## 한국어

### 수행 내용

JAM complete trace와 현재 SDL3_mixer backend 및 pinned SDL_mixer source를 대조했습니다.
이번 단계에서는 코드 동작을 변경하지 않고 원인 후보만 확정했습니다.

### 확인 결과

1. "jam.ezw"의 CHD open/read 성공과 DirectSound streaming buffer 공급은 이전 로그에서
   이미 확인되었습니다.
2. Sdl3MixerAudioBackend::Play()는 streaming track에
   MIX_PROP_PLAY_HALT_WHEN_EXHAUSTED_BOOLEAN=false를 전달하지 않습니다. SDL_mixer
   header의 기본값은 true이며, stream queue가 고갈되면 track을 stopped로 만들 수
   있습니다.
3. backend 생성자는 MIX_CreateMixerDevice() 실패 뒤 MIX_CreateMixer()로 fallback합니다.
   후자는 실제 device가 없는 memory-only mixer입니다. 현재 DirectSoundCreate는
   has_playback_device_가 false여도 진단 메시지만 출력하고 성공을 반환합니다.
4. 따라서 현재 audio.log의 queue, PCM peak/RMS, DirectSound play 성공만으로 실제
   스피커 출력까지 확인할 수 없습니다.
5. re2dj_windows_vfs_runtime_probe CTest는 45초 이상 결과를 내지 않아 중단했습니다.
   이는 별도의 probe/lifecycle 문제 가능성을 보여주지만, JAM playback 원인으로
   확정하지 않았습니다.

### 결론

현재 가장 우선순위가 높은 다음 작업은 output device 상태와 SDL_mixer post-mix callback을
bounded trace로 추가하는 것입니다. 동시에 streaming track에는 exhaustion false 정책이
필요합니다. 이번 단계에서는 코드를 수정하지 않았습니다.

### 문서

- 기존 JAM 진단 설계에 SDL 출력 경계 후보를 추가했습니다.
- SDL3_mixer raw audio 지식 문서에 streaming exhaustion과 memory-only mixer 계약을
  추가했습니다.
- 후속 구현 작업 지시서를 추가했습니다.

### 검증

- git diff --check를 문서 변경 후 실행합니다.
- 코드 빌드는 코드 변경이 없어 수행하지 않습니다.

## English

### Work performed

Compared the JAM complete trace with the current SDL3_mixer backend and the pinned
SDL_mixer source. This stage confirms causes and candidates without changing runtime code.

### Findings

1. Successful CHD open/read for "jam.ezw" and delivery into the DirectSound streaming
   buffer were already confirmed by the previous run.
2. Sdl3MixerAudioBackend::Play() does not set
   MIX_PROP_PLAY_HALT_WHEN_EXHAUSTED_BOOLEAN=false for streaming tracks. The SDL_mixer
   header defaults this property to true, allowing an exhausted stream queue to stop the
   track.
3. The backend falls back from MIX_CreateMixerDevice() to MIX_CreateMixer(). The latter is
   a memory-only mixer with no physical device. DirectSoundCreate currently returns success
   after only emitting a diagnostic message when has_playback_device_ is false.
4. Consequently, queue counts, PCM peak/RMS, and successful DirectSound Play calls in the
   current audio log do not prove speaker output.
5. The re2dj_windows_vfs_runtime_probe CTest produced no result for more than 45 seconds and
   was interrupted. This may indicate a separate probe/lifecycle issue, but it was not
   attributed to the JAM playback cause.

### Conclusion

The highest-priority next task is to add bounded traces for playback-device state and the
SDL_mixer post-mix callback. The streaming track also needs the exhaustion-false policy.
No runtime code was changed in this stage.

### Documentation

- Added the SDL output-boundary candidates to the existing JAM diagnosis design.
- Added the streaming-exhaustion and memory-only-mixer contracts to the SDL3_mixer raw-audio
  knowledge document.
- Added a follow-up implementation work order.

### Verification

- Run git diff --check after the documentation changes.
- No code build was run because no code was changed.
