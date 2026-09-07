# 20260907-219 CHD/FAT32 판독 캐시 구현 계획 / CHD and FAT32 Read Cache Work Order

설계: [20260907-219-runtime-performance-hot-paths.md](../design/20260907-219-runtime-performance-hot-paths.md)

## 목표 / Goal

CHD 기반 FAT32 판독 경로에서 반복 압축 해제와 반복 체인 순회를 제거한다. 판독 결과 바이트는 변하지 않아야 한다.

*Remove repeated decompression and repeated chain traversal from the CHD-backed FAT32 read path. The bytes returned must not change.*

## 작업 항목 / Work Items

1. `include/re2dj/storage/chd_hunk_cache.h`, `src/storage/chd_hunk_cache.cpp`를 새로 만든다.
   * `ChdHunkCache` 클래스. 생성자에서 hunk 크기와 바이트 예산을 받아 항목 수를 정한다.
   * `const std::uint8_t* Find(std::uint32_t hunk) const` 대신, 적중 시 복사 없이 접근할 수 있는 `Lookup`과 삽입용 `Insert`를 제공한다.
   * LRU 제거. 용량 0이면 항상 미적중으로 동작한다.
2. `MameChdImage`
   * `ChdHunkCache`를 멤버로 보유하고 `Read`에서 사용한다.
   * 호출당 hunk 벡터 할당을 제거한다.
   * `std::mutex`로 `Read`와 `ReadSector`를 직렬화한다.
3. `Fat32Volume`
   * 디렉터리 캐시, 경로 캐시, 클러스터 체인 캐시를 추가한다.
   * `Find`가 캐시를 사용하도록 바꾼다.
   * `ReadFileRange`가 체인 캐시로 시작 클러스터를 찾고, 중간 버퍼 없이 목적지에 직접 판독하도록 바꾼다.
   * `std::mutex`로 공개 판독 API를 직렬화한다.
4. `CMakeLists.txt`의 `re2dj_chd_storage`에 새 소스를 추가한다.
5. `tests/unit/chd_hunk_cache_test.cpp`를 추가하고 테스트 목록에 등록한다.

*Create the `ChdHunkCache` component with LRU eviction sized from a hunk size and a byte budget; use it in `MameChdImage::Read`, drop the per-call hunk allocation, and serialize reads with a mutex; add directory, path, and cluster-chain caches to `Fat32Volume`, route `Find` through them, make `ReadFileRange` locate its start cluster through the chain cache and read straight into the destination, and serialize the public read API; register the new source in `re2dj_chd_storage` and add a unit test.*

## 제약 / Constraints

* 플랫폼 공용 코어이므로 호스트 OS API를 직접 호출하지 않는다. `std::filesystem`과 표준 라이브러리만 쓴다.
* 원본 CHD는 어떤 경우에도 쓰기 대상이 아니다.
* 캐시는 읽기 전용 이미지를 전제로 하므로 무효화 경로를 만들지 않는다.
* 소스 주석은 영어로만 작성한다.

*The shared core must not call host OS APIs directly and uses only `std::filesystem` and the standard library; the original CHD is never written; the caches assume a read-only image so no invalidation path is created; source comments are English only.*

## 검증 / Verification

* `re2dj_unit_tests` 전체 통과.
* 사용자 제공 CHD가 있으면 `re2dj_chd_probe`가 변경 전과 동일한 FAT32 파라미터와 PE 헤더를 보고하는지 확인한다.
* 원본 자산 없이도 빌드와 단위 테스트가 통과해야 한다.

*Run the full `re2dj_unit_tests`; where a user-supplied CHD exists, confirm `re2dj_chd_probe` reports the same FAT32 parameters and PE header as before; the build and unit tests must still pass with no original assets present.*
