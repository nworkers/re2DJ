# SDL3_mixer raw PCM backend

SDL_mixer 3의 `MIX_LoadRawAudio`는 caller가 제공한 PCM과 `SDL_AudioSpec`을 복사해 `MIX_Audio`를 만든다. `MIX_SetTrackAudio`로 이를 `MIX_Track`에 연결하고 `MIX_PlayTrack`의 property로 시작 frame과 loop를 지정할 수 있다. gain, stereo gain, frequency ratio와 playback position API는 DirectSound buffer control을 backend 상태로 옮기는 데 사용한다.

SDL 3.4.14와 SDL_mixer 3.2.4는 동일한 zlib license를 사용한다. 이 프로젝트의 raw PCM 경로에는 별도 codec이 필요하지 않으므로 SDL_mixer의 선택적 codec integrations는 비활성화한다.

- [SDL 3.4.14](https://github.com/libsdl-org/SDL/tree/release-3.4.14)
- [SDL_mixer 3.2.4](https://github.com/libsdl-org/SDL_mixer/tree/release-3.2.4)
- [SDL_mixer API header](https://github.com/libsdl-org/SDL_mixer/blob/release-3.2.4/include/SDL3_mixer/SDL_mixer.h)

---

# SDL3_mixer Raw PCM Backend

SDL_mixer 3 copies caller-provided PCM and an SDL_AudioSpec into MIX_Audio through MIX_LoadRawAudio. MIX_SetTrackAudio attaches it to a MIX_Track, while MIX_PlayTrack properties control start frames and looping. Gain, stereo gains, frequency ratio, and playback-position APIs map the corresponding DirectSound buffer state without exposing SDL types to the shared core.

SDL 3.4.14 and SDL_mixer 3.2.4 use the zlib license. Optional codec integrations remain disabled because the guest already supplies decoded raw PCM.
