# 20260907-219 CHD/FAT32 판독 캐시 작업 로그 / CHD and FAT32 Read Cache Work Log

* 설계: [20260907-219-runtime-performance-hot-paths.md](../design/20260907-219-runtime-performance-hot-paths.md)
* 작업 지시: [20260907-219-chd-fat32-read-cache.md](../work-orders/20260907-219-chd-fat32-read-cache.md)

## 변경 내용 / What Changed

* `include/re2dj/storage/chd_hunk_cache.h`, `src/storage/chd_hunk_cache.cpp`를 새로 추가했다. hunk 번호를 키로 하는 LRU 캐시이고, 용량은 hunk 크기와 바이트 예산에서 계산한다. 기본 예산은 32 MiB다.
* `MameChdImage`가 `ChdHunkCache`와 `std::mutex`, 재사용 스크래치 버퍼를 갖는다. `Read`는 호출당 hunk 버퍼 할당을 하지 않고, 같은 hunk를 다시 요청하면 `chd_read`를 호출하지 않는다.
* `Fat32Volume`에 디렉터리 캐시, 경로 캐시, 클러스터 체인 캐시를 추가했다. 세 캐시 모두 상한이 있고, 상한을 넘으면 통째로 비운다. 읽기 전용 볼륨이므로 어떤 항목도 무효화가 필요 없다.
* `Fat32Volume::Find`가 경로 캐시를 쓴다. 실패한 조회도 기억한다. 게스트가 선택적 파일을 반복해서 찾는 경우가 있기 때문이다.
* `Fat32Volume::ReadFileRange`가 체인 캐시로 시작 클러스터를 찾고, 중간 클러스터 버퍼 없이 `MameChdImage::Read`로 목적지에 직접 판독한다. 호출마다 첫 클러스터부터 체인을 다시 걷던 동작이 사라졌다.
* `Fat32Volume::ReadFatEntry`가 512바이트 섹터 버퍼 대신 4바이트를 직접 판독한다.
* 공개 판독 API를 `std::mutex`로 직렬화했다. 이전에는 잠금이 전혀 없었고 `libchdr`의 내부 버퍼가 공유 상태였다.
* `tests/unit/chd_hunk_cache_test.cpp`를 추가하고 테스트 등록을 갱신했다.

*Added the `ChdHunkCache` LRU component and wired it into `MameChdImage` together with a mutex and a reusable scratch buffer, so a repeated hunk no longer calls `chd_read` and no allocation happens per call. Added bounded directory, path, and cluster-chain caches to `Fat32Volume`; `Find` now uses the path cache including negative results, and `ReadFileRange` locates its start cluster through the chain cache and reads straight into the destination instead of re-walking the chain from the first cluster on every call. `ReadFatEntry` reads four bytes directly rather than a whole sector buffer. The public read API is now serialized by a mutex where previously there was no lock at all. A unit test for the cache was added and registered.*

## 검증 / Verification

* `re2dj_unit_tests`: checks 1404, failures 0.
* 사용자 제공 `roms/ez2dj4th/ez2dj4th.chd`에 대해 `re2dj_chd_probe` 출력이 변경 전후 **바이트 단위로 동일**했다. CHD v5, hunk 4,096, unit 512, FAT32 파라미터, `EZ2DJ/EZ2DJ.EXE` PE 헤더가 모두 같다.
* `re2dj ez2dj4th --list-targets` 출력도 변경 전후 동일했다.

*The unit suite passes with 1,404 checks and no failures; `re2dj_chd_probe` output on the user-supplied CHD is byte-identical before and after, as is the `re2dj ez2dj4th --list-targets` output.*

## 측정 / Measurement

Debug 구성, 파일 캐시가 더워진 상태에서 측정했다.

| 워크로드 | 변경 전 | 변경 후 |
| --- | --- | --- |
| `re2dj ez2dj4th --list-targets` (n=5, 첫 회 제외 중앙값) | 0.541 s | 0.525 s |
| `re2dj_chd_probe <chd>` (n=3) | 1.13 ~ 1.26 s | 1.02 ~ 1.35 s |

**이 두 워크로드에서는 유의미한 차이가 나타나지 않았다.** 두 워크로드는 `chd_open`의 고정 비용과 파일 하나에 대한 단일 선형 판독이 대부분을 차지하므로, 이번 변경이 제거한 비용을 거의 건드리지 않는다.

제거한 비용은 **같은 파일을 작은 단위로 반복해서 읽을 때** 나타나는 것이다. 그 형태의 부하는 게스트 자신의 판독 패턴에서만 발생하므로, 실제 개선 폭은 게임 실행 수준의 측정으로 확인해야 한다. 현재 시점에서 "게임이 얼마나 빨라졌다"고 단정하지 않는다.

*Measured in the Debug configuration with a warm file cache. Neither available tool workload shows a meaningful difference, because both are dominated by the fixed `chd_open` cost and a single linear read of one file, which is not what this change targets. The removed cost appears only when the same file is read repeatedly in small pieces, which is the guest's own read pattern; the real improvement therefore has to be confirmed by a game-level measurement, and no claim about game speed is made here.*

## 남은 사항 / Open Items

* 게스트 실행 중 판독 지연을 계측할 수단이 없다. VFS 경계에 판독 횟수와 누적 시간을 세는 진단을 붙이면 이번 변경의 효과를 실제로 확인할 수 있다.
* 32 MiB 기본 예산은 근거 있는 측정 없이 정한 값이다. 실제 작업 집합을 확인한 뒤 조정할 여지가 있다.

*There is no instrument for read latency while the guest runs; a counter of read calls and accumulated time at the VFS boundary would make the effect measurable. The 32 MiB default budget was chosen without a measured working set and may need adjusting.*
