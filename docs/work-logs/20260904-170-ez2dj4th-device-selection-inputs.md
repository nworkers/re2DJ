# 20260904-170 EZ2DJ 4th 장치 선택 입력 관측 결과
# 20260904-170 EZ2DJ 4th Device Selection Input Observation Results

## 1. 개요 (Overview)

Task 169가 남긴 질문 — 선택 루틴의 루프가 0회 도는지, 돌지만 후보를 채우지 못하는지 — 에 답했다.

**결론: 루프는 9개 레코드를 모두 순회하지만, 매 반복에서 `record + 0x4c8`이 0이라 GUID 비교에 도달하기 전에 건너뛴다. 게스트는 우리 열거 데이터를 정상적으로 받아 레코드에 복사해 두었다.**

This task answered the question Task 169 left open: whether the selection routine's loop iterates zero times or iterates without filling a candidate.

**Conclusion: the loop walks all nine records but skips each one before the GUID comparison because `record + 0x4c8` is zero. The guest did receive our enumeration data and copied it into the records.**

---

## 2. 변경 내용 (Changes Implemented)

`src/tools/windows_x86_launcher_probe/main.cpp`만 변경했다. 새 CLI 옵션은 추가하지 않았다.

1. **데이터 창 추가.** 참조 스캔에 자식 프로세스의 지정 RVA 바이트 창을 읽어 16진수와 출력 가능 문자로 기록하는 블록을 넣었다. `.rdata`와 `.data`는 디스크에서 암호화되어 있어 이 경로로만 읽을 수 있다.
2. **진입 앵커 재조준.** `guard1_loop_head`(`0x00010174`), `guard1_helper_call_0`(`0x000101cf`), `guard1_helper_call_1`(`0x00010217`), `guard1_decision_start`(`0x0001024c`).
3. **helper 본문 추가.** `bodies`에 `{"guard1_match_helper", 0x00012820, 0x00000100, 0}`.
4. **장치 테이블 창 추가.** count(`0x0054cd9c`)와 레코드 0·1의 머리·선택 필드·게이트.

---

## 3. 검증 결과 (Verification Results)

- `scripts/build_win32.bat`: 빌드 성공, 경고·에러 0.
- `re2dj_unit_tests.exe`: 1,253 checks, 0 failures.
- `re2dj_windows_product_loader_probe.exe`: 전체 통과.

### 3.1 루프 구조 복원 (`20260904-020106-461`)

코드 창에서 복원한 선택 루틴 `RVA 0x0001010f`의 앞부분이다.

```
0x0001012f  cmp  dword [ebp+0x08], 0
0x00010133  jne  0x0001013f
0x00010135  mov  eax, 0x80070057          ; E_INVALIDARG, out 인자 null
0x0001013f  lea  eax, [ebp-0x04]          ; &count
0x00010143  lea  ecx, [ebp-0x14]          ; &array
0x00010147  call 0x000012a8 -> 0x000100ea ; *array = 0x00946d50, *count = [0x0094cd9c]
0x0001014f  mov  dword [ebp-0x1c], 0      ; 후보 D = 0
0x00010156  mov  dword [ebp-0x18], 0      ; 후보 C = 0
0x0001015d  mov  dword [ebp-0x10], 0      ; 후보 B = 0
0x00010164  mov  dword [ebp-0x08], 0      ; 후보 A = 0
0x0001016b  mov  dword [ebp-0x0c], 0      ; index = 0

0x00010174  mov  edx, [ebp-0x0c]          ; 루프 머리
0x00010177  add  edx, 1
0x0001017d  mov  eax, [ebp-0x0c]
0x00010180  cmp  eax, [ebp-0x04]
0x00010183  jae  0x0001024c               ; index >= count -> 결정 단계
0x00010189  imul ecx, [ebp-0x0c], 0x4d0
0x00010195  cmp  dword [edx+ecx+0x4c8], 0
0x0001019d  je   0x00010247               ; 게이트가 0이면 이 레코드를 통째로 건너뜀
0x000101af  cmp  dword [ecx+eax+0x118], 0
0x000101b7  je   0x00010201               ; 어느 GUID와 비교할지 선택
0x000101b9  push 0x004e4da0               ; GUID A
0x000101cf  call 0x00012820
0x00010201  push 0x004e4dc0               ; GUID B
0x00010217  call 0x00012820
```

### 3.2 비교 상수는 문자열이 아니라 GUID다 (확인됨)

데이터 창에서 읽은 두 상수의 바이트를 `GUID` 리틀엔디언으로 해석한 결과다.

| 주소 | 바이트에서 해석한 GUID | 대응 |
| - | - | - |
| `0x004e4da0` | `{f5049e78-4861-11d2-a407-00a0c90629a8}` | `IID_IDirect3DTnLHalDevice` |
| `0x004e4db0` | `{8767df22-bacc-11d1-8969-00a0c90629a8}` | `IID_IDirect3DNullDevice` |
| `0x004e4dc0` | `{50936643-13e9-11d1-89aa-00a0c9054129}` | `IID_IDirect3DRefDevice` |
| `0x004e4dd0` | `{881949a1-d6f3-11d0-89ab-00a0c9054129}` | `IID_IDirect3DMMXDevice` |

푸시되는 두 주소는 `0x004e4da0`과 `0x004e4dc0`이므로, 비교 대상은 T&L HAL과 Reference 디바이스다. helper `0x00012820`은 2인자 cdecl이며 일치 시 0이 아닌 값을 반환한다. 즉 이 helper는 문자열 비교가 아니라 **GUID 비교**다.

### 3.3 루프는 9회 돌고 비교에는 한 번도 도달하지 않는다 (확인됨)

`20260904-020043-391.jsonl`: `hits=10`, `recorded=9`, `capped=true`.

- `guard1_loop_head` 8건 기록, `EAX`가 `0`부터 `7`까지 단조 증가. `EAX`는 루프 인덱스다.
- `guard1_decision_start` 1건, `EAX=0x00000009`.
- `guard1_helper_call_0`과 `guard1_helper_call_1`은 **0건**.

따라서 아홉 번의 반복 모두 `0x0001019d`의 `je`로 빠져나갔다.

### 3.4 게이트 필드가 0이다 (확인됨)

장치 테이블을 자식 메모리에서 직접 읽었다.

| 창 | 주소 | 내용 |
| - | - | - |
| `device_table_count` | `0x0094cd9c` | `09000000` = **9** |
| `device_record_0_head` | `0x00946d50` | `"RGB Emulation"` + 0 패딩, `+0x28` = `0x009471ec`, `+0x2c` = `0x0008af51` |
| `device_record_0_gate` | `0x00947218` | `+0x4c8` = **`0x00000000`** |
| `device_record_1_head` | `0x00947220` | `"Direct3D HAL"` |
| `device_record_1_gate` | `0x009476e8` | `+0x4c8` = **`0x00000000`** |

- **확인됨 — 레코드 수는 9다.** `.ddraw.log`가 기록한 3회 열거 × 3개 장치와 일치한다.
- **확인됨 — 게스트는 장치 이름을 레코드에 복사한다.** `+0x00`에 40바이트 인라인 `CHAR` 배열로 `"RGB Emulation"`, `"Direct3D HAL"`이 들어 있다. 우리 facade가 콜백에 넘긴 스택 지역 버퍼의 수명 문제라는 Task 170 설계 4절의 선행 가설은 **반증**되었다.
- **확인됨 — 우리 열거 caps가 레코드에 들어 있다.** `+0x2c`의 `0x0008af51`은 `FillDeviceDescription`이 채운 `dwDevCaps` 값과 정확히 같다. 따라서 `D3DDEVICEDESC7`은 레코드 `+0x2c`부터 놓인다.
- **확인됨 — `+0x28`은 레코드 자신 안을 가리킨다.** 값 `0x009471ec`는 레코드 base + `0x49c`다. 게스트는 GUID를 레코드에 복사하고 그 사본을 가리키는 포인터를 둔다.
- **확인됨 — 두 레코드 모두 `+0x4c8`이 0이다.** 이것이 GUID 비교 이전에 모든 레코드를 걸러내는 게이트다.
- **추정 — 레코드는 DirectX 7 SDK 예제 프레임워크의 열거 구조와 같은 계열이다.** 선두 40바이트 설명 문자열, `+0x28`의 GUID 포인터, `+0x2c`의 `D3DDEVICEDESC7`, 그리고 `+0x120`에서 관측된 `0x0000017c`(= `sizeof(DDCAPS)`의 `dwSize`)가 모두 그 형태와 맞는다. 전체 필드 배치는 확인하지 않았다.
- **미확정 — `+0x4c8`에 무엇을 쓰는가.** 이 필드를 채우는 코드 경로와 그것이 실행되지 않은 이유는 아직 관측하지 않았다.

### 3.5 3회 반복의 의미 (추정)

`.ddraw.log`의 여섯 호출이 세 번 반복되는 것은 게스트가 `DirectDrawEnumerateEx`로 얻은 드라이버마다 `DirectDrawCreateEx` → `QueryInterface(IID_IDirect3D7)` → `GetCaps` → `EnumDisplayModes` → `EnumDevices`를 한 벌씩 수행하기 때문으로 보인다. 드라이버 3개 × 장치 3개 = 레코드 9개라는 관측과 맞는다. 드라이버 열거 자체는 아직 HLE 경계에 걸리지 않으므로 확인은 못 했다.

The six calls repeating three times in `.ddraw.log` appear to be one pass per driver returned by `DirectDrawEnumerateEx`, which matches three drivers times three devices giving nine records. The driver enumeration itself does not yet pass through an HLE boundary, so this is not confirmed.

---

## 4. 가설 판정 (Hypothesis Verdicts)

| 가설 | 판정 |
| - | - |
| 루프가 0회 돈다 | **반증.** 9회 돈다 |
| 열거 문자열의 스택 수명 문제 | **반증.** 게스트가 레코드에 복사한다 |
| helper 비교가 실패한다 | **반증.** helper에 도달조차 하지 않는다 |
| `+0x4c8` 게이트가 0이라 걸러진다 | **확인** |

---

## 5. 다음 작업 (Next Task)

`record + 0x4c8`을 채우는 코드 경로를 찾는다. 주소가 고정 전역이므로 레코드 0의 게이트 `0x00947218`에 하드웨어 쓰기 감시를 걸면 writer를 직접 잡을 수 있고, 기존 `--null-context-field-writer-trace` 기구가 4바이트 범위 감시를 이미 제공한다. writer가 한 번도 실행되지 않으면 그 앞의 조건을, 실행되는데 0을 쓰면 그 입력을 추적한다. 병행해서 `.text`에서 `0x4c8` 변위 접근을 스캔해 정적 후보를 모은다.

Find the code path that fills `record + 0x4c8`. The address is a fixed global, so a hardware write watch on record 0's gate at `0x00947218` catches the writer directly, and the existing `--null-context-field-writer-trace` machinery already watches a four-byte range. If the writer never runs, trace the condition before it; if it runs and writes zero, trace its input. In parallel, scan `.text` for accesses with a `0x4c8` displacement to collect static candidates.

---

## 6. 관련 문서 (Related Documents)

- [Task 170 설계](../design/20260904-170-ez2dj4th-device-selection-inputs.md)
- [Task 170 작업 지시서](../work-orders/20260904-170-ez2dj4th-device-selection-inputs.md)
- [Task 169 작업 로그](../work-logs/20260904-169-ez2dj4th-guard1-failure-source.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)
