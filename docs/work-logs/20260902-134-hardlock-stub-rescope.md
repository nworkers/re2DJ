# Hardlock 스텁 재정의와 4th descriptor ID 확보 작업 로그

관련 작업 지시: [Hardlock 스텁 재정의와 4th descriptor ID 확보](../work-orders/20260902-134-hardlock-stub-rescope.md)

*Related work order: [Hardlock stub rescope and 4th descriptor ID capture](../work-orders/20260902-134-hardlock-stub-rescope.md).*

## 결과

- 제품 CLI에서 `--hardlock-bypass`를 제거했습니다. `OriginalProcessOptions::hardlock_bypass`와 인자 전달, profile 검증도 함께 제거했습니다. 제품 실행 경로에는 합성 응답 옵션이 남아 있지 않습니다.
- launcher 옵션을 `--hardlock-stub`으로, runtime export를 `g_re2dj_hardlock_stub_enabled`로, trace 접두사를 `re2dj:vfs:hardlock-stub:`으로, JSONL 이벤트를 `hardlock_stub`으로 바꿨습니다. 플랫폼 중립 `HardlockStubDevice`와 unit test는 그대로 유지했습니다.
- product-loader probe의 검사를 "우회 opt-in 확인"에서 "제품 인자에 진단 옵션이 하나도 없음"으로 바꿨습니다.
- 명시적 진단 옵션 `--hardlock-descriptor-ids`와 export `g_re2dj_hardlock_descriptor_ids_enabled`를 추가했습니다. 옵션이 없으면 기존 `secret_fields=redacted` 동작이 그대로입니다.
- 4th 원본을 두 번 독립 실행해 module address와 두 ID를 확보했습니다.

*Removed `--hardlock-bypass` from the product CLI along with `OriginalProcessOptions::hardlock_bypass`, its argument forwarding, and its profile validation, so the product path carries no synthetic-response option. Renamed the launcher option to `--hardlock-stub`, the runtime export to `g_re2dj_hardlock_stub_enabled`, the trace prefix to `re2dj:vfs:hardlock-stub:`, and the JSONL event to `hardlock_stub`, while keeping the platform-neutral `HardlockStubDevice` and its unit tests unchanged. Changed the product-loader probe's check from "bypass is opt-in" to "no diagnostic option appears in product arguments at all". Added the explicit `--hardlock-descriptor-ids` diagnostic and its `g_re2dj_hardlock_descriptor_ids_enabled` export, leaving the existing `secret_fields=redacted` behavior in place without the option. Obtained the 4th module address and both IDs from two independent runs.*

## 4th descriptor 관찰값

두 실행 `20260902-012549-512`, `20260902-012610-638`이 동일한 값을 기록했습니다.

| 필드 | 값 |
| --- | --- |
| API version | `0347` |
| module id | `0x0000` |
| module address | `0x4c53` |
| remote | `1` |
| port | `0x0378` |
| `ID_Ref` | `a755931881fd81ea` |
| `ID_Verify` | `ceed1a5e4f27078f` |

3rd의 module address는 `0x4c51`, `ID_Ref`는 `478c8b793f201f8a`, `ID_Verify`는 `cc22ae2da344b2a2`였으므로 4th는 별개의 module입니다. 이 값들은 seed 자체나 유효한 Function `0x0e` 응답이 아니라 seed 복구 제약식의 입력입니다.

*Both runs recorded API version `0347`, module id `0x0000`, module address `0x4c53`, remote `1`, port `0x0378`, `ID_Ref=a755931881fd81ea`, and `ID_Verify=ceed1a5e4f27078f`. 3rd's module address is `0x4c51` with `ID_Ref=478c8b793f201f8a` and `ID_Verify=cc22ae2da344b2a2`, so 4th is a separate module. These values are inputs to seed-recovery constraints, not seeds or valid Function `0x0e` responses.*

## 외부 자료 검토

사용자가 제시한 [exetools 스레드](https://exetools.forumotion.com/t224-reversing-seeds-from-hardlock-key-possible)의 `hl_seed.c`는 SaPu 작성, `Copyright 2002-2005`이며 **라이선스가 명시되어 있지 않습니다.** 명시 없는 저작권 표기는 모든 권리 유보로 취급하므로 GPL보다도 도입 여지가 없습니다. 소스를 복사·번역·파생하지 않았고 저장소에 넣지 않았습니다.

스레드에서 사용 가능한 것은 사실 관계뿐입니다. `ID_Ref`/`ID_Verify` 한 쌍에서 브루트포스로 seed 후보를 찾을 수 있고, 한 쌍만으로는 후보가 여럿 남으며(스레드 보고 55개), 추가 암호문으로 구분해야 한다는 점입니다. 이는 [Task 107](20260831-107-ez2dj3rd-hardlock-seed-smt.md)이 3rd에서 SMT로 후보 11개를 얻고 판별 oracle이 필요하다고 남긴 결론과 독립적으로 일치합니다.

*The `hl_seed.c` referenced in the user-supplied [exetools thread](https://exetools.forumotion.com/t224-reversing-seeds-from-hardlock-key-possible) is by SaPu, marked `Copyright 2002-2005`, and **states no license.** A copyright notice without a license is treated as all rights reserved, leaving even less room for adoption than GPL, so its source was not copied, translated, derived from, or placed in the repository. Only its factual claims are usable: seed candidates can be brute-forced from an `ID_Ref`/`ID_Verify` pair, one pair leaves several candidates (the thread reports 55), and additional ciphertext is needed to disambiguate. That independently matches Task 107's conclusion for 3rd, where SMT produced eleven candidates and recorded the need for a distinguishing oracle.*

## 검증

- Windows x86 Debug build 통과
- 선택 CTest: `3/3` 통과. product-loader probe가 제품 인자에 진단 옵션이 없음을 확인합니다.
- 4th 두 실행이 동일한 module address와 두 ID를 기록했습니다.
- `git diff --check` 통과

*Verification: the Windows x86 Debug build passes, selected CTest passes 3/3 with the product-loader probe confirming that no diagnostic option reaches product arguments, both 4th runs recorded the same module address and IDs, and `git diff --check` passes.*

## 다음 단계

1. 스텁에 후보 Function `0x0e` 응답을 호출 순서대로 주입하는 옵션을 추가합니다.
2. 복호화된 영역의 엔트로피와 x86 prologue 출현으로 후보를 자동 판별하는 도구를 만듭니다.
3. Function `0x0e` 구현은 허용 가능한 독립 근거가 확보될 때까지 보류합니다. 1·2가 끝나면 후보만 주어지면 즉시 판별 가능한 상태가 됩니다.

*Next: add an option that injects candidate Function `0x0e` responses into the stub in call order; build a judge that scores candidates from the decrypted region's entropy and x86 prologue frequency; and keep the Function `0x0e` implementation deferred until a policy-compatible independent basis exists, since steps one and two make any supplied candidate immediately testable.*
