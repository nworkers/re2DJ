# 20260904-175 EZ2DJ 4th 초기화되지 않은 out 인자 추적 결과
# 20260904-175 EZ2DJ 4th Uninitialized Out-Parameter Tracing Results

## 1. 개요 (Overview)

Task 174가 남긴 접근 위반의 원인을 끝까지 따라갔다.

**결론: 실패한 호출은 자원 적재다. 게스트는 이름 `2PLAYERInsertCoin.str`로 자원을 요청하고, 적재가 실패하면 `RVA 0x0000827f`의 loader가 out 인자를 쓰지 않고 0을 반환한다. 호출자는 반환값을 무시하고 초기화되지 않은 지역(`0xcccccccc`)을 다음 호출로 넘겨 접근 위반이 난다.**

**적재가 실패하는 이유는 두 가지다. 첫째, 게스트는 자기 현재 디렉터리를 바꾼 뒤 상대 이름으로 자원을 열지만(`SetCurrentDirectoryA`를 해석해 쓴다), re2DJ의 VFS는 모든 상대 이름을 HDD 루트에 붙여 `EZ2DJ/2PLAYERInsertCoin.str`을 찾는다. 실제 파일은 `EZ2DJ/SYSTEM/Common/`에 있다. 둘째, FAT32 긴 이름 조립에 결함이 있어 13자를 넘는 이름 뒤에 4바이트가 끼어든다.**

The failing call is a resource load. The loader at `RVA 0x0000827f` returns 0 without writing its out parameter when the load fails, the caller ignores that return value and passes the uninitialized local (`0xcccccccc`) into the next call, and that call faults. The load fails for two reasons: the guest changes its working directory and opens resources by bare name while re2DJ's VFS joins every relative name to the HDD root, and the FAT32 long-name assembly splices four extra bytes into names longer than thirteen characters.

---

## 2. 변경 내용 (Changes Implemented)

1. **코드 범위 교체.** 참조 스캔의 `code_ranges`에서 `driver_stage`를 빼고 `av_callee`(`0x00009690`), `av_caller`(`0x00065700`), `link_thunks`(`0x00001840`), `singleton_loader`(`0x00008270`)를 넣었다.
2. **자원 이름 창.** `loader_names_0/1/2`(`0x000f8800`, `0x000f8840`, `0x000f8880`)를 데이터 창에 추가했다.
3. **CHD 디렉터리 나열.** `re2dj_chd_probe`에 `--list <relative-directory>` 옵션을 추가했다.

---

## 3. 검증 결과 (Verification Results)

- `scripts/build_win32.bat`: 빌드 성공, 경고와 에러 0.
- `re2dj_unit_tests.exe`: 1,253 checks, 0 failures.
- `re2dj_windows_product_loader_probe.exe`: 전체 통과.
- 참조 스캔: `logs/windows_x86_launcher_probe/ez2dj4th/20260904-142908-872.jsonl`.
- 접근 위반 실행: `logs/windows_x86_launcher_probe/ez2dj4th/20260904-141951-723`(`.jsonl`, `.vfs.log`).

### 3.1 호출 사슬 (확인됨)

```
0x000658a1  call  0x004c1c71                  ; sprintf(name, "%dPLAYERInsertCoin.str", n)
0x000658a9  lea   ecx, [ebp-0x114]            ; out
0x000658b0  lea   edx, [ebp-0x110]            ; name
0x000658b7  mov   ecx, [0x00ac29b4]           ; 싱글턴
0x000658bd  call  0x0040185c -> 0x0040827f    ; loader(name, &out)
0x000658c2  push  1
0x000658c4  mov   eax, [ebp-0x114]            ; out을 검사 없이 사용
0x000658d7  call  0x004023d8 -> 0x00409696    ; 여기서 접근 위반
```

`n`은 바로 앞 호출의 결과를 `neg / sbb / add 2`로 1 또는 2로 만든 값이다.

### 3.2 loader는 실패 경로에서 out을 쓰지 않는다 (확인됨)

```
0x0000827f  push ebp; mov ebp,esp; SEH 프레임 설치
0x000082cc  mov  ecx, [ebp+0x08]              ; name
0x000082d3  mov  ecx, [edx+0x1d0]             ; 캐시 컨테이너
0x000082d9  call ...                          ; 이름으로 조회
0x000082de  test eax, eax
0x000082e0  je   0x00008308                   ; 캐시에 없으면 적재로
0x000082e8  mov  [eax], ecx                   ; *out = 캐시된 값
...
0x00008311  call ...                          ; 파일 적재
0x00008316  test eax, eax
0x00008318  jne  0x00008346                   ; 성공이면 등록으로
0x0000831e  push 0x004eb118                   ; 실패: 오류 메시지 출력
0x0000832b  mov  dword [ebp-0x28], 0          ; 0을 반환
0x00008344  jmp  epilogue                     ; *out은 그대로 둔다
```

- **확인됨 — 실패 경로는 `*out`을 쓰지 않고 0을 반환한다.** 성공 경로(`0x000082e8`, `0x00008395` 이후)에서만 쓴다.
- **확인됨 — 호출자는 반환값을 검사하지 않는다.** `0x000658c2`부터 곧바로 `[ebp-0x114]`를 사용한다. 원본에서도 적재 실패는 곧 크래시라는 뜻이다.

### 3.3 faulting 함수의 모양 (확인됨)

```
0x00009696  mov  [ebp-0x04], ecx              ; this
0x000096a4  cmp  dword [ebp+0x08], 0          ; arg1 == 0이면 조기 반환
0x000096b2  mov  [eax], ecx                   ; this->[0x000] = arg1
0x000096ba  mov  [edx+0x04], eax              ; this->[0x004] = arg2
0x000096c0  mov  dword [ecx+0x210], 1
0x000096cd  mov  dword [edx+0x08], 0
0x000096d7  mov  dword [eax+0x20c], 0
0x000096ef  call memset(this+0x0c, 0, 0x200)
0x00009701  mov  eax, [eax+0x08]              ; arg1->[0x08]  ***** 접근 위반 *****
0x00009704  mov  [edx+0x0c], eax              ; arg1->[0x0c] = arg1->[0x08]
0x0000970c  cmp  dword [edx+0x1d8], 0
```

`arg1`이 `0`이면 조기 반환하지만 `0xcccccccc`는 0이 아니므로 그대로 역참조된다.

### 3.4 자원 이름 (확인됨)

`.rdata` 창에서 읽은 이름들이다.

| RVA | 이름 |
| - | - |
| `0x000f8800` | `Freeplay.bmp` |
| `0x000f8810` | `Credits.bmp` |
| `0x000f8834` | `Credits_mask.bmp` |
| `0x000f8848` | `coin0.wav` |
| `0x000f8854` | `coin1.wav` |
| `0x000f8860` | `%dPLAYERInsertCoin.str` |
| `0x000f8878` | `%dPLAYERPressStart.str` |
| `0x000f8890` | `%dPLAYERWAIT.str` |

### 3.5 VFS는 상대 이름을 HDD 루트에 붙인다 (확인됨)

접근 위반 실행의 `.vfs.log`다.

```
create-file:stage=request:request=2PLAYERInsertCoin.str:access=0x80000000:disposition=0x00000003
create-file:stage=native:request=2PLAYERInsertCoin.str:mapped=...\chd\ez2dj4th\EZ2DJ\2PLAYERInsertCoin.str:success=0:error=2
```

- **확인됨 — 실패한 열기는 34건이고 모두 상대 이름이다.** `Credits.abm`, `Credits_0.abm`부터 `Credits_S10.abm`, `2PLAYERInsertCoin.str`까지다. 성공한 열기는 모두 게스트가 만든 절대 경로(`EZ2DJ.ini`, `fontkr.dat`, `fontEn.dat`)다.
- **확인됨 — 게스트는 `SetCurrentDirectoryA`를 해석해 쓴다.** 동적 resolver 기록에 `SetCurrentDirectoryA`와 `GetCurrentDirectoryA`가 있다. re2DJ는 두 API를 대체하지도, 게스트의 현재 디렉터리를 추적하지도 않는다.
- **확인됨 — `MapVfsPath`는 상대 이름을 HDD 루트에 그대로 붙인다.** `injected_runtime.cpp`의 분기에서 `name[0]`이 구분자가 아니면 `hdd_suffix = name`이다. 현재 디렉터리는 고려되지 않는다.

### 3.6 실제 파일은 하위 디렉터리에 있다 (확인됨)

`re2dj_chd_probe --list`로 확인했다.

| 디렉터리 | 항목 수 | 확인한 자원 |
| - | - | - |
| `EZ2DJ` | 9 | `EZ2DJ.exe`, `EZ2DJ.INI`, `FONTKR.DAT`, `FONTEN.DAT`, `BG`, `Sound`, `SYSTEM` |
| `EZ2DJ/SYSTEM` | 31 | 모드별 하위 디렉터리 29개와 `WARNING.ABM` |
| `EZ2DJ/SYSTEM/Common` | 67 | `Credits.abm`, `1PLAYER.STR`, `1PLAYERInsertCoin.str` |
| `EZ2DJ/SYSTEM/Title` | 38 | `TITLE.STR`, `INSERTCOIN.str`, `PRESS.STR` |

`EZ2DJ` 바로 아래에는 요청된 자원이 하나도 없다. 게스트가 현재 디렉터리를 `SYSTEM\Common` 같은 위치로 바꾼 뒤 상대 이름으로 여는 것이 유일하게 앞뒤가 맞는 해석이다.

### 3.7 FAT32 긴 이름 조립에 결함이 있다 (확인됨)

나열 결과에서 13자를 넘는 이름마다 13번째 문자 뒤에 두 개의 잘못된 문자가 끼어 있다.

| 나열된 이름 | 끼어든 바이트 | 짧은 이름의 앞 4자 |
| - | - | - |
| `logo4th_mask.佌佇abm` | `4c 4f 47 4f` | `LOGO` |
| `INSERTCOIN.st义䕓r` | `49 4e 53 45` | `INSE` |
| `SIMBOL_MASK.a䥓䉍bm` | `53 49 4d 42` | `SIMB` |
| `1PLAYERInsert倱䅌Coin.str` | `31 50 4c 41` | `1PLA` |
| `Credits_0.abm剃䑅` | `43 53 45 44` | — |

- **확인됨 — 끼어드는 4바이트는 같은 항목의 짧은 이름 앞부분이다.** 긴 이름 슬롯 하나가 담는 13문자를 넘어 4바이트를 더 읽고 있다.
- **확인됨 — 정확히 13자인 이름도 영향을 받는다.** `Credits_0.abm`은 13자라 종료 문자가 없는 경우다.
- **추정 — 이 결함만으로도 13자를 넘는 자원의 조회는 실패한다.** `Fat32Volume::Find`는 이름 비교로 항목을 찾으므로, 조립된 이름이 다르면 일치하지 않는다. 현재 실패는 디렉터리 문제로도 설명되므로 둘을 분리해 확인하지는 않았다.

---

## 4. 가설 판정 (Hypothesis Verdicts)

| 가설 | 판정 |
| - | - |
| 싱글턴 메서드가 조건부로만 out을 쓴다 | **확인.** 실패 경로는 쓰지 않는다 |
| 실패 원인이 우리 쪽 응답이다 | **확인.** VFS 경로 해석이 원인이다 |
| 자원이 이미지에 없다 | **반증.** `EZ2DJ/SYSTEM/` 아래에 있다 |
| 게스트가 현재 디렉터리를 바꾼다 | **확인.** `SetCurrentDirectoryA`를 해석해 쓴다 |

---

## 5. 다음 작업 (Next Task)

두 결함을 각각 다룬다.

1. **VFS 현재 디렉터리 추적.** `SetCurrentDirectoryA`와 `GetCurrentDirectoryA`를 HLE로 받아 게스트의 논리 현재 디렉터리를 유지하고, 상대 이름을 그 디렉터리 기준으로 해석한 뒤 매핑한다. 호스트 프로세스의 실제 작업 디렉터리는 바꾸지 않는다. 게스트가 보는 경로는 이미지 안의 경로여야 하기 때문이다.
2. **FAT32 긴 이름 조립 수정.** 슬롯당 13문자만 취하고, 종료 문자가 없는 경우와 `0xffff` 패딩을 함께 처리한다. 이미지 없이 재현 가능한 단위 테스트를 함께 둔다.

Handle the two defects separately: give the VFS a tracked guest working directory fed by HLE `SetCurrentDirectoryA` and `GetCurrentDirectoryA` without moving the host process's real directory, and fix the FAT32 long-name assembly to take exactly thirteen characters per slot while handling both the unterminated and `0xffff`-padded cases, with a unit test that needs no image.

---

## 6. 관련 문서 (Related Documents)

- [Task 175 설계](../design/20260904-175-ez2dj4th-uninitialized-out-parameter.md)
- [Task 175 작업 지시서](../work-orders/20260904-175-ez2dj4th-uninitialized-out-parameter.md)
- [Task 174 작업 로그](../work-logs/20260904-174-ez2dj4th-io-out-helper.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)
