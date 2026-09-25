# 작업 370 작업 지시서 — 게스트 API 호출 기록 / Task 370 work order — Guest API call log

설계: [20260925-370-guest-api-call-log.md](../design/20260925-370-guest-api-call-log.md)

## 절차 / Steps

1. `hle::RecordingImportCallServices`, `ApiCallRecord`, `FormatApiCallEvents`를 추가한다.
   *Add `hle::RecordingImportCallServices`, `ApiCallRecord`, and `FormatApiCallEvents`.*
2. `re2dj::logging::GetApiLogger()`와 `*.api.log` 파일을 추가하고, 주 로그에 그 경로를 남긴다.
   *Add `re2dj::logging::GetApiLogger()` and the `*.api.log` file, logging its path in the main log.*
3. Linux 진단은 facade 호출마다 장식자로 dispatch한다. continuation은 호출마다 API log에 쓴다. `DeviceIoControl` 내용은 숨긴다. Linux backend는 `re2dj_logging`에 link한다.
   *The Linux diagnostic dispatches every facade call through the decorator, continuation writes each call to the API log with `DeviceIoControl` contents withheld, and the Linux backend links `re2dj_logging`.*
4. 단위 테스트.
   *Unit tests.*

## 완료 조건 / Done when

- Linux 두 폭과 Windows x86 build·CTest가 통과하고, 기존 진단·probe와 실제 4th의 정지 지점이 그대로다.
  *Both Linux widths and Windows x86 build and pass CTest, with existing diagnostics, probes, and the real 4th's stop unchanged.*
- 실제 4th 실행의 `*.api.log`에 모든 호출의 입력·출력·last error가 남는다. Hardlock buffer는 길이만 남는다.
  *A real-4th run's `*.api.log` holds every call's inputs, outputs, and last error, with Hardlock buffers reduced to lengths.*
