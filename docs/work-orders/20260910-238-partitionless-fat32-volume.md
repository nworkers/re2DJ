# 작업 지시서: 파티션 테이블 없는 FAT32 볼륨 지원

## 한국어

### 관련 설계

[파티션 테이블 없는 FAT32 볼륨 지원 설계](../design/20260910-238-partitionless-fat32-volume.md)

### 작업 항목

1. BPB 해석과 검증을 helper로 뺍니다. 인자는 boot sector 바이트, 볼륨 시작 LBA, 볼륨 섹터 수입니다.
2. 파티션 항목에서 FAT32를 찾으면 그 위치로, 없거나 실패하면 LBA 0과 이미지 전체로 helper를 부릅니다.
3. 둘 다 실패하면 두 시도를 모두 언급하는 메시지로 거절합니다.
4. `Fat32VolumeInfo`에 파티션에서 온 볼륨인지 여부를 남기고 `chd_probe`가 보고하게 합니다.
5. 두 배치를 덮는 단위 시험을 추가합니다.
6. Windows x86 build와 시험을 검증합니다.
7. `ez2dj5th` CHD가 마운트되고 파일이 열리는지 확인합니다.
8. 파티션이 있는 이미지에 회귀가 없는지 확인합니다.
9. 작업 로그를 작성합니다.

### 제외 범위

- FAT12·FAT16·exFAT
- 확장 파티션과 GPT
- 파티션 선택 정책 변경
- 쓰기 지원

### 완료 조건

- `ez2dj5th`의 CHD가 마운트되고 내부 파일이 열립니다.
- 3rd·4th·6th·1st SE의 마운트 결과가 변하지 않습니다.
- 단위 시험이 파티션 볼륨과 whole-disk 볼륨을 모두 덮고 통과합니다.

## English

### Related design

[Partitionless FAT32 Volume Design](../design/20260910-238-partitionless-fat32-volume.md)

### Work items

1. Extract the BPB parse and validation into a helper taking the boot sector's bytes, the volume's start LBA and its sector count.
2. Call it at the FAT32 partition entry when there is one, and at LBA 0 over the whole image when there is none or that attempt fails.
3. Reject with a message naming both attempts when both fail.
4. Record in `Fat32VolumeInfo` whether the volume came from a partition, and report it from `chd_probe`.
5. Add unit tests covering both layouts.
6. Verify the Windows x86 build and tests.
7. Confirm the `ez2dj5th` CHD mounts and its files open.
8. Confirm no regression for partitioned images.
9. Write the work log.

### Out of scope

- FAT12, FAT16 and exFAT
- Extended partitions and GPT
- Any change to which partition is chosen
- Write support

### Completion criteria

- The `ez2dj5th` CHD mounts and files inside it open.
- Mounting 3rd, 4th, 6th and 1st SE is unchanged.
- Unit tests cover both the partitioned and whole-disk layouts and pass.
