# 20260904-169 EZ2DJ 4th guard 1 실패 원인 추적 결과
# 20260904-169 EZ2DJ 4th Guard 1 Failure Source Results

## 1. 개요 (Overview)

Task 168에서 확정한 guard 1 이탈에 대해, 그 호출이 반환하는 실패 코드와 코드 생성 지점, 그리고 실패로 판정되는 연산까지 특정했다.

**결론: guard 1의 호출은 `RVA 0x0001010f`의 선택 루틴이고, 네 개의 후보 슬롯이 모두 0이어서 `0x81000004`를 반환한다.**

For the guard 1 exit that Task 168 confirmed, this task identified the failure code the call returns, where that code is produced, and the operation judged to have failed.

**Conclusion: guard 1 calls the selection routine at `RVA 0x0001010f`, which returns `0x81000004` because all four of its candidate slots are zero.**

---

## 2. 변경 내용 (Changes Implemented)

`src/tools/windows_x86_launcher_probe/main.cpp`만 변경했다. 새 CLI 옵션은 추가하지 않고 기존 `--null-context-entry-trace`와 `--null-context-object-reference-scan`을 재조준했다.

1. `kNullContextEntryPoints`를 `guard0_return`(`0x00011706`), `guard1_call_site`(`0x00011725`), `guard1_return`(`0x0001172a`), `slot2_early_exit_1`(`0x00011738`)로 교체.
2. 참조 스캔 `bodies`에 `guard1_thunk`(`0x00003913`), `guard1_target`(`0x0001010f`), 그리고 대상이 호출하는 세 helper thunk(`0x000012a8`, `0x00002595`, `0x00002b34`) 추가.
3. 참조 스캔 값 목록에 `guard1_error_code`(`0x81000004`) 추가.
4. 앵커 목록에 `guard1_target_entry`와 루프·비교 체인 앵커 7개 추가.

---

## 3. 검증 결과 (Verification Results)

- `scripts/build_win32.bat`: 빌드 성공, 경고·에러 0.
- `re2dj_unit_tests.exe`: 1,253 checks, 0 failures.
- `re2dj_windows_product_loader_probe.exe`: 전체 통과.

### 3.1 1단계 — guard 반환값 (`20260904-014522-290`)

`hits=4`, `recorded=4`, `capped=false`, thread `20564`에서 네 앵커가 순서대로 한 번씩 히트했다.

| 순서 | 앵커 | RVA | `EAX` |
| - | - | - | - |
| 1 | `guard0_return` | `0x00011706` | `0x00000000` |
| 2 | `guard1_call_site` | `0x00011725` | `0x00000000` |
| 3 | `guard1_return` | `0x0001172a` | **`0x81000004`** |
| 4 | `slot2_early_exit_1` | `0x00011738` | `0x00000000` |

guard 0은 0을 반환해 `jge`를 통과하고, guard 1은 최상위 비트가 선 `0x81000004`를 반환해 `jge`가 실패한다. Task 168의 guard 1 판정이 반환값 수준에서 재확인되었다.

### 3.2 1단계 — thunk 해석 (`20260904-014546-500`)

`guard1_thunk`(`0x00003913`) 분기 목록은 guard 2 thunk와 같은 형태(2건)이며, 첫 항목이 `jmp 0x0001010f`다. guard 1의 호출 대상 함수는 **`RVA 0x0001010f`**다.

### 3.3 2단계 — 실패 코드 생성 지점 (`20260904-014712-897`)

`.text` 전체에서 `0x81000004` 바이트 패턴은 5건이지만, 실제 `mov eax, imm32`는 하나뿐이다. 나머지 넷은 다른 명령 안에서 우연히 같은 4바이트가 나온 경우다.

| RVA | 직전 바이트 | 판정 |
| - | - | - |
| `0x0000fc90` | `d0` | 오탐 (데이터) |
| `0x0000ffbb` | `bc` | 오탐 (`8b 91 bc040000` = `mov edx,[ecx+0x4bc]`) |
| `0x0000ffd4` | `bc` | 오탐 (같은 형태) |
| `0x000102a1` | **`b8`** | **`mov eax, 0x81000004` at `0x000102a0`** |
| `0x000af9de` | `4f` | 오탐 (`e9 4f040000` = `jmp rel32`) |

유일한 실제 생성 지점 `0x000102a0`은 guard 1의 대상 함수 `0x0001010f` 안에 있다.

### 3.4 2단계 — 실패로 판정되는 연산

앵커 코드 창에서 복원한 결정 구조다. 모든 바이트는 런타임에 언패킹된 `.text`에서 읽었다.

```
0x00010239  imul edx, edx, 0x4d0        ; 레코드 스트라이드 1,232바이트
0x0001023f  mov  eax, [ebp-0x14]        ; 배열 base
0x00010242  add  eax, edx
0x00010247  jmp  0x00010174             ; 루프 반복

0x0001024c  mov  ecx, [ebp+0x0c]        ; 루프 종료 후 결정 시작
0x0001024f  and  ecx, 1
0x00010254  jne  0x00010266
0x00010256  cmp  dword [ebp-0x08], 0    ; 후보 A
0x0001025a  je   0x00010266
0x00010262  mov  [edx], eax             ; *out = A
0x00010264  jmp  0x000102a7             ; 성공

0x00010269  and  ecx, 1
0x0001026e  jne  0x00010280
0x00010270  cmp  dword [ebp-0x10], 0    ; 후보 B
0x00010274  je   0x00010280
0x0001027e  jmp  0x000102a7             ; 성공

0x00010280  cmp  dword [ebp-0x18], 0    ; 후보 C
0x00010284  je   0x00010290
0x0001028e  jmp  0x000102a7             ; 성공

0x00010290  cmp  dword [ebp-0x1c], 0    ; 후보 D
0x00010294  je   0x000102a0             ; 넷 다 0이면 실패로 낙하
0x0001029e  jmp  0x000102a7             ; 성공

0x000102a0  mov  eax, 0x81000004        ; 실패 반환
0x000102a5  jmp  0x000102b8

0x000102a7  mov  edx, [ebp+0x08]        ; 성공 경로
0x000102ac  mov  dword [eax+0x494], 1
0x000102b6  xor  eax, eax               ; 0 반환
```

- **확인됨 — 실패 조건은 후보 4개가 모두 0인 것이다.** `[ebp-0x08]`, `[ebp-0x10]`, `[ebp-0x18]`, `[ebp-0x1c]` 중 하나라도 0이 아니면 그 값을 `*[ebp+0x08]`에 쓰고 0을 반환한다. 넷 다 0일 때만 `0x000102a0`으로 낙하한다.
- **확인됨 — 앞선 루프는 스트라이드 `0x4d0`(1,232바이트) 배열을 순회한다.** 인덱스는 `[ebp-0x0c]`, base는 `[ebp-0x14]`이며 루프 머리는 `0x00010174`, 꼬리 `jmp`는 `0x00010247`이다.
- **확인됨 — 루프 안에서 helper `0x00012820`을 두 번 호출한다.** thunk `0x00002595`를 거치며, 호출 지점은 `0x000101cf`와 `0x00010217`이다.
- **확인됨 — 성공 경로는 부수 효과가 있다.** `mov dword [*out + 0x494], 1`을 수행하므로, 선택 결과 객체의 `+0x494` 플래그가 초기화의 다음 단계에서 쓰인다.
- **추정 — 이 함수는 열거 결과에서 장치를 고르는 선택 루틴이다.** 우선순위가 있는 후보 4개, 인자 `[ebp+0x0c]`의 비트 0으로 상위 후보 두 개를 건너뛰는 구조, 그리고 "아무것도 못 골랐다"는 형태의 오류 코드가 모두 그 해석과 맞는다. 1,232바이트 레코드의 실제 내용은 확인하지 않았다.
- **미확정 — 루프가 0회 도는지, 돌지만 후보를 채우지 못하는지.** `[ebp-0x0c]`의 최종 값과 `[ebp-0x14]` 배열의 원소 수는 아직 관측하지 않았다.

### 3.5 분기 목록의 오탐 (Branch listing false positives)

`ListNearBranches`는 디스어셈블러가 아니라 바이트 스캐너이므로 명령 중간의 `0xe8`·`0xe9`를 호출로 보고한다. guard 1 대상에서 확인한 오탐은 다음과 같다.

- `0x00010246`의 `call 0x00002b34`: 실제로는 `89 45 e8`(`mov [ebp-0x18], eax`)의 `e8`이다. `0x00002b34`에 thunk가 없다는 점(`guard1_helper_2b34` 목록의 첫 항목이 `0x00002b35`)이 이를 뒷받침한다.
- `0x00010158`의 `call 0x0001015d`: 바로 다음 명령을 가리키는 형태.
- `0x0001011f`와 `0x00010282`, `0x0001028b`, `0x000102e3`: 대상이 이미지 밖.

이 판별을 명시해 두지 않으면 이후 작업이 없는 호출을 쫓게 된다.

* `ListNearBranches` is a byte scanner rather than a disassembler, so it reports `0xe8` and `0xe9` bytes that sit inside other instructions. The confirmed false positives in guard 1's target are listed above; recording them keeps later work from chasing calls that do not exist.

---

## 4. 다음 작업 (Next Task)

`0x0001010f`의 루프가 실제로 몇 번 도는지와 후보 슬롯이 왜 비는지를 관측한다. 진입 앵커를 루프 머리(`0x00010174`), 두 helper 호출 지점(`0x000101cf`, `0x00010217`), 그리고 결정 시작(`0x0001024c`)에 두면 반복 횟수와 helper 반환값을 함께 얻을 수 있다. helper `0x00012820`의 본문 분기 목록도 같은 실행에서 수집한다.

Observe how many times the loop in `0x0001010f` actually iterates and why the candidate slots stay empty. Placing the entry anchors at the loop head (`0x00010174`), the two helper call sites (`0x000101cf`, `0x00010217`), and the decision start (`0x0001024c`) yields both the iteration count and the helper return values in one run, alongside a body branch listing for helper `0x00012820`.

---

## 5. 관련 문서 (Related Documents)

- [Task 169 설계](../design/20260904-169-ez2dj4th-guard1-failure-source.md)
- [Task 169 작업 지시서](../work-orders/20260904-169-ez2dj4th-guard1-failure-source.md)
- [Task 168 작업 로그](../work-logs/20260904-168-ez2dj4th-d3d7-init-abort.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)
