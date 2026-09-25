# 작업 370 설계 — 게스트 API 호출 기록 / Task 370 design — Guest API call log

선행: [작업 365 설계](20260925-365-host-run-output-spdlog.md)

## 배경 / Background

Linux 실행의 호출 기록은 이름, 인자 DWORD, 문자열 인자 일부, EAX만 남는다. 게다가 처음 128개와 마지막 128개만 남는다. 그래서 다음 사실을 기록에서 볼 수 없다.

*The Linux run's call record keeps only the name, the argument DWORDs, some string arguments, and EAX, and only for the first and last 128 calls, so the record does not show:*

- handler가 게스트 메모리에서 **읽은 입력**. 예: `GetVersionExA`의 size field, `SystemTimeToFileTime`의 SYSTEMTIME.
  *what a handler **read** from guest memory as input, such as `GetVersionExA`'s size field or `SystemTimeToFileTime`'s SYSTEMTIME;*
- 게스트에게 **돌려준 출력**. 예: 채운 구조체, 문자열, 반환 slot.
  *the **output** it gave back — filled structures, strings, return slots;*
- last error의 변화. / *changes to the last error;*
- handler가 실패한 이유. / *why a handler failed.*

사용자는 우리 API가 받은 입력과 돌려준 응답을 로그로 확인하기를 원한다.

*The user wants the log to show the inputs our APIs received and the responses they returned.*

## 결정 / Decisions

1. **기록 장식자.** handler는 게스트 메모리와 last error를 `ImportCallServices`로만 다룬다. 그래서 공용 `hle::RecordingImportCallServices`로 실제 서비스를 감싼다. 이 장식자는 모든 호출을 그대로 넘기면서 다음을 `ApiCallRecord`에 적는다.
   ***Recording decorator:** handlers reach guest memory and the last error only through `ImportCallServices`, so a shared `hle::RecordingImportCallServices` wraps the real services, forwarding every call and noting in an `ApiCallRecord`:*
   - `ReadGuestString`(주소와 문자열), `ReadGuestBytes`, `WriteGuestBytes`(주소, 길이, 앞 64 byte). / *`ReadGuestString` (address and text), `ReadGuestBytes`, and `WriteGuestBytes` (address, length, the first 64 bytes);*
   - `SetLastError` 값. / *`SetLastError` values.*

   handler는 고치지 않는다. 앞으로 추가할 export도 자동으로 기록된다.
   *No handler changes, and every future export is recorded automatically.*
2. **Linux 연결.** `NativeKernel32Diagnostic::Dispatch`가 facade 호출마다 장식자로 감싸서 dispatch한다. 기록과 handler 실패 문구를 남긴다. last error의 변화는 기록 안의 `SetLastError` 항목으로 보인다. continuation은 호출마다 호출 번호·이름·호출 위치·인자, 기록 세부, 결과(EAX/EDX와 호출 뒤 last error, 또는 `UNHANDLED`와 이유)를 API logger에 쓴다.
   ***Linux wiring:** `NativeKernel32Diagnostic::Dispatch` dispatches each facade call through the decorator, keeping the record and any handler failure text; last-error changes show as the record's `SetLastError` entries. Continuation writes each call to the API logger: sequence, name, call site, arguments, the record's details, and the outcome (EAX/EDX and the resulting last error, or `UNHANDLED` with its reason).*
3. **API logger.** `re2dj::logging::GetApiLogger()`는 주 로그 옆의 `logs/re2dj-<시각>.api.log`에 쓴다. 기록은 모든 호출을 남기고, 호출 하나에 여러 줄이다. 그래서 console에는 내보내지 않는다. 주 로그에는 이 파일의 경로를 남긴다. logging을 초기화하지 않은 probe에서는 null이라 기록하지 않는다.
   ***API logger:** `re2dj::logging::GetApiLogger()` writes `logs/re2dj-<time>.api.log` next to the main log, every call with several lines each, and not to the console; the main log records its path. Probes that never initialize logging get null and record nothing.*
4. **숨기는 값.** `DeviceIoControl`의 buffer 내용은 사용자 Hardlock 재료에서 나온 handshake·descriptor다. 그래서 길이만 남기고 byte는 `[withheld]`로 적는다. 저장소 규칙상 재료 값은 로그에 남기지 않는다.
   ***Withheld values:** `DeviceIoControl` buffer contents are handshakes and descriptors derived from the user's Hardlock material, so only their lengths are logged, with the bytes shown as `[withheld]`, as the repository never logs material values.*

5. **읽기 쉬운 형식.** byte는 메모리 순서로 4개씩 묶어 적는다. 앞 64 byte만 적고 나머지는 `... (+N)`으로 센다. 전체를 담은, NUL로 끝나는 ASCII 문자열은 따옴표 친 글자로 적는다. `"`와 `\`는 backslash로, 나머지 비ASCII byte는 `\xNN`으로 적는다. continuation이 호출 요약용으로 읽는 문자열 인자(`GetModuleHandleA`, `LoadLibraryA`, `GetEnvironmentVariableA` 등)는 handler가 읽지 않더라도 `arg` 줄로 남긴다.
   ***Readable form:** bytes in memory order, grouped by four, the first 64 shown and the rest counted as `... (+N)`; a whole NUL-terminated ASCII string appears as quoted text, with `"` and `\` backslash-escaped and other non-ASCII bytes as `\xNN`. String arguments continuation reads for its call summary (`GetModuleHandleA`, `LoadLibraryA`, `GetEnvironmentVariableA`, and so on) appear as `arg` lines even when the handler never reads them.*

실제 4th에서 나온 예는 다음과 같다.

*Examples from the real 4th:*

```text
#0026 kernel32.dll!GetEnvironmentVariableA ret=00afb0a5 args=(00b12278, 00b12288, 00000058)
      arg   "HL_SEARCH"
      last_error <- 203
      -> eax=00000000 edx=00000000 last_error=203
#0037 kernel32.dll!GetVersionExA ret=00afa975 args=(f782ba78)
      read  f782ba78 [4] 9c000000
      write f782ba78 [156] 9c000000 06000000 02000000 f0230000 02000000 00000000 ... (+92)
      -> eax=00000001 edx=00000000 last_error=0
#0053 kernel32.dll!DeviceIoControl ret=00afa45d args=(00001008, 9c402450, f782bb5c, 00000006, f782bb5c, 00000006, f782bb58, 00000000)
      read  f782bb5c [6] [withheld]
      write f782bb5c [6] [withheld]
      write f782bb58 [4] [withheld]
#1613 kernel32.dll!GetModuleFileNameA ret=004c9498 args=(00000000, 00ace674, 00000104)
      write 00ace674 [19] "D:\\ez2dj\\EZ2DJ.EXE" + NUL
      last_error <- 0
      -> eax=00000012 edx=00000000 last_error=0
#1700 winmm.dll!timeBeginPeriod ret=00406cdc args=(00000001)
      -> UNHANDLED: winmm.dll!timeBeginPeriod is resolvable but not implemented
```

게임 loop가 돌면 frame마다 호출이 생겨 파일이 커질 것이다. 상한은 그때 실제 크기를 보고 정한다.

*Once the game loop runs, per-frame calls will grow the file; its bound will be set then, from the observed size.*

## 범위 밖 / Out of scope

- Windows 경로의 같은 기록(injected runtime은 실제 API를 쓴다). / *The same record on the Windows path, where the injected runtime uses the real APIs.*
- console 호출 요약(처음·마지막 128개)은 그대로 둔다. / *The console call summary (first and last 128) stays as it is.*
