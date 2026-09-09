# 작업 로그: 파티션 테이블 없는 FAT32 볼륨 지원

## 한국어

### 관련 문서

- 설계: [파티션 테이블 없는 FAT32 볼륨 지원 설계](../design/20260910-238-partitionless-fat32-volume.md)
- 작업 지시: [파티션 테이블 없는 FAT32 볼륨 지원](../work-orders/20260910-238-partitionless-fat32-volume.md)
- 선행 작업: [ez2dj1st·ez2dj5th Hardlock descriptor 확보](20260909-237-ez2dj1st-5th-hardlock-descriptors.md)

### 해결한 문제

`re2dj ez2dj5th`가 `CHD MBR has no in-range FAT32 partition`으로 실패해 이 target을 CHD로 실행할 수 없었습니다.

### 원인

공급된 `ez2dj5.chd`의 LBA 0이 파티션 부트 레코드가 아니라 FAT32 boot sector 자체입니다. 파티션 테이블 없는 whole-disk 볼륨입니다.

```
lba0_prefix=eb 58 90 4d 53 57 49 4e 34 2e 31 00 02 20 24 00 signature=55aa
```

`eb 58 90` short jump, OEM `MSWIN4.1`, 512 bytes per sector, 32 sectors per cluster, 36 reserved sectors입니다. `55aa`가 있어 MBR 서명 검사는 통과하지만 파티션 항목 자리에 FAT32 형식이 없어 그 다음 검사에서 거절되었습니다.

**확인됨.**

### 코드 변경

| 항목 | 내용 |
| --- | --- |
| `ParseFat32BootSector` | BPB 해석과 검증을 자유 함수로 분리. 인자는 boot sector, 볼륨 시작 LBA, 볼륨 섹터 수 |
| `Fat32Volume::Open` | 파티션 항목에서 FAT32를 찾으면 그 위치로, 없거나 실패하면 LBA 0과 이미지 전체로 다시 시도 |
| `Fat32VolumeInfo::partitioned` | 볼륨이 파티션에서 왔는지. `partition_index`만으로는 첫 파티션과 구분되지 않음 |
| `chd_probe` | `partitioned=`를 보고 |

받아들임의 기준은 기존 BPB 검증 그 자체입니다. 별도의 형식 추정을 두지 않았습니다. 검증이 sector 크기, 클러스터 크기의 2의 거듭제곱 여부, 예약 섹터, FAT 개수, 총 섹터 수가 볼륨 안에 들어오는지, FAT이 선언된 데이터 영역을 덮는지를 이미 확인하므로 MBR이 우연히 통과할 수 없습니다.

파티션 순회도 바뀌었습니다. 이전에는 첫 FAT32 항목을 찾고 그 하나만 시도한 뒤 실패하면 끝이었습니다. 지금은 항목 4개를 돌면서 범위를 벗어나거나 boot sector가 아닌 것은 건너뜁니다.

### 검증 — 결과

`ez2dj5th` CHD가 마운트됩니다.

```
filesystem=fat32 partitioned=0 partition=0 partition_lba=0 partition_sectors=40708647
bytes_per_sector=512 sectors_per_cluster=32 reserved_sectors=36 fat_count=2
sectors_per_fat=9934 root_cluster=2 data_lba=19904 cluster_count=1271523
label=EZ2DJ5 type=FAT32
filesystem_executable=EZ2DJ/EZ2DJ.EXE first_cluster=403322 size=1388544
filesystem_pe=machine=i386 magic=PE32 entry_rva=0x0070d240 sections=6
```

CHD 안의 실행 파일이 entry RVA `0x0070d240`, section 6개로 [작업 237](20260909-237-ez2dj1st-5th-hardlock-descriptors.md)에서 추출본으로 관측한 값과 일치합니다.

`re2dj ez2dj5th`가 CHD에서 end-to-end로 실행됩니다. VFS가 CHD staging root를 씁니다.

| 항목 | 값 |
| --- | --- |
| `vfs_mount.chd` | `roms/ez2dj5th/ez2dj5.chd` |
| `vfs_mount.source_root` | `%TEMP%/re2dj/chd/ez2dj5th/EZ2DJ` |
| preparation | `handoff_prepared`, `d3d3_prepared`, `vfs_prepared`, `iat_verified` 모두 true |
| outcome | `success` |
| `.vfs.log` 줄 수 | 2635 |
| 자산 개방 | 159 |
| transform | 37 |

### 검증 — 시험과 회귀

- Windows x86 Release 전체 build 성공
- `re2dj_unit_tests.exe` → `checks: 1441, failures: 0` (1424에서 17 증가)
- 파티션 이미지 4개의 마운트 결과가 변하지 않았습니다.

| 이미지 | `partitioned` | `partition_lba` | label |
| --- | --- | --- | --- |
| `ez2dj3rd` | 1 | 63 | `NO NAME` |
| `ez2dj4th` | 1 | 63 | `EZ2DJ3_S` |
| `ez2dj6th` | 1 | 63 | `EZ2DJ3_S` |
| `ez2dj1stse` | 1 | 63 | `EZ2DJ_SE` |
| `ez2dj5th` | **0** | **0** | `EZ2DJ5` |

- 1st SE·3rd·4th를 실행해 화면을 캡처했습니다. 세 제품 모두 정상입니다.

새 단위 시험은 CHD 없이 판정 로직을 직접 덮습니다. 같은 boot sector가 파티션 시작(LBA 63)과 whole-disk(LBA 0) 양쪽에서 받아들여지는지, 파티션 테이블은 boot sector로 거절되는지, 그리고 볼륨보다 큰 총 섹터 수·서명 없음·2의 거듭제곱이 아닌 클러스터 크기·데이터 영역 밖의 root cluster·짧은 입력·널 입력이 모두 거절되는지 확인합니다.

### 남은 과제

- FAT12·FAT16·exFAT과 GPT는 지원하지 않습니다.
- `ez2dj5th`의 화면 출력은 확인하지 않았습니다. 이 작업은 마운트까지만 다뤘습니다.

## English

### Related documents

- Design: [Partitionless FAT32 Volume Design](../design/20260910-238-partitionless-fat32-volume.md)
- Work order: [Partitionless FAT32 Volume Support](../work-orders/20260910-238-partitionless-fat32-volume.md)
- Preceding task: [ez2dj1st and ez2dj5th Hardlock descriptors](20260909-237-ez2dj1st-5th-hardlock-descriptors.md)

### Problem solved

`re2dj ez2dj5th` failed with `CHD MBR has no in-range FAT32 partition`, so the target could not run from its CHD at all.

### Cause

LBA 0 of the supplied `ez2dj5.chd` is not a partition boot record but the FAT32 boot sector itself: `eb 58 90` short jump, OEM `MSWIN4.1`, 512 bytes per sector, 32 sectors per cluster, 36 reserved sectors, with a `55aa` signature. The image is a whole-disk volume with no partition table. The signature let it pass the MBR check, but the partition entry area holds no FAT32 type, so the next check rejected it. **Confirmed.**

### Code change

`ParseFat32BootSector` now holds the BPB parse and validation as a free function taking the boot sector, the volume's start LBA and its sector count. `Fat32Volume::Open` calls it at each in-range FAT32 partition entry and, when none is accepted, once more with LBA 0 and the whole image. `Fat32VolumeInfo` gained `partitioned`, which `chd_probe` reports; `partition_index` alone cannot tell a whole-disk volume from the first partition.

Acceptance is the existing BPB validation itself, with no separate format guess. That validation already checks the sector size, that the cluster size is a power of two, the reserved sectors, the FAT count, that the declared total fits inside the volume, and that the FAT covers the declared data region, so an MBR cannot pass by accident.

The partition walk changed too: it used to take the first FAT32 entry and give up if that one failed, and now it skips entries that are out of range or not boot sectors and keeps looking.

### Verification — result

The `ez2dj5th` CHD mounts as `partitioned=0` at `partition_lba=0` with label `EZ2DJ5`, and its `EZ2DJ/EZ2DJ.EXE` reads back with entry RVA `0x0070d240` and six sections — the same values [task 237](20260909-237-ez2dj1st-5th-hardlock-descriptors.md) observed from the extracted copy.

`re2dj ez2dj5th` now runs end to end from the CHD: the VFS mounts the CHD staging root, every preparation item is true, `iat_verified=true`, the outcome is `success`, and the run produces 2635 trace lines, 159 asset opens and 37 transforms.

### Verification — tests and regression

The full Windows x86 Release build succeeded and `re2dj_unit_tests.exe` reported `checks: 1441, failures: 0`, up seventeen from 1424. The four partitioned images mount exactly as before — 3rd, 4th, 6th and 1st SE all at `partitioned=1`, `partition_lba=63`, with unchanged labels — and only 5th reports `partitioned=0` at LBA 0. 1st SE, 3rd and 4th were run and their screens captured; all three are correct.

The new unit tests cover the decision directly, without a CHD: that the same boot sector is accepted both at a partition start (LBA 63) and as a whole-disk volume (LBA 0), that a partition table is refused as a boot sector, and that a declared total larger than the volume, a missing signature, a cluster size that is not a power of two, a root cluster below the first data cluster, a short input and a null input are all refused.

### Remaining work

- FAT12, FAT16, exFAT and GPT remain unsupported.
- What `ez2dj5th` displays has not been checked; this task covered mounting only.
