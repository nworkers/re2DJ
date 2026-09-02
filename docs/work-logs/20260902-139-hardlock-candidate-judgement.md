# Hardlock 후보 판별 작업 로그

관련 작업 지시: [Hardlock 후보 판별](../work-orders/20260902-139-hardlock-candidate-judgement.md), 관련 가이드: [seed 복구 워크스루](../guides/hardlock-seed-recovery-walkthrough.md)

*Related work order: [Hardlock candidate judgement](../work-orders/20260902-139-hardlock-candidate-judgement.md); related guide: the [seed recovery walkthrough](../guides/hardlock-seed-recovery-walkthrough.md).*

## 결과 — 두 제품 모두 후보가 하나로 좁혀졌다

| 제품 | 후보 수 | 판별된 후보 | 근거 |
| --- | --- | --- | --- |
| ez2dj3rd | 105 | **`candidate-83`** | 유일하게 보호를 통과 |
| ez2dj4th | 93 | **`candidate-68`** | 유일하게 보호를 통과 |

후보 번호는 reSoftlock `artifacts/<제품>/seed-candidates.txt`의 index입니다. **seed 값과 응답 바이트는 이 저장소에 기록하지 않습니다.**

*The result — both products narrowed to one candidate: `candidate-83` for 3rd out of 105 and `candidate-68` for 4th out of 93, each the only one to pass the protection. The numbers are indexes into reSoftlock's `artifacts/<product>/seed-candidates.txt`; **no seed value or response byte is recorded in this repository.***

## 판별 근거

틀린 후보와 판별된 후보는 **실행 형태 자체가 다릅니다.** fault 주소는 쓰지 않았습니다.

| 관찰 | 틀린 후보 | 판별된 후보 |
| --- | --- | --- |
| 종료 | fault | **`0x00000000` 정상 종료** |
| 3rd vfs trace | 365줄 | **377줄** |
| 4th vfs trace | 451줄 | **468줄** |
| `0x450` handshake | 4회 | **8회** |
| 장치 재오픈 | 없음 | **`\\.\FEnteDev` 다시 열림** |
| descriptor `function=1` | 0회 | **2회** (4th, 최초 관찰) |
| `EZ2DJ.ini` 열기 | 없음 | **있음** |

핵심은 마지막 두 줄입니다. transform loop 이후 게스트가 **장치를 다시 열고 자신의 설정 파일 `EZ2DJ.ini`를 읽습니다.** 이것은 보호 stub의 동작이 아니라 복호화된 게임 코드의 동작입니다. 암호문 상태에서는 나올 수 없습니다.

*Judgement basis: the run shape itself differs, with no use of the fault address. Wrong candidates fault; the identified candidate exits `0x00000000`, produces a longer trace, doubles the `0x450` handshakes, reopens `\\.\FEnteDev`, adds `function=1` descriptors on 4th, and opens `EZ2DJ.ini`. The last two matter most: after the transform loop the guest reopens the device and reads its own configuration file, which is decrypted game-code behavior rather than protection-stub behavior and cannot arise from ciphertext.*

## 분포

주입은 198회 실행 전부 완전했습니다. 3rd는 32줄, 4th는 36줄 모두 `mapped=1:unmapped=0`입니다.

| 종료 코드 | 3rd (105) | 4th (93) |
| --- | --- | --- |
| `0xc0000005` access violation | 74 | 59 |
| `0xc0000096` privileged instruction | 21 | 19 |
| `0xc000001d` illegal instruction | 6 | 10 |
| 기타 fault | 3 | 4 |
| **`0x00000000`** | **1** | **1** |

fault 코드가 후보마다 갈리는 것 자체가 인과성의 증거입니다. 응답이 다르면 복호화 결과가 다르고, 다른 쓰레기 코드는 다른 방식으로 죽습니다.

*Distribution: all 198 runs injected completely, with 32 transform lines for 3rd and 36 for 4th reading `mapped=1:unmapped=0`. Wrong candidates spread across access violations, privileged instructions, and illegal instructions, with exactly one clean exit per product. That the fault code varies by candidate is itself evidence of causality: a different response decrypts to different garbage, and different garbage dies differently.*

## 재현성

판별된 후보를 각각 세 번 실행했습니다.

- 3rd `candidate-83`: 세 실행 모두 377줄, `0x00000000`, `EZ2DJ.ini` 2회, handshake 8회
- 4th `candidate-68`: 세 실행 모두 468줄, `0x00000000`, `EZ2DJ.ini` 2회, handshake 8회, `function=1` 2회

*Reproducibility: each identified candidate was run three times, and every run reproduced the same line count, clean exit, two `EZ2DJ.ini` opens, and eight handshakes, plus two `function=1` descriptors on 4th.*

## 남은 유보

- **미확정 — 실제 dongle seed인지.** 판별된 후보는 원본 실행이 보호를 통과하게 만드는 응답을 냅니다. 이것이 물리 dongle의 seed와 같은 값이라는 직접 증거는 아닙니다. 원본이 요구하는 응답을 만족한다는 사실만 확인되었습니다.
- **미확정 — 복호화 영역의 직접 측정.** 실행 중 게스트 memory dump 경로가 아직 없어 [Task 137](20260902-137-decrypted-region-judge.md) 판별기로 `.text`를 직접 채점하지 못했습니다. 근거는 실행 형태이지 엔트로피 측정이 아닙니다.
- 합성 진단값 `0100fafa0010`과 `0001`을 계속 사용했습니다. 이 값들이 실제 driver 응답이라는 근거는 없습니다.

*Remaining reservations: it is unresolved whether the identified candidate holds the physical dongle's seeds — what is confirmed is only that it produces responses the original accepts; the decrypted region was not measured directly, because there is still no in-run guest memory dump path and the Task 137 judge could not score `.text`, so the basis is run shape rather than entropy; and the synthetic `0100fafa0010` and `0001` diagnostics were still in use, with no evidence that they are real driver responses.*

## 드러난 다음 장벽 — VFS 절대 경로 이중 결합

보호를 통과한 두 실행 모두 같은 지점에서 멈춥니다. 게스트가 **절대 host 경로**로 `EZ2DJ.ini`를 열면 VFS가 그 경로를 상대 경로로 보고 root에 다시 붙입니다.

```
request = C:\...\ez2dj\EZ2DJ.ini
mapped  = C:\...\ez2dj\C:\...\ez2dj\EZ2DJ.ini
success = 0, error = 123
```

`MapVfsPath`는 `D:\ez2dj`와 `C:\windows` 접두사만 인식하고, 나머지 중 슬래시로 시작하지 않는 경로를 전부 게스트 상대 경로로 취급합니다. 드라이브 문자로 시작하는 host 경로가 그 분기에 걸립니다.

정상 종료 코드 `0x00000000`은 게임이 설정을 읽지 못해 스스로 종료한 결과로 **추정**됩니다. 보호 통과가 새로운 경계를 연 것이며, 이 버그는 그 경계의 첫 항목입니다.

*The next barrier exposed — VFS absolute-path double join. Both passing runs stop at the same place: when the guest opens `EZ2DJ.ini` by an **absolute host path**, the VFS treats it as relative and joins it onto the root again, failing with error 123. `MapVfsPath` recognizes only the `D:\ez2dj` and `C:\windows` prefixes and treats every other path not starting with a slash as guest-relative, which catches a host path beginning with a drive letter. The clean `0x00000000` exit is **inferred** to be the game exiting on its own after failing to read its configuration. Passing the protection opened a new boundary, and this bug is its first item.*

## 검증

- 198회 주입 실행 전부 주입 완전
- 판별된 후보 각 3회 재현
- 판별 기준에 fault 주소 미사용
- 원본 process 잔존 없음. 3rd loop는 실행마다 잔여 프로세스를 확인하고 경로 일치 시에만 종료하도록 했으며, 잔존 사례는 없었습니다
- 원본 HDD, CHD, overlay 변경 없음
- seed 값과 응답 바이트는 저장소에 들어가지 않았습니다

*Verification: all 198 runs injected completely; each identified candidate reproduced three times; the fault address was not used as a criterion; no original process remained, the 3rd loop checking for a survivor each run and terminating only on a path match with no survivors observed; the original HDD, CHD, and overlay are unchanged; and no seed values or response bytes entered the repository.*

## 다음 단계

1. `MapVfsPath`가 절대 host 경로를 그대로 통과시키도록 고칩니다. 두 제품이 같은 지점에서 막혀 있습니다.
2. launcher probe에 게스트 memory dump를 추가해 [Task 137](20260902-137-decrypted-region-judge.md) 판별기로 복호화된 `.text`를 직접 채점합니다. 이번 판별의 유보 하나가 해소됩니다.
3. 보호 통과 이후 실행 경계를 다시 측정합니다. 지금까지의 도달 지점 기록은 모두 보호 앞에서 멈춘 것들입니다.

*Next: fix `MapVfsPath` so an absolute host path passes through, since both products are blocked at that same point; add a guest memory dump to the launcher probe so the Task 137 judge can score the decrypted `.text` directly, which removes one reservation from this judgement; and re-measure the execution boundary beyond the protection, since every reached-boundary record so far stopped in front of it.*
