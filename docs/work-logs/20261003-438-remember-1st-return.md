# 작업 438 작업 로그 — Remember 1st에서 6th로 돌아가기 / Task 438 work log — returning from Remember 1st to 6th

설계: [20261003-438-remember-1st-return.md](../design/20261003-438-remember-1st-return.md) · 지시서: [20261003-438-remember-1st-return.md](../work-orders/20261003-438-remember-1st-return.md)

## 2026-10-03

- **증상**: 1st 자식이 `IDirectDraw4::RestoreDisplayMode`(`0x0041473c`)에서 멈추고 launcher가 `ExitProcess(0)`으로 끝났다(`20261003-111451-799`, `20261003-111521-062`). 직전에 1st는 bookkeeping 쓰기, 사운드 버퍼 정지·해제, `IDirect3DDevice3::SetTexture(0)`, `IDirectDraw4::RestoreAllSurfaces`를 했다.
  *Symptom: the 1st child stopped at `IDirectDraw4::RestoreDisplayMode` (`0x0041473c`) and the launcher ended with `ExitProcess(0)` (`20261003-111451-799`, `20261003-111521-062`); just before, 1st had written bookkeeping, stopped and released its sound buffers, called `IDirect3DDevice3::SetTexture(0)` and `IDirectDraw4::RestoreAllSurfaces`.*
- **원본 확인**(Remember 1st, `0x411bbf5c`, 작업 436의 파일 이미지):
  - import 표의 `ExitProcess` 호출은 세 곳이다: 오류 wrapper `0x004169c2`(0), `0x004370f2`(0xff), CRT `0x0043e5f9`(인자 그대로).
  - 오류 wrapper `0x004169b0`의 호출처 36곳은 모두 오류 문구를 넘긴다.
  - WinMain의 정상 경로는 `0x004218c1`의 `[0x01b0c2c4] = 1`, 정리 `0x0041ebb0`, bookkeeping 저장, `0x004219c0`의 `[0x00451f58] = 0x105` 순이다.

  *The original (Remember 1st, `0x411bbf5c`, task 436's file image): the import table's `ExitProcess` is called at three places, the error wrapper `0x004169c2` (0), `0x004370f2` (0xff) and the CRT's `0x0043e5f9` (its argument); all 36 callers of the error wrapper `0x004169b0` pass an error text; WinMain's normal path is `[0x01b0c2c4] = 1` at `0x004218c1`, the clean-up `0x0041ebb0`, the bookkeeping save, then `[0x00451f58] = 0x105` at `0x004219c0`.*
- **구현**: `DirectDraw4RestoreDisplayMode`, `DirectDraw7RestoreDisplayMode`(`DD_OK`). 단위 테스트에 두 호출을 더했다.
  *Implementation: `DirectDraw4RestoreDisplayMode` and `DirectDraw7RestoreDisplayMode` (`DD_OK`), with both calls added to the unit tests.*
- **검증**
  - `scripts/test_all.sh linux-x64-debug`(경고를 오류로): build 성공, CTest 4개 통과.
  - 정리 경로를 직접 재현하려고, 직접 띄운 1st 실행에서 게스트 코드를 바꿔 어트랙트 루프를 끝내게 해 봤다(`0x0041f6d0`이 1을 돌려주고, 어트랙트 장면 함수들은 곧바로 돌아오게, `[0x01b0c2c4] = 1`). 세 번 모두 90~200초 안에 정리 경로에 닿지 않았다. 그 시점에 게스트가 어느 루프에 있었는지는 확인하지 못했다. 이 시험은 직접 띄운 프로세스의 메모리에만 썼다.
  - 실제 경로 확인은 사용자에게 남긴다.

  *Verification: `scripts/test_all.sh linux-x64-debug` (warnings as errors) builds and passes 4 CTest tests. To reproduce the clean-up path, the guest code of a directly started 1st run was changed so the attract loop would end (`0x0041f6d0` returning 1, the attract scene functions returning at once, `[0x01b0c2c4] = 1`); none of three tries reached the clean-up within 90 to 200 seconds, and where the guest was looping then was not found. The test wrote only into the memory of the process it started. The real path is left to the user.*
- **후속**: 사용자 실행에서 1st가 이어서 `PostQuitMessage`에서 멈춰 [작업 439](20261003-439-remember-1st-quit-message.md)로 이어졌다. 작업 437~440 뒤 사용자가 6th 복귀를 확인했다.
  *Follow-up: in the user's run 1st next stopped at `PostQuitMessage`, leading to [task 439](20261003-439-remember-1st-quit-message.md); after tasks 437 to 440 the user confirmed the return to 6th.*
