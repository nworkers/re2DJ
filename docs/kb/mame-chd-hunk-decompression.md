# MAME CHD hunk 압축 해제와 판독 비용 / MAME CHD Hunk Decompression and Read Cost

## 한국어

### CHD의 판독 단위

MAME CHD는 논리 이미지를 고정 크기 **hunk**로 나누고 각 hunk를 독립적으로 압축한다. 논리 주소 지정 단위는 별도의 **unit**(하드디스크 이미지에서는 보통 512바이트 sector)이다. 따라서 hunk 하나 안에 여러 unit이 들어간다.

`libchdr`의 판독 API는 hunk 단위다.

```c
CHD_EXPORT chd_error chd_read(chd_file *chd, uint32_t hunknum, void *buffer);
```

`buffer`는 hunk 크기 전체를 담아야 한다. 논리 바이트 범위를 읽으려면 호출자가 주소를 hunk 번호와 hunk 내 오프셋으로 나눠 필요한 hunk를 모두 요청하고 잘라 붙여야 한다.

### 캐시가 없다는 점이 중요하다

**확인됨 — 이 저장소에 포함된 `libchdr`(`third_party/libchdr/src/libchdr_chd.c`) 기준.** `chd_read`는 곧바로 `hunk_read_into_memory`를 호출하며, 직전에 읽은 hunk를 비교하거나 재사용하는 경로가 없다. 같은 hunk를 연속으로 두 번 요청하면 압축 해제도 두 번 일어난다.

이 성질은 상위 계층의 접근 패턴과 곱해질 때 문제가 된다. 예를 들어 hunk 4,096바이트, unit 512바이트, 코덱 LZMA인 이미지에서

* 512바이트 sector 하나를 읽으면 4,096바이트 hunk 하나를 통째로 LZMA 해제한다.
* FAT32 FAT 항목은 4바이트이므로 hunk 하나에 1,024개가 들어간다. 클러스터 체인을 순차로 걷는 동안 같은 hunk를 최대 1,024번 다시 요청하게 된다.
* 파일 판독이 호출마다 체인을 처음부터 다시 걷는 구조라면, 전체 비용은 파일 길이에 대해 제곱으로 증가한다.

따라서 CHD를 블록 장치로 쓰는 계층은 **압축 해제된 hunk를 캐시하는 것을 기본 전제로 설계해야 한다.** 원본 이미지를 읽기 전용으로만 열면 캐시된 hunk는 이미지 수명 동안 유효하므로 무효화 정책이 필요 없다.

이 프로젝트에서는 `re2dj::storage::ChdHunkCache`가 그 역할을 한다. 적용 설계는 [런타임 핫 경로 성능 설계](../design/20260907-219-runtime-performance-hot-paths.md)에 있다.

### 스레드 안전성

`libchdr`의 `chd_file`은 코덱별 내부 작업 버퍼를 보유한다. 여러 스레드가 같은 `chd_file`로 동시에 `chd_read`를 호출하는 것은 안전하지 않다. 호출자가 직렬화해야 한다.

### 출처 / Sources

* [libchdr](https://github.com/rtissera/libchdr) — 이 저장소가 `third_party/libchdr`로 포함한 구현.
* [MAME CHD 포맷 설명](https://docs.mamedev.org/techspecs/chd.html) — hunk, unit, 압축 코덱 개념.

---

## English

### The Read Unit of a CHD

A MAME CHD divides its logical image into fixed-size **hunks** and compresses each hunk independently. Logical addressing uses a separate **unit** — typically a 512-byte sector for hard-disk images — so one hunk holds several units.

`libchdr` exposes reads at hunk granularity:

```c
CHD_EXPORT chd_error chd_read(chd_file *chd, uint32_t hunknum, void *buffer);
```

`buffer` must hold a whole hunk. To read a logical byte range, the caller splits the address into a hunk number and an offset within the hunk, requests every hunk the range touches, and stitches the pieces together.

### The Absence of a Cache Matters

**Confirmed against the `libchdr` copy vendored here (`third_party/libchdr/src/libchdr_chd.c`).** `chd_read` calls `hunk_read_into_memory` directly, with no path that compares against or reuses the previously read hunk. Requesting the same hunk twice in a row decompresses it twice.

That property becomes a problem when it multiplies with the access pattern of the layer above. For an image with 4,096-byte hunks, 512-byte units, and LZMA compression:

* Reading one 512-byte sector fully LZMA-decompresses a 4,096-byte hunk.
* A FAT32 FAT entry is four bytes, so one hunk holds 1,024 of them. Walking a cluster chain sequentially can re-request the same hunk up to 1,024 times.
* If file reads re-walk the chain from the start on every call, the total cost grows quadratically in file length.

A layer that treats a CHD as a block device therefore has to **assume a decompressed-hunk cache from the start.** When the image is opened read-only, a cached hunk stays valid for the life of the image, so no invalidation policy is needed.

In this project `re2dj::storage::ChdHunkCache` fills that role; the applied design is in the [runtime hot-path performance design](../design/20260907-219-runtime-performance-hot-paths.md).

### Thread Safety

A `libchdr` `chd_file` owns per-codec internal working buffers. Concurrent `chd_read` calls on the same `chd_file` from several threads are not safe, and the caller has to serialize them.

### Sources

* [libchdr](https://github.com/rtissera/libchdr) — the implementation vendored here as `third_party/libchdr`.
* [MAME CHD format notes](https://docs.mamedev.org/techspecs/chd.html) — hunks, units, and compression codecs.
