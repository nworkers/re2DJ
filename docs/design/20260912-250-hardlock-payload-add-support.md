# 설계: Hardlock 요청 단위 payload 가산(add) 연산 지원

## 한국어

### 배경

Hardlock `API_CODE` (Function `0x0011`) 변환 요청은 7개 블록(56바이트)의 버퍼를 사용합니다. reSoftlock 분석 및 Hardlock 에뮬레이터 동작 관찰 결과, `API_CODE`는 단순히 출력 블록을 덮어쓰는(write) 것뿐만 아니라, 특정 블록(블록 3)에 대해 게스트 버퍼의 32비트 DWORD들에 계산된 값을 더하는(add with carry) 동작을 수행합니다.

기존 `HardlockPayloadResponseEntry` 구조는 각 바이트에 대해 덮어쓰기(`output`, `output_mask`)만 지원하였으므로, 게스트 버퍼에 값을 가산하는 동작을 표현할 수 없었습니다.

### 목표

1. `HardlockPayloadResponseEntry`에 32비트 little-endian 가산 필드(`output_add`, `output_add_mask`)를 도입합니다.
2. 가산 그룹은 4바이트 단위(DWORD)로 완전히 지정되어야 하며, 동일 바이트가 쓰기와 가산에 동시에 지정되지 않도록 검증(`ValidateHardlockPayloadResponse`)합니다.
3. `ApplyHardlockPayloadResponse`에서 쓰기 필드를 먼저/함께 적용하고, 지정된 가산 그룹에 32비트 little-endian 가산을 적용합니다.
4. 프로세스 주입 런타임 간 공유 레코드(`kHardlockPayloadRecordSize`)에 가산 필드를 포함하도록 크기를 확장(`4 + kHardlockPayloadMaxBytes * 6`)하고 패킹/언패킹을 구현합니다.

### 자료구조 및 ABI

```text
Record Layout:
+-------------------+--------------------+--------------------+--------------------+--------------------+--------------------+--------------------+
| block_count (u32) | input (max_bytes)  | input_mask         | output             | output_mask        | output_add         | output_add_mask    |
+-------------------+--------------------+--------------------+--------------------+--------------------+--------------------+--------------------+
```

---

## English

### Background

Hardlock `API_CODE` (Function `0x0011`) transform requests use a 7-block (56-byte) buffer. Analysis from reSoftlock and observation of Hardlock emulator behavior indicate that `API_CODE` not only overwrites output blocks, but also performs a 32-bit DWORD addition (with carry) on specific blocks (such as block 3) relative to the guest's input buffer.

The existing `HardlockPayloadResponseEntry` structure only supported byte overwrites (`output`, `output_mask`), and could not express additions onto the guest buffer.

### Goals

1. Introduce 32-bit little-endian addition fields (`output_add`, `output_add_mask`) to `HardlockPayloadResponseEntry`.
2. Validate that add groups are fully specified in 4-byte (DWORD) units and that no byte is both written and added simultaneously (`ValidateHardlockPayloadResponse`).
3. In `ApplyHardlockPayloadResponse`, apply overwrites and perform 32-bit little-endian addition on specified add groups.
4. Expand the inter-process injected runtime shared record size (`kHardlockPayloadRecordSize = 4 + kHardlockPayloadMaxBytes * 6`) to accommodate the add fields and update packing/unpacking routines.
