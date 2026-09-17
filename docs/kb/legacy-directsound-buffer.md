# 구형 DirectSound secondary buffer 계약

## 생성 경계

`IDirectSound::CreateSoundBuffer`는 `DSBUFFERDESC`, 결과 `IDirectSoundBuffer**`, aggregation pointer를 받는다. 성공값은 `DS_OK`이며 descriptor가 buffer 크기, PCM format과 필요한 control capability를 지정한다. Microsoft는 DirectSound를 legacy API로 분류하지만 기존 응용 프로그램 호환 계약은 계속 문서화한다.

- [IDirectSound::CreateSoundBuffer — Microsoft Learn](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/mt708943%28v%3Dvs.85%29)
- [DSBUFFERDESC — Microsoft Learn](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee416820%28v%3Dvs.85%29)

`DSBCAPS_STATIC`은 static sample buffer를 요청하고, `DSBCAPS_LOCHARDWARE`/`DSBCAPS_LOCSOFTWARE`는 mixing 위치를 제한한다. 현대 host가 특정 legacy capability 조합을 제공하지 못할 수 있으므로 HLE는 원본 descriptor와 HRESULT를 먼저 관찰하고, 지원을 가장해 원본 state를 버리지 않아야 한다.

## sample upload

secondary buffer 생성 뒤 일반적인 upload는 `IDirectSoundBuffer::Lock`으로 최대 두 개의 circular 영역을 받고 PCM을 기록한 다음 같은 pointer/byte pair를 `Unlock`에 전달한다.

- [IDirectSoundBuffer::Lock — Microsoft Learn](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/mt708932%28v%3Dvs.85%29)
- [IDirectSoundBuffer::Unlock — Microsoft Learn](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/mt708941%28v%3Dvs.85%29)

`0x80004001`은 일반 COM `E_NOTIMPL`이다. 특정 DirectSound 호출에서 이 값이 관찰됐다는 사실과 그 원인은 별개이며, 원인을 확정하려면 실제 descriptor, interface identity와 host 실행 결과가 필요하다.

- [COM generic error codes — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/com/com-error-codes-1)

## duplicate buffer

`IDirectSound::DuplicateSoundBuffer`는 새 secondary buffer 객체가 원본과 같은 sample memory를 공유하게 한다. 초기 format과 control 값은 같지만 각 객체의 cursor, volume/pan/frequency와 Play/Stop은 이후 독립적이다. 한 객체가 Lock으로 sample을 바꾸면 다른 객체에서도 그 변경이 보여야 한다. primary buffer는 duplicate할 수 없다.

- [IDirectSound::DuplicateSoundBuffer — Microsoft Learn](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/mt708944%28v%3Dvs.85%29)

## streaming 변환 주의점

`Lock/Unlock`의 pointer와 길이는 응용 프로그램이 접근할 수 있었던 범위를 뜻하며 실제로 변경한 byte 범위를 직접 나타내지 않는다. 응용 프로그램은 `DSBLOCK_ENTIREBUFFER`로 전체 ring을 받은 뒤 일부 frame만 고칠 수 있다. HLE가 Unlock 길이 전체를 producer queue에 추가하면 같은 PCM을 중복 큐잉하고 latency가 계속 증가한다.

복사형 host streaming API로 변환할 때 Unlock 범위 전체를 queue에 복사하면 초기 ring을
반복할 수 있다. 현재 HLE는 committed snapshot과 현재 frame을 비교해 실제로 달라진
원형 구간만 queue에 추가한다. 소비량만으로 아직 guest가 갱신하지 않은 ring 시작점을
다시 넣으면 작업 284에서 확인한 것처럼 1~2초 반복이 발생한다. 동일한 PCM을 guest가
새로 쓴 경우는 pointer 범위만으로 구분할 수 없으므로 별도의 명시적 write 범위나 pull
동기화가 필요하다. SDL3의 `SDL_AudioStream`과 SDL_mixer의 `MIX_SetTrackAudioStream`은
응용 프로그램이 공급하는 streaming PCM 입력을 위한 계약이다.

- [SDL_AudioStream — SDL Wiki](https://wiki.libsdl.org/SDL3/SDL_AudioStream)
- [MIX_SetTrackAudioStream — SDL_mixer Wiki](https://wiki.libsdl.org/SDL3_mixer/MIX_SetTrackAudioStream)

## 반복 재생과 `DSBCAPS_STATIC`

`IDirectSoundBuffer::Play`의 `DSBPLAY_LOOPING`은 버퍼 끝에 도달하면 처음부터 다시 재생하고 명시적으로 멈출 때까지 계속하라는 뜻이다. 이 플래그 없이 재생한 버퍼는 끝에 도달하면 스스로 멈춘다. 생성 플래그와 무관한 재생 계약이므로, HLE가 링 순환 경로로 재생하는 버퍼라도 반복 없이 재생됐다면 한 바퀴 뒤 멈춰야 한다.

- [IDirectSoundBuffer::Play — Microsoft Learn](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/mt708933%28v%3Dvs.85%29)
- [Filling and Playing Static Buffers — Microsoft Learn](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417553%28v%3Dvs.85%29)

`DSBCAPS_STATIC`은 "버퍼가 사운드 카드 메모리에 있다"는 배치 요청일 뿐이다. Microsoft는 "static buffer"(한 번 채워 재생하는 버퍼)가 반드시 이 플래그로 만든 버퍼는 아니라고 따로 적는다. 반대로 `DSBCAPS_GETCURRENTPOSITION2`는 에뮬레이션 장치에서 재생 커서를 더 정확히 돌려받겠다는 요청이며, 스트리밍 여부를 뜻하지 않는다.

- [DSBCAPS — Microsoft Learn](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee416818%28v%3Dvs.85%29)

따라서 생성 플래그만으로 링 버퍼와 일회성 버퍼를 가르는 규칙은 API 계약이 아니라 **관찰에 맞춘 휴리스틱**이다. 작업 303에서 확인한 빌드별 조합은 다음과 같다.

| 빌드 | 링 버퍼 (360,448 바이트, 반복 재생) | 일회성 효과음 (반복 없음) |
| --- | --- | --- |
| 1st Tracks | `0x140c6` (LOCHARDWARE·STATIC·GETPOS2 포함) | `0x140c2` (STATIC·GETPOS2 포함) |
| 1st SE | `0x140c6` | `0x140e2` (STATIC·GETPOS2 포함) |
| 2nd, 3rd, 4th, 5th | `0x140c0` (GETPOS2 포함) | `0x40e0` (둘 다 없음) |
| EZ2Dancer 2nd MOVE | `0x140c0` | `0x40c0` |

HLE는 크기 360,448을 먼저 링으로 보고, 그 밖에는 `LOCHARDWARE` 또는 `GETCURRENTPOSITION2`가 있으면서 `STATIC`이 없을 때만 링으로 본다. 규칙이 다시 어긋나더라도 반복 없이 재생된 스트리밍 버퍼는 한 바퀴 뒤 멈추므로 끝없는 반복으로 번지지 않는다.

---

# Legacy DirectSound Secondary-Buffer Contract

IDirectSound::CreateSoundBuffer accepts a DSBUFFERDESC, an output IDirectSoundBuffer pointer, and an aggregation pointer. The descriptor carries buffer size, PCM format, and requested control capabilities. A typical upload locks up to two circular regions, writes PCM samples, and unlocks the same pointer/byte pairs. DirectSound is documented as legacy, so an HLE should observe the original descriptor and HRESULT before translating the contract to a modern backend.

Generic COM value 0x80004001 is E_NOTIMPL. Observing it from a DirectSound call does not by itself establish why that particular host path rejected the request; interface identity, descriptor values, and repeatable runtime evidence are still required.

IDirectSound::DuplicateSoundBuffer creates a separate secondary-buffer object sharing the original sample memory. Initial parameters match, but cursor, controls, and Play/Stop state can diverge independently. Writes through either object's Lock are visible through the other; primary buffers cannot be duplicated.

Lock/Unlock pointer-length pairs describe accessible regions, not necessarily changed bytes.
Appending the entire lock range can replay the initial ring. The current HLE compares a
committed snapshot and appends only changed circular ranges. Task 284 shows that refilling
from consumption alone replays the initial segment every 1–2 seconds when the guest has not
refreshed that range. Identical PCM writes need an explicit write range or synchronized pull
contract. SDL3 `SDL_AudioStream` connected through `MIX_SetTrackAudioStream` supplies the
streaming PCM input.

`DSBPLAY_LOOPING` on `IDirectSoundBuffer::Play` restarts the buffer from its beginning at the
end and keeps playing until explicitly stopped; a buffer played without it stops by itself at
the end. That is a play-time contract independent of creation flags, so a buffer the HLE plays
through its ring-cycling path must still stop after one pass when played without looping.
`DSBCAPS_STATIC` only requests on-board sound card memory, and Microsoft notes that a "static
buffer" (filled once, then played) is not necessarily one created with that flag, while
`DSBCAPS_GETCURRENTPOSITION2` only asks for a more accurate play cursor on emulated devices.
Telling rings from one-shot buffers by creation flags is therefore a **heuristic fitted to
observation**, not an API contract. In task 303, 1st Tracks and 1st SE rings were `0x140c6`
with effects `0x140c2` and `0x140e2` (both STATIC with GETCURRENTPOSITION2); 2nd through 5th
rings `0x140c0` with effects `0x40e0`; EZ2Dancer 2nd MOVE rings `0x140c0` with effects
`0x40c0`. The HLE treats the 360,448-byte size as a ring first, and otherwise only a buffer with
`LOCHARDWARE` or `GETCURRENTPOSITION2` and without `STATIC`. Should that rule drift again, a
non-looping streaming play still stops after one pass instead of repeating forever.
[IDirectSoundBuffer::Play](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/mt708933%28v%3Dvs.85%29),
[Filling and Playing Static Buffers](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417553%28v%3Dvs.85%29),
[DSBCAPS](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee416818%28v%3Dvs.85%29).
