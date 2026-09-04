# 20260905-184 게스트 필드 참조 스캐너와 `+0xa10` 기록자 조사 결과
# 20260905-184 Guest Field Reference Scanner And The `+0xa10` Writer — Results

## 1. 개요 (Overview)

EZ2DJ 4th의 검정화면을 막고 있는 null 필드 `[0x00aca5b0 + 0xa10]`을 누가 채워야 했는지 찾기 위해, 실행 중인 게스트 코드를 상수로 검색하는 스캐너를 만들고 조사했다.

**결론: 게임 `.text` 전체에서 이 필드에 쓰는 명령은 두 개뿐이고, 둘 다 0을 쓴다. 0이 아닌 값을 넣는 코드가 없다.**

**부수 확인: 게스트는 같은 필드를 한 곳에서는 null 검사하고, 결함이 나는 곳에서는 검사하지 않는다.**

To find what should fill the null field blocking EZ2DJ 4th, a scanner that searches the live guest's code for a constant was built and used. Across the game's entire `.text` only two instructions write the field and both store zero: nothing puts a non-zero value there. The guest null-checks the same field on one path and dereferences it unchecked on the path that faults.

---

## 2. 변경 내용 (Changes Implemented)

`src/tools/windows_x86_launcher_probe/main.cpp`

1. **스캐너 추가.** `ScanGuestFieldReferences`가 게스트 `.text`를 읽어 주어진 32비트 상수를 참조하는 명령을 찾는다. 첫 접근 위반 시점에 실행하므로 패킹된 코드가 이미 복호화된 상태다.
2. **참조 형태 세 가지.** `mod=10` 베이스+변위, `rm=100`일 때 SIB가 끼는 경우, `mod=00 rm=101` 절대 주소를 모두 처리한다.
3. **즉값 형태.** `mov r32, imm32`, `push imm32`, 레지스터 대상 group 1 `imm32`도 찾아 `offset-load`로 분류한다. 필드 주소를 먼저 계산해 두고 그 레지스터로 저장하는 코드를 놓치지 않기 위해서다.
4. **접근 종류 분류.** opcode로 read / write / modify / address / indirect / other를 구분한다. group 1(`0x80`, `0x81`, `0x83`)은 reg 필드로 `cmp`와 나머지를 갈라, null 검사를 대입으로 잘못 읽지 않게 한다.
5. **옵션.** `--field-reference-scan <hex>`. 반복 지정할 수 있어 한 실행에서 변위와 절대 주소를 함께 찾는다. 디버거가 붙어야 동작하므로 detached 실행과는 배타적이다.
6. 기존 `ScanRuntimeNullContextFieldReferences`는 손대지 않았다. 이전 작업들의 결론이 그 출력에 근거하고 있다.

---

## 3. 검증 결과 (Verification Results)

- `scripts/build_win32.bat`: 빌드 성공, 경고와 에러 0.
- `re2dj_unit_tests.exe`: 1,265 checks, 0 failures.
- `re2dj_windows_product_loader_probe.exe`: 전체 통과.
- 진단 실행: `20260905-013628-588`, `20260905-013912-801`.

### 3.1 스캐너가 기존 스캐너의 상위집합이다 (확인됨)

작업 지시서의 자기 검증 기준대로 변위 `0x11c`로 대조했다.

| 항목 | 기존 스캐너 (`20260903-032038-359`) | 새 스캐너 |
| - | - | - |
| 기록된 RVA | 23 | 42 |
| 기존 RVA 중 누락 | — | **0건** |
| 새로 찾은 RVA | — | 19건 |

- **확인됨 — 기존 결과를 하나도 잃지 않는다.** 기존 23개가 모두 새 결과에 있다.
- **확인됨 — 추가분은 구조적으로 기존 스캐너가 볼 수 없던 것이다.** `0x000b93e0`, `0x000b93f3`, `0x000b9484`, `0x000b94d7`은 `modrm=0x94`, 즉 SIB가 끼어 변위가 한 바이트 밀린 경우다. 기존 스캐너는 변위를 항상 `index+2`에서 읽으므로 이들을 지나쳤다. 나머지는 새로 추가한 절대 주소와 즉값 형태다.
- **확인됨 — 오탐도 그대로 재현된다.** `0x000c9acf`는 `0f 85 1c 01 00 00`, 즉 `jnz rel32`의 분기 오프셋이 상수와 일치한 것이다. 두 스캐너 모두 잡으며 새 스캐너는 `other`로 분류한다. 바이트 검색의 한계이고 설계가 예고한 그대로다.

### 3.2 스캔 범위가 게임 코드 전체를 덮는다 (확인됨)

`re2dj_pe_analyzer`로 읽은 4th 실행 파일의 섹션 표다.

```
.text     0x00001000 0x000db022
.rdata    0x000dd000 0x0000c766
.data     0x000ea000 0x005e66b0
.idata    0x006d1000 0x0000171c
.reloc    0x006d3000 0x0000c05d
.protect  0x006e0000 0x00039569
```

- **확인됨 — 스캔 범위가 `.text`와 정확히 같다.** 스캐너가 읽은 897,058바이트는 `0x000db022`와 일치한다.
- **확인됨 — 실행 가능한 다른 섹션은 `.protect` 하나뿐이다.** 패커 구간이며 이번 스캔에 포함하지 않았다.
- **확인됨 — 전역 `0x00aca5b0`은 `.data` 안이다.** RVA `0x006ca5b0`이 `.data` 범위에 든다.

### 3.3 스캐너가 결함 명령 자체를 찾아낸다 (확인됨)

`0xa10` 스캔 결과에 `rva=0x00022b34`가 read로 들어 있다. 접근 위반이 난 `mov ecx,[eax+0xa10]` 바로 그 명령이다. 스캐너가 올바른 코드를 보고 있다는 독립적인 확인이다.

### 3.4 `+0xa10`을 쓰는 코드는 0만 쓴다 (확인됨)

`0xa10` 스캔은 20건을 찾았다. read 15, write 2, other 1, offset-load 2다.

| RVA | 바이트 | 해독 | 판정 |
| - | - | - | - |
| `0x000225d1` | `c7 80 10 0a 00 00 00 00 00 00` | `mov dword [eax+0xa10], 0` | **0을 쓴다** |
| `0x00022aad` | `c7 80 10 0a 00 00 00 00 00 00` | `mov dword [eax+0xa10], 0` | **0을 쓴다** |
| `0x00022a5f` | `83 ba 10 0a 00 00 00` + `74 4f` | `cmp dword [edx+0xa10], 0` / `jz +0x4f` | null 검사 |
| `0x00022b34` | `8b 88 10 0a 00 00` | `mov ecx,[eax+0xa10]` | **결함 지점** |
| `0x000228e3` | `f4 05 10 0a 00 00` | `0f 85`류 오탐과 같은 성격 | 오탐 |
| `0x00063a68` | `68 10 0a 00 00` + `e8 …` | `push 0xa10` / `call 0x00402dec` | 할당 크기, 필드와 무관 |

- **확인됨 — 0이 아닌 값을 이 필드에 넣는 명령이 `.text`에 없다.** write 두 건은 모두 `imm32 = 0`이다.
- **확인됨 — `0x000225d1`은 초기화 나열의 일부다.** 앞이 `mov eax,[ebp-4]`, 뒤가 `mov ecx,[ebp-4]`에 이어 같은 형태의 `c7`이다. 생성자에서 멤버를 차례로 0으로 만드는 모양이다.
- **확인됨 — `0x00022aad`은 해제 뒤 정리다.** 바로 앞 `0x00022aa5`가 `call` rel32로 RVA `0x000c1ba0`을 부르고, 그다음 필드를 0으로 만든다.
- **확인됨 — `0x00022a5f`의 null 검사는 그 정리 경로를 감싼다.** `jz`가 건너뛰는 `0x4f`바이트 뒤는 `0x00022ab7`, 즉 정리 직후다.
- **확인됨 — 결함 지점에는 검사가 없다.** `0x00022b34`가 읽은 값을 `0x00022b3a`가 그대로 역참조한다.
- **확인됨 — 절대 주소로는 한 번도 접근하지 않는다.** `0x00acafc0` 스캔은 0건이다.
- **확인됨 — `0x00063a68`은 필드 접근이 아니다.** `push 0xa10` 뒤 `call`과 `mov [ebp-0x14], eax`가 이어지므로 2,576바이트 할당의 크기 인자다. 같은 값이 오프셋이자 크기로 쓰이는 우연이다.

### 3.5 판정하지 않은 것 (미확정)

- **미확정 — 이 필드를 채우는 코드가 어디 있는가.** `.text`에 리터럴 `+0xa10` 저장이 없다는 사실만 확인했다. 객체가 다른 별칭으로 설치되는지, `.protect`에서 설치되는지, 애초에 이 실행에서 생성 경로가 실행되지 않는지는 구분하지 못했다.
- **미확정 — 전역 `0x00aca5b0`이 어떤 클래스인가.** RVA `0x22000`–`0x23000` 구간의 메서드들이 이 필드를 다루는 것만 확인했다.
- **미확정 — `+0xa10`이 가리켜야 할 객체의 종류.** 결함 지점이 vtable 슬롯 9(`[eax+0x24]`)를 부르지만 어떤 인터페이스인지 확정하지 않았다.

---

## 4. 이 결과가 뜻하는 것 (What This Means)

게스트 코드는 이 포인터를 **비우기만 한다.** 생성자가 0으로 두고, 정리 경로가 null 검사 뒤 다시 0으로 두며, 결함 지점은 검사 없이 역참조한다. 그 사이에 값을 넣는 코드가 게임 `.text` 어디에도 없다.

따라서 다음 조사는 "왜 안 쓰였나"가 아니라 **"어디서 쓰이도록 되어 있나"**를 다른 수단으로 찾아야 한다. 바이트 검색은 자기 범위 안에서 답을 냈고, 그 범위 밖에 답이 있다.

*The guest only ever clears this pointer. The next step therefore cannot be another byte search of the same range; it has to find the installation path by other means.*

---

## 5. 다음 작업 (Next Task)

1. 필드 주소 `0x00acafc0`에 하드웨어 쓰기 감시점을 걸어 두 번의 0 쓰기가 언제 일어나는지 관측한다. 생성자 시점을 잡으면 객체의 수명 구간이 정해진다.
2. 결함 함수의 호출 사슬을 `0x004235fa` → `0x0043627e` → `0x004076ef`로 거슬러, null 검사가 있는 `0x00422a50` 계열과 어떤 분기에서 갈리는지 본다.
3. 두 방법이 모두 비면 `.protect` 구간으로 스캔 범위를 넓힌다.

---

## 6. 관련 문서 (Related Documents)

- [Task 184 설계](../design/20260905-184-guest-field-reference-scan.md)
- [Task 184 작업 지시서](../work-orders/20260905-184-guest-field-reference-scan.md)
- [Task 182 작업 로그](20260905-182-directx7-legacy-delegation.md)
- [4th 그래픽 경로 분석](../analysis/ez2dj4th-graphics-path.md)
