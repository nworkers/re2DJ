# 무음 스트리밍 진행 / Streaming silence progress

동일한 PCM도 재생 시간을 차지합니다. 기존 changed-frame 비교는 같은 무음이 이어지면
queue 공급을 생략하여 mixer cursor와 guest의 다음 read를 함께 정지시킬 수 있습니다.
`011631-574`에서 track reset 적용 후에도 무음이 유지되었습니다. 이전 track 수명 수정은
JAM 해결로 확인된 것이 아닙니다.

Identical PCM still occupies playback time. Changed-frame comparison can omit repeated
silence, exhausting the queue and stalling both the mixer cursor and the guest's next read.
Run `011631-574` remains silent with track reset applied; that change did not resolve JAM.

Unlock에서 mixer가 소비한 source frame 수를 읽고, 최초 ring 이후 이미 공급한 바이트를
제외한 소비 분량을 원형 버퍼 순서대로 보충합니다. 값 비교를 제거합니다. 공급량은 한 번에
ring 크기까지 제한하며 성공한 공급만 누적합니다. 정지/재시작은 누적량을 초기화합니다.
기존 전체 ring 선공급 지연은 유지하며 callback에서 guest memory를 직접 읽지 않습니다.

On Unlock, read consumed source frames and replenish the consumed portion not yet
resubmitted, in circular-buffer order. Remove value comparison. Limit each submission to
one ring and account only successful submissions. Reset accounting on restart. Preserve
the existing full-ring prequeue latency; callbacks do not read guest memory directly.

합성 무음을 여러 ring 동안 재생하고 이후 tone으로 교체하여 cursor가 초기 ring을 넘어
진행하며 cooked PCM이 유음이 되는지 standalone dummy-audio 테스트로 검증합니다.

Validate with a standalone dummy-audio test: play identical silence for several rings,
replace it with a tone, and require both continued cursor progress and audible cooked PCM.
