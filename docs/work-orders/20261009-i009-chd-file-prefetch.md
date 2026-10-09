# #9 작업 지시서 — CHD 파일 백그라운드 미리 읽기 / #9 work order — prefetching CHD files in the background

이슈: [#9](https://github.com/nworkers/re2DJ/issues/9) · 설계: [20261009-i009-chd-file-prefetch.md](../design/20261009-i009-chd-file-prefetch.md)

## 절차 / Steps

1. `guest_file_prefetcher.{h,cpp}`: 작업 스레드, FIFO 큐, `Job`(버퍼·`loaded`·취소), `Start`·`Copy`·`Cancel`, 메모리 한도. `re2dj` HLE 라이브러리에 추가.
   *The worker thread, FIFO queue, `Job` (buffer, `loaded`, cancellation), `Start`, `Copy`, `Cancel` and the memory limit, added to the HLE library.*
2. `GuestFiles`: 첫 `Read` 뒤 남은 양이 256KiB 이상인 이미지 파일에서 시작, `Read`가 미리 읽은 범위를 먼저 사용, `Close`가 취소. 시작·끝 로그.
   *Start on an image file with at least 256 KiB left after its first `Read`, serve `Read` from the prefetched range first, cancel on `Close`, and log the start and end.*
3. 단위 테스트 `guest_file_prefetcher_test.cpp`.
   *Unit test `guest_file_prefetcher_test.cpp`.*
4. `ARCHITECTURE.md`의 게스트 파일 설명 갱신.
   *Update the guest-file description in `ARCHITECTURE.md`.*
5. Windows·WSL Linux 빌드와 CTest, 실제 6th SpaceMix 플레이로 검증.
   *Build and run CTest on Windows and WSL Linux, and verify by playing 6th's SpaceMix.*

## 완료 조건 / Done when

나눠 읽는 큰 CHD 파일이 백그라운드에서 미리 읽히고, 그 뒤의 `ReadFile`이 디스크를 기다리지 않으며, 읽은 내용이 직접 읽기와 같고, 단위 테스트와 모든 타깃 빌드가 통과한다.

*A large CHD file read in pieces is prefetched in the background, later `ReadFile`s no longer wait on the disk, the bytes match a direct read, and the unit tests and every target's build pass.*
