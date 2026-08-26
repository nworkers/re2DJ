# DirectSound SDL3/SDL3_mixer HLE 작업 지시

관련 설계: [DirectSound SDL3/SDL3_mixer HLE](../design/20260826-069-directsound-sdl3-mixer-hle.md)

## 상태

**완료.** SDL3/SDL3_mixer raw PCM backend, 공용 buffer 모델, Windows COM facade, ordinal IAT 교체와 probe를 구현했다. 원본 실행은 DirectSound 초기화와 buffer upload/play를 통과하고 다음 `IDirect3D3::CreateVertexBuffer` AV에 도달했다.

## 작업

1. SDL 3.4.14와 SDL_mixer 3.2.4 raw PCM 전용 의존성을 CMake에 고정한다.
2. 플랫폼 공용 legacy audio buffer와 circular lock/state test를 추가한다.
3. Windows x86 DirectSound/DirectSoundBuffer COM facade와 SDL3_mixer backend를 분리 구현한다.
4. DSOUND ordinal #1 IAT 교체 옵션과 검증을 launcher에 연결한다.
5. Windows x86 build, CTest와 두 번의 canonical 실행에서 access violation과 다음 경계를 확인한다.
6. ARCHITECTURE, TODO, analysis, IMPLEMENTED와 작업 로그를 결과에 맞춰 갱신한다.

---

# DirectSound SDL3/SDL3_mixer HLE Work Order

Related design: [DirectSound SDL3/SDL3_mixer HLE](../design/20260826-069-directsound-sdl3-mixer-hle.md)

**Complete.** SDL3/SDL3_mixer raw PCM, the neutral buffer model, Windows COM facade, ordinal IAT replacement, and probe are implemented. The original executable passes DirectSound initialization, upload, and playback before reaching the next IDirect3D3::CreateVertexBuffer AV.
