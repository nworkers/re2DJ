# DirectSound Play 상태 계약

## 한국어

`IDirectSoundBuffer::Play`는 buffer가 이미 재생 중이어도 성공하며 현재 playback
position에서 계속 재생합니다. 최신 호출의 flags는 이전 flags를 대체하지만, 호출 자체가
host queue를 삭제하거나 cursor를 0으로 되돌리는 의미는 아닙니다.

streaming HLE는 이 계약을 보존하기 위해 재생 중인 buffer의 일반 `Play`에서 controls만
갱신하고 producer queue와 cursor를 유지해야 합니다. 반대로 `SetCurrentPosition`처럼
명시적으로 위치를 변경하는 API는 현재 track을 정지하고 새 cursor에서 다시 공급하는
별도 경로가 필요합니다.

- [IDirectSoundBuffer::Play — Microsoft Learn](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/mt708933%28v%3Dvs.85%29)
- [IDirectSoundBuffer::Stop — Microsoft Learn](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/mt708940%28v%3Dvs.85%29)

## English

`IDirectSoundBuffer::Play` succeeds when the buffer is already playing and continues from
the current playback position. The latest call's flags supersede the previous flags, but the
call does not mean that a host queue should be deleted or that the cursor should return to
zero.

A streaming HLE should therefore update controls only for an ordinary `Play` on an already
playing buffer, preserving the producer queue and cursor. An API such as
`SetCurrentPosition` that explicitly changes the position needs a separate path that stops
the current track and refills it from the new cursor.

- [IDirectSoundBuffer::Play — Microsoft Learn](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/mt708933%28v%3Dvs.85%29)
- [IDirectSoundBuffer::Stop — Microsoft Learn](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/mt708940%28v%3Dvs.85%29)
