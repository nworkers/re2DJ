# 20260904-181 Hardlock 종료 귀속 로그 결과
# 20260904-181 Hardlock Exit Attribution Log Results

## 1. 개요 (Overview)

나중에 Hardlock 때문에 종료가 일어날 때 그것을 가릴 수 있는 로그를 남기고 Hardlock 방향의 조사를 닫았다.

**결론: 이 게스트의 종료는 사용자 모드 wrapper로 관측할 수 없다. `ExitProcess`와 `TerminateProcess`에 관측 wrapper를 달아 게스트에게 전달했는데도 하나도 불리지 않았고, `DLL_PROCESS_DETACH`도 실행되지 않았다. 그래서 종료 훅에 의존하지 않는 방식으로 바꿨다. Hardlock 요청 줄과 런처의 종료 진단 양쪽에 `GetTickCount64` 값을 넣어, 어떤 경로로 죽든 마지막 Hardlock 요청과 종료 사이 간격을 밀리초로 얻는다.**

**첫 측정값: 마지막 요청 `tick_ms=381776031`, 종료 `tick_ms=381779937` — 간격 3,906ms. 같은 실행에서 요청 간 간격이 94ms에서 28,016ms까지 흩어져 있으므로, 이 3.9초는 요청 직후가 아니다. 이번 종료는 Hardlock 요청에 곧바로 뒤따른 것이 아니다.**

The guest's exit cannot be observed from user mode: wrappers on both `ExitProcess` and `TerminateProcess` were handed to the guest and neither fired, and `DLL_PROCESS_DETACH` did not run either. The approach therefore stopped depending on an exit hook: a monotonic tick now sits on both the Hardlock request line and the launcher's exit diagnostic, giving the interval between the two however the process dies. The first measurement is 3,906 ms, against request spacing that ranges from 94 ms to 28,016 ms in the same run, so this exit did not immediately follow a Hardlock request.

---

## 2. 변경 내용 (Changes Implemented)

`src/platform/windows/injected_runtime.cpp`

1. **Hardlock 누적 상태.** `CompleteHardlockRequest`가 종류별 횟수, 거절 횟수, 마지막 요청의 종류·outcome·바이트 수·tick을 누적한다.
2. **요청 줄에 tick.** `hardlock-device` 줄에 `tick_ms`를 추가했다. 이것이 종료 경로와 무관한 귀속의 핵심이다.
3. **필드 이름 정정.** 같은 줄의 `answered`를 `handshake_answered`로 바꿨다. Task 180이 이 값을 "미응답"으로 잘못 읽었고, 이름이 그 오독을 부른다.
4. **종료 관측 wrapper.** `Re2djHleExitProcess`와 `Re2djHleTerminateProcess`가 종료 코드, 호출자와 RVA, 앞뒤 바이트 창, Hardlock 요약을 남긴 뒤 원래 동작을 그대로 수행한다. `route` 필드로 경로를 구분한다.
5. **detach 대비.** wrapper가 불리지 않은 채 프로세스가 내려가면 `DLL_PROCESS_DETACH`에서 같은 요약을 남긴다. loader lock 아래이므로 두 줄로 제한한다.

`src/tools/windows_x86_launcher_probe/main.cpp`

6. **종료 진단에 tick.** `exit_process` 디버그 이벤트 기록에 `tick_ms`를 추가했다. `GetTickCount64`는 시스템 전역이라 자식이 남긴 값과 직접 비교된다.

동작은 어디서도 바뀌지 않는다. 누적 값과 tick은 기록에만 쓰인다.

---

## 3. 검증 결과 (Verification Results)

- `scripts/build_win32.bat`: 빌드 성공, 경고와 에러 0.
- `re2dj_unit_tests.exe`: 1,265 checks, 0 failures.
- `re2dj_windows_product_loader_probe.exe`: 전체 통과.
- 진단 실행 6회: `20260904-202015-872`, `-202535-750`, `-203242-443`, `-204408-589`, `-204537-984`, `-204959-444`, `-205408-549`.

### 3.1 사용자 모드 종료 훅은 이 게스트에 닿지 않는다 (확인됨)

`20260904-204959-444`에서 두 wrapper가 모두 게스트에게 전달되었다.

```
dynamic-resolver:name=ExitProcess:route=observe:address=0x631c3828:caller=0x00aebf18
dynamic-resolver:name=TerminateProcess:route=observe:address=0x631c178f:caller=0x00aed61a
dynamic-resolver:name=ExitProcess:route=observe:address=0x631c3828:caller=0x00aed61a
```

그럼에도 그 실행은 `exit-process`, `exit-detach` 어느 것도 남기지 않고 코드 `1`로 끝났다.

- **확인됨 — 두 wrapper 모두 불리지 않았다.** resolver가 우리 주소를 돌려줬는데도 기록이 없다.
- **확인됨 — `DLL_PROCESS_DETACH`도 실행되지 않았다.** 같은 실행에 detach 기록이 없다.
- **확인됨 — 스레드 11개가 모두 코드 `1`로 종료된 뒤 프로세스가 끝났다.** 프로세스 전체를 한 번에 내리는 종료의 모습이다.
- **추정 — 종료는 kernel32 아래에서 일어난다.** 사용자 모드 wrapper가 모두 비켜간 것과 detach 미실행이 함께 관측되는 경로는 ntdll 수준의 강제 종료다. 그 함수를 직접 확인하지는 않았다.

### 3.2 tick 짝짓기는 경로와 무관하게 동작한다 (확인됨)

`20260904-205408-549`의 두 기록이다.

```
hardlock-device:request=descriptor:outcome=completed:bytes=256:...:tick_ms=381776031
{"debug_event":"exit_process","code":"0x00000001","tick_ms":381779937}
```

- **확인됨 — 간격은 3,906ms다.** 종료 훅이 하나도 불리지 않은 실행에서도 값을 얻었다.
- **확인됨 — 요청 간 간격은 일정하지 않다.** 같은 실행의 마지막 다섯 구간이 0, 28,016, 94, 23,344, 4,406ms다.
- **판정 — 이번 종료는 Hardlock 요청 직후가 아니다.** 3.9초는 이 실행의 정상 요청 간격 범위 안에 있으므로, 종료가 특정 요청에 뒤따랐다고 볼 근거가 없다.

### 3.3 Hardlock 요청은 계속 모두 완료된다 (확인됨)

여섯 실행 모두에서 네 종류가 `outcome=completed`이고 거절은 0건이다. 한 실행 기준 `descriptor` 52–96회, `transform` 36회, `handshake` 4회, `initialize` 1회다.

### 3.4 검증하지 못한 것 (미확정)

- **미확정 — `exit-process`와 `exit-detach` 기록의 실제 출력.** Task 180의 wrapper 기록은 관측되었지만, 이번에 추가한 Hardlock 요약 줄과 detach 줄은 그것을 내보내는 종료가 이번 실행들에서 한 번도 일어나지 않아 확인하지 못했다. 관측된 종료는 모두 사용자 모드를 비켜갔고, 나머지 실행은 진단 이벤트 상한에서 런처가 `TerminateProcess`로 끝냈다. `TerminateProcess`는 `DLL_PROCESS_DETACH`를 건너뛰므로 그 실행에서는 기록이 남을 수 없다.
- 이 미검증 부분은 tick 짝짓기가 대체하므로 귀속 자체는 성립한다.

---

## 4. Hardlock 검토 종료 (Closing The Hardlock Line)

닫는 근거다.

| 근거 | 관측 |
| - | - |
| 요청이 모두 처리된다 | 여섯 실행에서 `outcome=completed`, 거절 0건 |
| 응답이 진행을 막지 않는다 | 요청 90여 건을 지나 타이틀·모드 선택 자원까지 적재 |
| 종료가 요청에 붙어 있지 않다 | 마지막 요청과 종료 사이 3,906ms, 정상 요청 간격 범위 안 |
| 종료 코드가 실행마다 다르다 | `1`과 `0xffffffff` |

다시 열 조건도 명확하다. `exit_process`의 `tick_ms`와 마지막 `hardlock-device`의 `tick_ms` 차이가 작고, 그 요청의 `outcome`이 `completed`가 아니면 그때 다시 본다.

The Hardlock direction is closed on four grounds — every request completes, answers do not gate progress, the exit is not adjacent to a request, and the exit code varies — with an explicit condition for reopening it: a small tick difference together with a non-completed last request.

---

## 5. 다음 작업 (Next Task)

Hardlock이 아닌 방향으로 종료 원인을 본다. 종료가 kernel32 아래에서 일어나므로 사용자 모드 훅은 더 늘려도 소용이 없다. 남은 관측 수단은 런처 쪽이다. 종료 직전 게스트 스레드들의 마지막 실행 지점을 남기면 어느 코드가 종료를 결정했는지 좁힐 수 있다.

Look at the exit from a direction other than Hardlock. Since it happens below kernel32, more user-mode hooks will not help; the remaining lever is on the launcher side, where recording where the guest threads last executed before the exit would narrow down what decided it.

---

## 6. 관련 문서 (Related Documents)

- [Task 181 설계](../design/20260904-181-hardlock-exit-attribution-log.md)
- [Task 181 작업 지시서](../work-orders/20260904-181-hardlock-exit-attribution-log.md)
- [Task 180 작업 로그](20260904-180-ez2dj4th-exit-process-source.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)
