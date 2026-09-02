# ez2dj4th 보호 형태 조사 작업 로그

관련 작업 지시: [ez2dj4th 보호 형태 조사](../work-orders/20260902-133-ez2dj4th-protection-shape.md)

*Related work order: [ez2dj4th protection shape survey](../work-orders/20260902-133-ez2dj4th-protection-shape.md).*

## `0x450` 응답 바이트별 대조

기준 응답 `0100fafa0010`으로 시작해 한 바이트씩 바꾼 bounded 실행을 했습니다. 모든 실행은 `--device-mock-wts-console-session --device-mock-hardlock-44c-tail 0001 --hardlock-bypass --slot-writer-trace`를 사용했고, 기준값 실행을 두 번 해 결정성을 확인했습니다.

| 응답 | descriptor | transform | child exit |
| --- | --- | --- | --- |
| `0100fafa0010` (기준, 2회) | 37 | 36 | `0xc0000005` |
| `fe00fafa0010` (byte 0) | 37 | 36 | `0xc0000005` |
| `01fffafa0010` (byte 1) | 37 | 36 | `0xc0000005` |
| `010005fa0010` (byte 2) | 0 | 0 | `0x00000008` |
| `0100fa050010` (byte 3) | 0 | 0 | `0x00000008` |
| `0100fafaff10` (byte 4) | 37 | 36 | `0xc0000005` |
| `0100fafa00ef` (byte 5) | 37 | 36 | `0xc0000005` |

이어서 byte 2·3만 바꾼 두 번째 대조를 했습니다. `010000fa0010`, `0100fa000010`, `010001010010`, `0100ffff0010`, `010012340010`, `0100fbfa0010`, `0100fafb0010` 일곱 가지 모두 descriptor에 도달하지 못하고 handshake 3회 뒤 `0x00000008`로 종료했습니다.

**확인됨.** 원본은 6바이트 `0x450` 응답 중 byte 2와 byte 3만 검증하며, 두 바이트는 정확히 일치해야 합니다. 한 바이트만 인접값으로 바꿔도 실패하므로 범위나 구조 검사가 아니라 16비트 정확 일치입니다. 나머지 네 바이트는 검사되지 않습니다.

*Byte-wise `0x450` differential: starting from the `0100fafa0010` baseline, one byte at a time was changed in bounded runs using `--device-mock-wts-console-session --device-mock-hardlock-44c-tail 0001 --hardlock-bypass --slot-writer-trace`, with the baseline run twice to confirm determinism. Changing byte 0, 1, 4, or 5 left the run at 37 descriptors and 36 transforms ending in `0xc0000005`, while changing byte 2 or byte 3 stopped it before any descriptor, after three handshakes, exiting `0x00000008`. A second differential over bytes 2 and 3 alone — `010000fa0010`, `0100fa000010`, `010001010010`, `0100ffff0010`, `010012340010`, `0100fbfa0010`, and `0100fafb0010` — failed in every case. **Confirmed:** the original validates only bytes 2 and 3 of the six-byte response and requires them to match exactly; changing one byte to an adjacent value still fails, so this is a 16-bit exact match rather than a range or structural check, and the other four bytes are unchecked.*

## 섹션별 평문 여부

원본 이미지 각 섹션의 byte 분포를 측정했습니다.

| 섹션 | 크기 | Shannon 엔트로피 | zero byte 비율 |
| --- | --- | --- | --- |
| `.text` | 901,120 | 7.997 | 0.37–0.44% |
| `.rdata` | 53,248 | 7.896 | 4.50% |
| `.data` | 114,688 | 7.998 | 0.41% |
| `.protect` | 237,568 | 7.969 | 2.22% |

`.text`는 64 KiB 단위 14개 구간 전부가 엔트로피 `7.997`이고 zero byte 비율이 균등난수 기대값 `0.39%` 부근입니다. 마지막 구간만 section padding 때문에 `7.726`입니다. `55 8b ec` prologue와 `cc` padding은 전체에서 각각 0회입니다.

**확인됨.** `.text`에는 평문 구간이 없습니다. 부분 암호화가 아니라 전 구간이 암호문입니다.

*Section plaintext survey: byte distributions were measured for each section of the original image. `.text` is 7.997 bits/byte across all fourteen 64 KiB chunks with zero-byte shares near the uniform-random expectation of 0.39%, the final chunk reading 7.726 only because of section padding, and the whole section contains zero `55 8b ec` prologues and zero `cc` padding. `.rdata`, `.data`, and `.protect` are likewise high entropy. **Confirmed:** `.text` has no plaintext region; it is ciphertext throughout rather than partially encrypted.*

## 판정 — 진입점 직행 가능 여부

**불가능합니다.** 건너뛸 관문 뒤에 평문 게임 코드가 있는 구조가 아닙니다.

1. `.text` 전체가 파일에서 암호문이므로 점프해 갈 평문 진입점 자체가 없습니다.
2. 보호 코드는 실행 중에 `.text`를 다시 씁니다. 파일과 프로세스 memory의 같은 위치 바이트가 다릅니다.
3. [Task 131](20260901-131-hardlock-bypass-stub.md)의 인과 실험대로 Function `0x0e` 출력이 그 결과와 이후 실행 경로를 결정합니다.

즉 Hardlock은 게임 앞을 막는 검문소가 아니라 게임을 복호화하는 열쇠 공급원입니다. 보호를 건너뛰면 도착지는 진입점이 아니라 암호문입니다. 남은 blocker는 `0x450`이 아니라 Function `0x0e` 하나로 좁혀졌습니다. `0x450`은 통과하는 값을 이미 알고 있습니다.

*Verdict — jumping straight to the entry point is not possible. This is not a gate standing in front of plaintext game code: `.text` is ciphertext throughout the file so there is no plaintext entry point to jump to, the protection rewrites `.text` at runtime so file and process bytes differ at the same address, and per [Task 131](20260901-131-hardlock-bypass-stub.md)'s causality experiment the Function `0x0e` output determines that result and the downstream execution path. Hardlock is therefore the key source that decrypts the game rather than a checkpoint in front of it, and skipping it arrives at ciphertext rather than at an entry point. The remaining blocker has narrowed to Function `0x0e` alone, since a passing `0x450` value is already known.*

## 검증

- 대조 실행 15회, 모두 bounded launcher 실행이며 child가 스스로 종료했습니다.
- 기준값 2회 실행이 동일한 횟수와 종료 코드를 기록해 결정성을 확인했습니다.
- 코드 변경 없이 기존 옵션만 사용했습니다.

*Verification: fifteen controlled runs, all bounded launcher runs whose children exited on their own; two baseline runs recorded identical counts and exit codes, confirming determinism; and no code was changed, only existing options used.*
