# SDL3_mixer raw PCM backend

SDL_mixer 3의 `MIX_LoadRawAudio`는 caller가 제공한 PCM과 `SDL_AudioSpec`을 복사해 `MIX_Audio`를 만든다. `MIX_SetTrackAudio`로 이를 `MIX_Track`에 연결하고 `MIX_PlayTrack`의 property로 시작 frame과 loop를 지정할 수 있다. gain, stereo gain, frequency ratio와 playback position API는 DirectSound buffer control을 backend 상태로 옮기는 데 사용한다.

SDL 3.4.14와 SDL_mixer 3.2.4는 동일한 zlib license를 사용한다. 이 프로젝트의 raw PCM 경로에는 별도 codec이 필요하지 않으므로 SDL_mixer의 선택적 codec integrations는 비활성화한다.

`MIX_SetTrackGain`은 개별 sound의 DirectSound dB를 linear gain으로 옮기고, `MIX_SetMixerGain`은 모든 track 합성 뒤의 독립 master 보정에 사용한다. gain `1.0`은 변화 없음이고 1보다 큰 값은 증폭한다. master gain은 track별 상대 음량을 보존하지만 최종 출력 clipping 가능성이 있으므로 제한된 dB 범위와 사용자 청취 검증이 필요하다.

- [SDL 3.4.14](https://github.com/libsdl-org/SDL/tree/release-3.4.14)
- [SDL_mixer 3.2.4](https://github.com/libsdl-org/SDL_mixer/tree/release-3.2.4)
- [SDL_mixer API header](https://github.com/libsdl-org/SDL_mixer/blob/release-3.2.4/include/SDL3_mixer/SDL_mixer.h)

---

# SDL3_mixer Raw PCM Backend

SDL_mixer 3 copies caller-provided PCM and an SDL_AudioSpec into MIX_Audio through MIX_LoadRawAudio. MIX_SetTrackAudio attaches it to a MIX_Track, while MIX_PlayTrack properties control start frames and looping. Gain, stereo gains, frequency ratio, and playback-position APIs map the corresponding DirectSound buffer state without exposing SDL types to the shared core.

SDL 3.4.14 and SDL_mixer 3.2.4 use the zlib license. Optional codec integrations remain disabled because the guest already supplies decoded raw PCM.

`MIX_SetTrackGain` maps each sound's DirectSound dB value to linear gain, while `MIX_SetMixerGain` provides independent compensation after all tracks are mixed. Gain `1.0` is unchanged and values above one amplify. Master gain preserves relative track levels but can clip the final output, so it needs a bounded dB range and listening validation.
## streaming input exhaustion

SDL_mixer track에 SDL_AudioStream을 입력으로 연결할 때
MIX_PROP_PLAY_HALT_WHEN_EXHAUSTED_BOOLEAN의 기본값은 true입니다. 실시간으로
SDL_PutAudioStreamData를 공급하는 track은 이 값을 false로 설정해야 queue가 일시적으로
비어도 track이 stopped 상태로 전환되지 않습니다. false이면 해당 순간에는 silence가
출력되고 이후 새 데이터가 들어오면 같은 track이 계속 소비합니다.

MIX_CreateMixer()는 실제 playback device가 없는 memory-only mixer를 생성합니다.
이 mixer는 MIX_Generate()를 호출해야 output이 생성되므로, 제품이 device mixer 생성에
실패한 뒤 이 경로로 fallback하면 queue가 정상이어도 스피커로 출력되지 않습니다.

## English

## Streaming input exhaustion

When an SDL_AudioStream is assigned as a SDL_mixer track input, the default value of
MIX_PROP_PLAY_HALT_WHEN_EXHAUSTED_BOOLEAN is true. A track fed incrementally through
SDL_PutAudioStreamData should set this property to false so a temporary empty queue does not
transition the track to stopped. With false, that interval contributes silence and later
input continues on the same track.

MIX_CreateMixer() creates a memory-only mixer without a playback device. It requires
MIX_Generate() to produce output, so a product that falls back to this path after device
mixer creation fails will not reach speakers even if its queue is populated.
