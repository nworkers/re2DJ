# 20260904-180 EZ2DJ 4th 자발적 종료 지점 관측 결과
# 20260904-180 EZ2DJ 4th Deliberate Exit Source Observation Results

## 1. 개요 (Overview)

Task 179가 남긴 질문 — 무엇이 `ExitProcess`를 부르는가 — 에 답했다.

**결론: 종료를 부르는 것은 게임 코드가 아니라 보호 스텁이다. 호출 지점은 `RVA 0x006ed264`의 `call dword [0x00aee6cc]`이고, 이 RVA는 패커가 소유한 `.protect` 구간에 있다. 이번 실행의 종료 코드는 `0xffffffff`였다.**

**부수 확인: 관측 wrapper를 단 실행은 이전보다 더 멀리 갔다. surface 1,484개를 만들고 `System/Title/Press.str`, `ez2catch/title/title-c.str`, `scratchmix/title/title-s.str`까지 적재해 타이틀·모드 선택 화면 자원 단계에 도달한다. 종료 직전 기록은 Hardlock descriptor 요청 두 건이다.**

> **정정 (Task 181).** 이 문서는 처음에 `answered=0`을 "응답되지 않음"으로 읽었다. 그 필드는 `result.handshake_answered`로 handshake 전용이며, descriptor는 `status_cleared`와 `tail`, transform은 `mapped`와 `unmapped`로 결과를 보고한다. 세 요청 모두 `outcome=completed`이고 각각 256, 264바이트를 쓴다. 아래 3.2절은 정정된 내용이다.

The exit is called by the protection stub, not by game code: the call site is `call dword [0x00aee6cc]` at `RVA 0x006ed264`, inside the packer-owned `.protect` region, and this run exited with `0xffffffff`. The instrumented run also reached further than before, loading title and mode-select resources after 1,484 surfaces, and the records immediately before the exit are two Hardlock descriptor requests.

> **Correction (Task 181).** This log first read `answered=0` as "unanswered". That field is `result.handshake_answered` and is handshake-specific; descriptor requests report through `status_cleared` and `tail`, transforms through `mapped` and `unmapped`. All three complete, writing 256 and 264 bytes. Section 3.2 below is corrected.

---

## 2. 변경 내용 (Changes Implemented)

`src/platform/windows/injected_runtime.cpp`만 변경했다. 새 CLI 옵션은 없다.

1. **관측 wrapper.** `Re2djHleExitProcess`가 종료 코드, 호출자 반환 주소, 이미지 base 기준 RVA, 반환 주소 앞뒤 바이트 창을 `re2dj:vfs:exit-process` 항목으로 남긴 뒤 실제 `ExitProcess`를 부른다. 동작은 바꾸지 않는다.
2. **기록 함수.** `ReportExitProcess`는 예산 제한을 두지 않는다. 프로세스당 한 번만 발생하므로 예산에 걸려 유실되면 관측 자체가 무의미해진다.
3. **동적 resolver 연결.** `ExitProcess` 이름에 이 wrapper를 돌려주고, resolver 기록에는 `route=observe`로 남긴다.

---

## 3. 검증 결과 (Verification Results)

- `scripts/build_win32.bat`: 빌드 성공, 경고와 에러 0.
- `re2dj_unit_tests.exe`: 1,265 checks, 0 failures.
- `re2dj_windows_product_loader_probe.exe`: 전체 통과.
- 진입 추적: `logs/windows_x86_launcher_probe/ez2dj4th/20260904-200011-427`.

### 3.1 종료 호출 지점 (확인됨)

```
re2dj:vfs:exit-process:code=4294967295:caller=0x00aed26a:image_base=0x00400000:
  caller_rva=0x006ed26a:in_image=1:window_base=0x00aed252:readable=1:
  bytes=ffd89966c1e0206889a6ae00c3c709668bd2ff15cce6ae000f8874fcffff7904
```

반환 주소 바로 앞 6바이트가 `ff 15 cc e6 ae 00`이다. `call dword [0x00aee6cc]`이며 `0x00aed264 + 6 = 0x00aed26a`로 반환 주소와 정확히 맞는다.

- **확인됨 — 정적 IAT 경로로는 잡을 수 없다.** 이 실행 파일은 `ExitProcess`를 `GetProcAddress`로 해석하므로 런처의 `--break-exit-process`가 심는 정적 slot은 존재하지 않는다. 동적 resolver 자리에서만 관측된다.
- **확인됨 — 호출자는 `.protect` 구간이다.** `RVA 0x006ed264`는 진입점 `RVA 0x006e0240`과 같은 구간이며, 이전 실행들의 fault 진단이 그 주소를 `.protect`로 분류했다.
- **확인됨 — 호출은 스텁 자신의 import slot을 거친다.** `0x00aee6cc`(`RVA 0x006ee6cc`)는 패커가 채우는 slot이며, 그 값이 우리 wrapper로 바뀌었기 때문에 관측되었다.
- **확인됨 — 이번 종료 코드는 `0xffffffff`다.** Task 179 실행의 `1`과 다르다. 두 실행이 서로 다른 경로로 끝났다는 뜻이다.
- **추정 — 종료는 보호 검사의 결정이다.** 호출자가 보호 스텁 안이고 직전 기록이 Hardlock 요청이다. 스텁 내부의 어느 검사가 이 분기를 만드는지는 아직 읽지 않았다.

### 3.2 Hardlock 요청의 처리 상태 (확인됨, Task 181에서 정정)

한 실행에서 관측된 요청 분포다.

| 요청 | 횟수 | outcome | 쓴 바이트 | 결과 플래그 |
| - | - | - | - | - |
| `handshake` | 4 | completed | 6 | `handshake_answered=1` |
| `initialize` | 1 | completed | 0 | 없음 |
| `transform` | 36 | completed | 264 | `mapped=1` |
| `descriptor` | 103 | completed | 256 | `status_cleared=1` |

IOCTL 코드로는 `0x9c40244c` 82회, `0x9c402458` 36회, `0x9c402450` 4회, `0x9c402468` 1회다.

- **확인됨 — 네 종류 모두 `outcome=completed`다.** 거절된 요청은 없다.
- **확인됨 — 로그의 `answered` 필드는 handshake 전용이다.** `result.handshake_answered`를 그대로 찍은 값이므로 다른 종류에서 0인 것은 정상이며 미응답을 뜻하지 않는다.
- **확인됨 — Hardlock 응답이 진행을 막고 있지 않다.** 게스트는 이 144건을 모두 지나 타이틀·모드 선택 자원까지 진행했다.

### 3.3 실행이 타이틀 단계까지 간다 (확인됨)

| 지표 | Task 179 실행 | 이번 실행 |
| - | - | - |
| 디버그 이벤트 | 156,565 | 161,553 |
| `CreateSurface` | 1,108 | **1,484** |
| 마지막 자원 | `BG/aquaris/*.str` | `System/Title/Press.str`, `ez2catch/title/title-c.str`, `scratchmix/title/title-s.str` |
| 종료 코드 | `1` | `0xffffffff` |

- **확인됨 — 접근 위반은 없다.** 이번에도 크래시 없이 스스로 종료한다.
- **미확정 — 두 실행의 종료 코드가 다른 이유.** 보호 검사가 시점에 따라 다른 분기를 타는 것으로 보이지만 확인하지 않았다.

---

## 4. 가설 판정 (Hypothesis Verdicts)

| 가설 | 판정 |
| - | - |
| 호출자가 Hardlock 검사 경로 안에 있다 | **확인.** `.protect` 스텁이다 |
| 호출자가 그래픽·사운드 초기화 실패 경로다 | **반증** |
| 호출자가 정상 종료 경로다 | **반증.** 게임 코드가 아니다 |
| wrapper가 불리지 않는다 | **반증.** 정확히 한 번 기록되었다 |

---

## 5. 다음 작업 (Next Task)

정정 후의 판단은 다르다. Hardlock 요청 144건이 모두 완료 처리되고 게스트는 그 뒤로도 계속 진행했으므로, Hardlock을 더 파고들 근거가 없다. 대신 나중에 실제로 Hardlock 때문에 종료가 일어날 때 그것을 즉시 가릴 수 있는 증거만 남기고 이 방향을 닫는다. [Task 181](20260904-181-hardlock-exit-attribution-log.md)이 그 작업이다.

With the correction, the conclusion changes: all 144 Hardlock requests completed and the guest kept going, so there is no basis to dig further. Task 181 instead leaves the evidence needed to attribute a future exit to Hardlock and closes this direction.

---

## 6. 관련 문서 (Related Documents)

- [Task 180 설계](../design/20260904-180-ez2dj4th-exit-process-source.md)
- [Task 180 작업 지시서](../work-orders/20260904-180-ez2dj4th-exit-process-source.md)
- [Task 179 작업 로그](../work-logs/20260904-179-direct3d7-vertex-buffer-facade.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)
