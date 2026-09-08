# 작업 로그: 예외 코드별 크래시 포렌식

## 한국어

### 관련 문서

- 설계: [예외 코드별 크래시 포렌식 설계](../design/20260908-232-crash-forensics-per-code.md)
- 작업 지시: [예외 코드별 크래시 포렌식](../work-orders/20260908-232-crash-forensics-per-code.md)
- 선행 사례: [4th StyleSelect divide-by-zero 진단](20260905-190-ez2dj4th-styleselect-divzero.md)

### 배경

사용자가 세 가지 문제를 보고했습니다.

1. Warning에서 로고로 넘어갈 때 비정상적인 깜빡임
2. StreetMix 선택 시 화면이 유지되지 않고 검은 화면
3. StreetMix 선택 시 게임 비정상 종료

### 재현

입력을 넣을 수 있는 스크립트를 만들어 재현했습니다. `config/ez2dj-io.example.ini`를 `--io-config`로 넘기고, `keybd_event`로 coin(`F5`)과 p1 start(`1`)를 보내고, 창의 client rect를 DPI 인식 상태로 캡처합니다.

- 타이틀 → coin 3회 → start → LEVEL SELECT 도달. 이 화면은 원본대로 렌더링됩니다(Club Mix / STREET MIX / RADIO MIX, `4 STAGES NORMAL`, 카운트다운).
- LEVEL SELECT 타이머가 만료되어 StreetMix로 진입하는 순간 프로세스가 종료됩니다.
- `runtime_detached_exit`는 `0xc0000094`(`STATUS_INTEGER_DIVIDE_BY_ZERO`)입니다.

문제 2와 3은 같은 사건으로 보입니다. 화면이 검게 되는 것은 프로세스가 죽어 present가 멈춘 결과입니다.

### 확인된 원인 — 포렌식이 남지 않던 이유

`ReportCrashException`은 실행 전체에서 첫 예외 하나만 기록했습니다. 1st SE의 `.protect` 보호 계층은 진입 직후 anti-debug 목적으로 `0xC0000005`를 한 번 일으키고 자체 SEH로 처리합니다. 그 무해한 예외가 유일한 기록 자리를 소모해, 실제로 프로세스를 끝내는 `0xC0000094`가 기록되지 않았습니다.

### 코드 변경

기록 자리를 예외 코드별로 하나씩 두고 서로 다른 코드를 최대 8개까지 받도록 했습니다. 같은 코드가 반복되면 여전히 한 번만 기록하므로, 로그를 예외로 채우지 않는다는 원래 의도는 유지됩니다.

### 검증 — 포렌식 확보

같은 재현 실행에서 두 기록이 모두 남습니다.

```
crash-exception:code=0xc0000005:address=0x01ee5fa5   (진입부 anti-debug, 기존과 동일)
crash-exception:code=0xc0000094:address=0x0042d1f9   (신규)
```

`0xC0000094` 기록의 내용은 다음과 같습니다.

| 항목 | 값 |
| --- | --- |
| address / RVA | `0x0042d1f9` / `0x0002d1f9` |
| eax | `0x00000400` (1024) |
| edx | `0x00000000` |
| ecx | `0x0163b9bc` |
| bytes at EIP | `f7 b9 90 74 01 00` = `idiv dword ptr [ecx+0x17490]` |

### 확인된 사실 — 충돌 지점 코드

포렌식의 코드 창으로 충돌 함수의 해당 구간을 읽었습니다.

```
0042d1cb  mov eax,[ebp-4]                       ; this
0042d1ce  cmp dword ptr [eax],3
0042d1d1  jge 0042d1e3
0042d1d3  mov ecx,[ebp-4]
0042d1d6  mov edx,[ebp-4]
0042d1d9  mov eax,[edx]                         ; n = this->[0]
0042d1db  mov [ecx+0x17490],eax                 ; divisor = n
0042d1e1  jmp 0042d1f0
0042d1e3  mov ecx,[ebp-4]
0042d1e6  mov dword ptr [ecx+0x17490],3         ; divisor = 3
0042d1f0  mov ecx,[ebp-4]
0042d1f3  mov eax,0x400                         ; 1024
0042d1f8  cdq
0042d1f9  idiv dword ptr [ecx+0x17490]          ; 1024 / divisor
0042d1ff  mov edx,[ebp-4]
0042d202  mov [edx+0x1749c],eax                 ; 몫 저장
          ... [+0x174a0], [+0x174a8], [+0x174b0], [+0x174b4]를 0으로 초기화
```

**확인됨.** 제수는 `this->[0]`을 3으로 클램프한 값입니다. `this->[0]`이 0이면 제수가 0이 되어 충돌합니다. 크래시 시점의 `this`는 `0x0163b9bc`이고, 뒤이어 여러 필드를 0으로 초기화하는 형태로 보아 UI 요소의 초기화 함수로 보입니다.

**미확정.** `this->[0]`의 출처. 4th의 같은 증상은 `FindFirstFileA` 미후킹으로 자산 파일 개수가 0이 된 것이었지만, 이번 실행의 열거 호출은 `System/Title`에 대한 `*.*` 한 건뿐이고 `matches=31`로 성공했습니다. 따라서 4th와 같은 경로는 아닙니다.

### 배제한 것

충돌 직전 `System\LevelSelect\T_GrayClubMix.bmp`가 `stage=native`로 열려 CHD 조회 실패처럼 보였습니다. 확인 결과 그 파일은 CHD에 있고(196,664 바이트) `LoadImageA`로 정상 적재됩니다. `stage=native`는 이미 staging에 materialize된 파일에 대한 존재 확인이므로 결함이 아닙니다.

`streetmix_eyecatch.wav`는 탐색 경로 4곳에서 모두 실패하지만, CHD의 `System/MusicSelect`, `System/MusicSelect/disc`, `System/Common` 어디에도 없습니다. 원본에 없는 파일이므로 정상적인 탐색 실패입니다.

### 검증 — 시험과 회귀

- `re2dj_unit_tests.exe` → `checks: 1421, failures: 0`
- `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only` → exit 0
- 3rd·4th를 35초 제한으로 실행. 두 경우 모두 정상 실행 중입니다.

### 남은 과제

- `this->[0]`을 0으로 만드는 입력 확인. 다음 단계는 복호화된 `.text`에서 오프셋 `0x17490`과 `0x1749c`를 참조하는 코드를 찾아 이 객체의 생성 지점을 역추적하는 것입니다. 4th에서 같은 방법으로 생성자를 찾았습니다.
- 보고된 문제 1(Warning→로고 깜빡임)은 아직 진단하지 않았습니다. ddraw 추적에서 그리기 없이 Flip이 연속되는 구간이 보이지만, `Present`가 매번 다시 그리므로 그 자체가 원인은 아닙니다.

## English

### Related documents

- Design: [Per-Exception-Code Crash Forensics Design](../design/20260908-232-crash-forensics-per-code.md)
- Work order: [Per-Exception-Code Crash Forensics](../work-orders/20260908-232-crash-forensics-per-code.md)
- Precedent: [4th StyleSelect divide-by-zero diagnosis](20260905-190-ez2dj4th-styleselect-divzero.md)

### Background

The user reported three problems: an abnormal flicker on the Warning-to-logo transition, a black screen when StreetMix is selected, and an abnormal termination when StreetMix is selected.

### Reproduction

A scripted-input harness reproduces it: `config/ez2dj-io.example.ini` is passed through `--io-config`, `keybd_event` sends coin (`F5`) and p1 start (`1`), and the window's client rect is captured DPI-aware.

Title, three coins, and start reach LEVEL SELECT, which renders correctly with Club Mix, STREET MIX, RADIO MIX, `4 STAGES NORMAL`, and a countdown. When that timer expires into StreetMix the process ends, and `runtime_detached_exit` reports `0xc0000094` (`STATUS_INTEGER_DIVIDE_BY_ZERO`).

Problems two and three look like one event: the screen goes black because the process died and presentation stopped.

### Confirmed cause of the missing forensics

`ReportCrashException` recorded only the first exception of a run. The 1st SE `.protect` layer raises one `0xC0000005` right after entry as an anti-debug trick and handles it in its own SEH, consuming the only slot, so the `0xC0000094` that ends the process was never written.

### Code change

The recording slot is now per exception code, accepting at most eight distinct codes. A repeating code is still recorded once, so the original intent of not filling the log with exceptions is preserved.

### Verification — forensics obtained

The same reproduction now records both `crash-exception:code=0xc0000005:address=0x01ee5fa5` and the new `crash-exception:code=0xc0000094:address=0x0042d1f9`. The latter carries RVA `0x0002d1f9`, `eax=0x00000400` (1024), `edx=0`, `ecx=0x0163b9bc`, and code bytes `f7 b9 90 74 01 00`, which is `idiv dword ptr [ecx+0x17490]`.

### Confirmed — the code at the fault

The forensic code window shows the function clamping a count and dividing by it: it compares `[this]` against 3, sets `[this+0x17490]` to `[this]` when below and to 3 otherwise, then computes `1024 / [this+0x17490]`, stores the quotient at `[this+0x1749c]`, and zeroes several further fields at `+0x174a0`, `+0x174a8`, `+0x174b0`, and `+0x174b4`.

**Confirmed.** The divisor is `[this]` clamped to at most 3, so a `[this]` of zero divides by zero. `this` was `0x0163b9bc`, and the surrounding field initialisation suggests a UI element's setup routine.

**Unresolved.** Where `[this]` comes from. The same symptom in 4th was an asset file count of zero caused by `FindFirstFileA` not being hooked, but this run makes exactly one enumeration call — `*.*` on `System/Title`, which succeeded with `matches=31` — so it is not the same path.

### Ruled out

`System\LevelSelect\T_GrayClubMix.bmp` opening at `stage=native` just before the crash looked like a failed CHD lookup. The file is in the CHD at 196,664 bytes and loads correctly through `LoadImageA`; the `stage=native` line is an existence check against the already-materialised staging copy, not a defect.

`streetmix_eyecatch.wav` fails in all four search directories, but it is absent from `System/MusicSelect`, `System/MusicSelect/disc`, and `System/Common` in the CHD. It is not in the original, so the search failure is correct.

### Verification — tests and regression

`re2dj_unit_tests.exe` reported `checks: 1421, failures: 0`, `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only` exited 0, and 3rd and 4th were both still running under a 35-second bound.

### Remaining work

- Establish what drives `[this]` to zero. The next step is to scan the decrypted `.text` for code referencing offsets `0x17490` and `0x1749c` and trace back to where this object is constructed, the method that found the constructor for 4th.
- Reported problem one, the Warning-to-logo flicker, is not yet diagnosed. The ddraw trace shows a stretch of consecutive Flips with no drawing between them, but `Present` redraws every time, so that alone is not the cause.
