# EZ2DJ I/O 포트 맵

## 한국어

### 확인됨 — 원본 1st SE 실행 파일

- byte 입력 helper는 `0x101`~`0x106`, byte 출력 helper는 `0x100`~`0x103`과 `0x106`을 사용한다.
- `0x101`, `0x102`, `0x106` 입력은 bitwise NOT 뒤 boolean 상태로 사용되므로 active-low다.
- `0x103`~`0x105`는 이전 값과 비교된다. `0x105`는 `current - previous`가 음수이면 256을 더한 delta를 credit 관련 전역에 누적하므로 8비트 증가 counter다.
- 근거와 주소는 [원본 실행 파일 구조 분석](ez2dj-exe-structures.md)과 [legacy port HLE 설계](../design/20260825-062-legacy-io-port-hle.md)에 누적되어 있다.

### 확인됨 — 실제 키보드 실행과 원본 counter 계산

사용자 실제 실행에서 F3에 연결한 기존 `0xfe → 0xff` pulse는 credit을 99까지 증가시켰다. 대응 unprotected binary의 VA `0x0041764c`는 port `0x105`의 `current - previous`를 계산하고 음수면 `0x100`을 더하며, VA `0x00417675`부터 그 delta를 credit 관련 누적값에 더한다. 따라서 `0x105`는 press마다 한 방향으로 1 증가하고 read 뒤에도 유지되는 modulo-256 counter여야 한다. 되돌아가는 pulse 모델은 잘못된 것으로 확인됐다.

작업 087은 초기값 `0x00`의 stable 8-bit counter를 구현하고 false→true마다 1 증가시키며 read는 값을 변경하지 않도록 정정했다. 공용 hold/release/repress/wrap test와 표준 Windows x86 build, CTest 3/3이 통과했다. 실제 press당 credit 1 증가는 사용자 재검증 전이므로 **미확정**이다.

### 추정 — 독립된 공개 구현과의 교차 확인

[2EZConfig-V2 commit `a7346e0`](https://github.com/ben-rnd/2EZConfig-V2/tree/a7346e066bb569643e0cb25f41cfdc079126fbc6)의 공개 구현은 같은 port 범위에 다음 의미를 부여한다. 이는 원본 cabinet 또는 회로도에서 직접 확인한 사실이 아니므로 프로젝트 분석에서는 **추정**으로 유지한다.

| 방향 | Port | 추정 의미 |
| --- | --- | --- |
| IN | `0x101` | P1/P2 Start, Effector 1~4, Service, Test(active-low) |
| IN | `0x102` | P1 key 1~5, pedal(active-low) |
| IN | `0x103` | P1 turntable absolute 8-bit position |
| IN | `0x104` | P2 turntable absolute 8-bit position |
| IN | `0x105` | coin 입력에 연결된 8비트 누적 counter; press마다 +1, read로 소비하지 않음 |
| IN | `0x106` | P2 key 1~5, pedal(active-low) |
| OUT | `0x100` | red/blue cabinet lamps, neon(active-high) |
| OUT | `0x101` | Start와 Effector lamps(active-high) |
| OUT | `0x102` | P1 key와 turntable lamps(active-high) |
| OUT | `0x103` | P2 key와 turntable lamps(active-high) |

구체적인 교차 확인 위치는 [입력 구현](https://github.com/ben-rnd/2EZConfig-V2/blob/a7346e066bb569643e0cb25f41cfdc079126fbc6/src/2ez-dll/ez2dj-io/ez2dj_io_input.cpp)과 [출력 구현](https://github.com/ben-rnd/2EZConfig-V2/blob/a7346e066bb569643e0cb25f41cfdc079126fbc6/src/2ez-dll/ez2dj-io/ez2dj_io_output.cpp)이다. 해당 저장소는 [GPL-3.0](https://github.com/ben-rnd/2EZConfig-V2/blob/a7346e066bb569643e0cb25f41cfdc079126fbc6/LICENSE)이므로 re2DJ는 코드를 복사·링크하지 않고 관찰 가능한 protocol만 독립 구현한다.

### 미확정

- 실제 1st SE cabinet 배선과 모든 bit의 물리 순서
- 원본 분석에서 관찰된 `OUT 0x106`의 의미
- turntable 초기 위치가 cabinet power-on 동작에서도 `0x80`인지 여부
- 각 출력 lamp의 실제 장치 연동과 timing 요구사항

## English

### Confirmed — original 1st SE executable

The byte helpers read ports `0x101` through `0x106` and write `0x100` through `0x103` plus `0x106`. Inputs `0x101`, `0x102`, and `0x106` are active-low. Values from `0x103` through `0x105` are compared with prior samples. For `0x105`, the original computes `current - previous`, adds 256 when negative, and accumulates that delta into credit-related globals, confirming an eight-bit increasing counter.

### Confirmed — real keyboard run and original counter calculation

In the user's real run, the old `0xfe` then `0xff` pulse mapped to F3 drove credit to 99. At VA `0x0041764c`, the corresponding unprotected binary computes port `0x105` current minus previous, adds `0x100` when negative, and from VA `0x00417675` accumulates the delta into credit-related globals. Port `0x105` must therefore increase once per press and remain stable after reads; the returning pulse model is confirmed incorrect.

Task 087 implements a stable eight-bit counter initialized to `0x00`, incremented once per false-to-true transition and unchanged by reads. Shared hold/release/repress/wrap coverage and the standard Windows x86 build plus CTest 3/3 pass. Exactly one real credit per press remains **unresolved** pending user revalidation.

### Inferred — cross-check against an independent public implementation

2EZConfig-V2 commit `a7346e0` assigns the semantic meanings listed in the table above to the same port range. Because this was not verified from original cabinet hardware or schematics, re2DJ records the physical mapping as **inferred**. Its GPL-3.0 code is neither copied nor linked; re2DJ independently implements only the externally observable protocol under its BSD-3-Clause policy.

### Unresolved

The exact 1st SE cabinet wiring, the meaning of `OUT 0x106`, hardware power-on turntable position, and physical lamp timing remain unresolved.
