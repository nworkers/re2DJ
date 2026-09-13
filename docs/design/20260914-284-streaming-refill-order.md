# 스트리밍 보충 순서 수정 / Streaming refill order correction

## 한국어

### 문제

작업 283의 소비량 기반 보충은 `consumed - replenished_bytes`를 계산하면서 guest ring의
다음 구간을 무조건 SDL queue에 넣었습니다. 초기 ring이 아직 갱신되지 않은 상태에서도
ring 시작점부터 다시 넣기 때문에, 약 360448 bytes를 소비할 때마다 첫 구간이 반복됩니다.
로그 `011631-574`의 `track-reset=1` 뒤 1~2초 반복은 이 계산과 일치합니다.

### 설계

초기 `Play`에서 공급한 ring을 committed snapshot으로 저장하고, 이후 `Unlock`에서는
현재 guest ring과 snapshot이 실제로 달라진 원형 구간만 queue에 추가합니다. 변경이 없는
구간을 소비량만으로 재공급하지 않습니다. 그러면 아직 guest가 다음 파일 chunk를 쓰지
않은 경우 이전 ring을 반복하지 않고, guest가 offset 0부터 새 PCM을 기록한 순간 해당
구간만 다음 queue 데이터가 됩니다.

재시작 때 snapshot을 새 buffer 상태로 초기화합니다. 이미 재생 중인 반복 `Play`의
continuation과 stopped track 재생성은 유지합니다. `Unlock`의 전체 ring pointer 길이는
변경 범위를 뜻하지 않으므로 전체를 queue에 복사하지 않습니다.

```mermaid
sequenceDiagram
    participant G as Guest ring
    participant H as HLE snapshot
    participant Q as SDL queue
    G->>Q: Initial Play: whole ring
    G->>H: Unlock: current ring
    H->>H: Compare with committed snapshot
    alt changed circular range
        H->>Q: Append only changed range
        H->>H: Commit new snapshot
    else unchanged range
        H-->>Q: No append; preserve order
    end
```

### 검증

합성 테스트는 tone ring을 초기 공급한 뒤 변경 없는 Unlock에서 queue가 ring 크기를
넘지 않는지 확인하고, ring 시작점을 새 tone으로 덮은 뒤 해당 PCM이 출력되는지 확인합니다.
기존 streaming start/continue, Stop, cursor 동기화와 Windows x86 빌드를 함께 검증합니다.

## English

### Problem

Task 283's consumption-based refill unconditionally appended the next guest-ring range
using `consumed - replenished_bytes`. It did so even before the guest refreshed that range,
so the initial ring was replayed every time approximately 360448 bytes were consumed. The
1–2 second repetition after `track-reset=1` in run `011631-574` matches this calculation.

### Design

Store the whole ring supplied by the initial `Play` as the committed snapshot. On later
`Unlock` calls, append only circular ranges whose current guest-ring bytes differ from that
snapshot. Do not refill an unchanged range based only on elapsed consumption. This prevents
replaying data before the guest writes the next file chunk, while a guest write at offset 0
becomes the next queued PCM range.

Reset the snapshot on restart. Preserve repeated-Play continuation and stopped-track
recreation. The full-ring pointer length returned by `Unlock` does not identify the bytes
that changed, so the whole region must not be copied to the queue.

### Verification

The synthetic test supplies a tone ring, performs unchanged Unlock calls, and requires the
queue to stay within one ring. It then overwrites the ring start with a second tone and
requires that PCM to reach output. Existing streaming start/continue, Stop, cursor
synchronization, and the Windows x86 build are also checked.
